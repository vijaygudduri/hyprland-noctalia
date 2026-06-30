#include "shell/control_center/tabs/home_tab.h"

#include "config/config_service.h"
#include "core/build_info.h"
#include "core/deferred_call.h"
#include "core/keybind_matcher.h"
#include "core/log.h"
#include "cursor-shape-v1-client-protocol.h"
#include "dbus/accounts/accounts_service.h"
#include "i18n/i18n.h"
#include "notification/notifications.h"
#include "pipewire/pipewire_service.h"
#include "render/animation/animation_manager.h"
#include "render/scene/input_area.h"
#include "shell/control_center/shortcut_registry.h"
#include "shell/panel/panel_button_style.h"
#include "shell/panel/panel_manager.h"
#include "shell/profile/avatar_path.h"
#include "shell/wallpaper/wallpaper.h"
#include "system/brightness_service.h"
#include "system/distro_info.h"
#include "system/weather_service.h"
#include "time/time_format.h"
#include "ui/builders.h"
#include "ui/controls/grid_view.h"
#include "ui/controls/slider.h"
#include "ui/dialogs/file_dialog.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>

using namespace control_center;

namespace {

  constexpr Logger kLog("control-center");

  constexpr float kHomeAvatarScale = 2.15f;
  constexpr std::size_t kHomeShortcutGridColumns = 3;
  constexpr std::size_t kHomeStackedShortcutMax = 2;
  constexpr float kHomeShortcutSquareTrim = 0.82f;
  constexpr float kVolumeBrightnessLabelWidth = 3.2f;

  float homeAvatarSize(float scale) { return Style::controlHeightLg * kHomeAvatarScale * scale; }

  std::filesystem::path avatarStartDirectory(const AccountsService* accounts, const ConfigService* config) {
    const std::string currentPath =
        config != nullptr ? shell::resolvedAvatarPath(accounts, config->config()) : std::string{};
    const std::filesystem::path current(currentPath);
    std::error_code ec;
    if (!current.empty() && std::filesystem::exists(current, ec) && current.has_parent_path()) {
      return current.parent_path();
    }
    if (const char* home = std::getenv("HOME"); home != nullptr && home[0] != '\0') {
      return std::filesystem::path(home) / "Pictures";
    }
    return {};
  }

  // Time/date helpers – date format now includes full year
  std::string formatShellTime(const ConfigService* config) {
    const char* format = config != nullptr ? config->config().shell.timeFormat.c_str() : "{:%H:%M}";
    return formatLocalTime(format);
  }

  std::string formatShellDate() {
    // Requested format: "Tuesday, June 30, 2026"
    return formatLocalTime("%A, %B %d, %Y");
  }

  std::string formatPercent(float fraction) {
    return std::to_string(static_cast<int>(std::round(fraction * 100.0f))) + "%";
  }

  std::string userHostLine() { return std::format("{}@{}", sessionDisplayName(), hostName()); }

  std::string noctaliaVersionLine() { return std::format("Noctalia {}", noctalia::build_info::displayVersion()); }

  void applyShortcutButtonStyle(Button& button, bool enabled, bool active, float fillOpacity) {
    if (enabled && active) {
      // Keep the same fill color the default (inactive) palette uses, so the tile
      // background never changes — only the border/icon highlight differs when active.
      const Button::ButtonPalette basePalette = Button::defaultPalette(ButtonVariant::Default);
      auto backgroundOf = [](const Button::ButtonStateColors& colors) {
        auto [bg, border, content] = colors;
        return bg;
      };

      const Button::ButtonStateColors activeNormal{
          backgroundOf(basePalette.normal),
          colorSpecFromRole(ColorRole::Primary),
          colorSpecFromRole(ColorRole::Primary),
      };

      Button::ButtonPalette activePalette{
          .borderWidth = Style::borderWidth * 2.5f,
          .normal = activeNormal,
          .hover = activeNormal,  // hover disabled: same as normal
          .pressed = Button::ButtonStateColors{
              backgroundOf(basePalette.pressed),
              colorSpecFromRole(ColorRole::Primary),
              colorSpecFromRole(ColorRole::Primary),
          },
          .disabled = Button::ButtonStateColors{
              backgroundOf(basePalette.disabled),
              colorSpecFromRole(ColorRole::Primary, 0.55f),
              colorSpecFromRole(ColorRole::Primary, 0.55f),
          },
          .selected = Button::defaultPalette(ButtonVariant::Primary).selected,
      };
      button.setCustomPalette(activePalette);
    } else {
      Button::ButtonPalette inactivePalette = Button::defaultPalette(ButtonVariant::Default);
      inactivePalette.hover = inactivePalette.normal;  // hover disabled: same as normal
      button.setCustomPalette(inactivePalette);
    }
    button.setSurfaceOpacity(fillOpacity);
    button.setEnabled(enabled);
  }

  void applyHomeCardStyle(Flex& card, float scale, float fillOpacity, bool showBorder) {
    applySectionCardStyle(card, scale, fillOpacity, showBorder);
    card.setGap(Style::spaceSm * scale);
  }

  void applyHomeCardHover(Flex& card, bool hovered, bool baseBorders) {
    if (hovered || baseBorders) {
      card.setBorder(colorSpecFromRole(ColorRole::Outline), Style::borderWidth);
    } else {
      card.clearBorder();
    }
  }

  void applyAvatarChrome(Image* avatar) {
    if (avatar == nullptr) {
      return;
    }
    avatar->setBorder(colorSpecFromRole(ColorRole::Primary), Style::borderWidth * 3.0f);
  }

} // namespace

HomeTab::HomeTab(const ControlCenterServices& services)
    : m_audio(services.audio), m_brightness(services.brightness),
      m_config(services.config), m_accounts(services.accounts), m_wallpaper(services.wallpaper),
      m_thumbnails(services.thumbnails), m_services(services.shortcutServices()),
      m_weather(services.weather) {
  if (m_thumbnails != nullptr) {
    m_thumbnailPendingSub = m_thumbnails->subscribePendingUpload([this]() {
      if (m_wallpaperBg == nullptr) {
        return;
      }
      PanelManager::instance().requestUpdateOnly();
    });
  }

  if (m_wallpaper != nullptr) {
    m_wallpaperChangedConn = m_wallpaper->changed().connect([this]() {
      if (m_thumbnails != nullptr && m_loadedWallpaperSize > 0) {
        ensureWallpaperThumbnail(m_wallpaper->currentPath(), m_loadedWallpaperSize);
      }
      if (m_wallpaperBg != nullptr) {
        PanelManager::instance().requestUpdateOnly();
      }
    });
  }
}

HomeTab::~HomeTab() {
  if (m_thumbnails != nullptr && !m_loadedWallpaperPath.empty()) {
    m_thumbnails->release(m_loadedWallpaperPath, m_loadedWallpaperSize);
  }
}

std::unique_ptr<Flex> HomeTab::create() {
  const float scale = contentScale();

  auto tab = ui::column({
      .out = &m_rootLayout,
      .align = FlexAlign::Stretch,
      .gap = Style::spaceMd * scale,
  });

  // ── User card ──────────────────────────────────────────────────────────────
  auto userCard = ui::column({
      .out = &m_userCard,
      .justify = FlexJustify::Center,
      .minHeight = homeAvatarSize(scale) + Style::spaceSm * scale + 4.5f * scale,
      .fillWidth = true,
      .configure = [scale, opacity = panelCardOpacity(), borders = panelBordersEnabled()](Flex& card) {
        applyHomeCardStyle(card, scale, opacity, borders);
      },
  });

  {
    const float wallpaperRadius = std::max(0.0f, Style::scaledRadiusXl(scale) - Style::borderWidth);
    userCard->addChild(
        ui::image({
            .out = &m_wallpaperPlaceholder,
            .fit = ImageFit::Cover,
            .radius = wallpaperRadius,
            .participatesInLayout = false,
            .configure = [](Image& image) { image.setZIndex(-2); },
        })
    );
    userCard->addChild(
        ui::image({
            .out = &m_wallpaperBg,
            .fit = ImageFit::Cover,
            .radius = wallpaperRadius,
            .participatesInLayout = false,
            .configure = [](Image& image) {
              image.setZIndex(-1);
              image.setOpacity(0.0f);
            },
        })
    );
    userCard->addChild(
        ui::box({
            .out = &m_wallpaperGradient,
            .participatesInLayout = false,
            .configure = [](Box& box) { box.setZIndex(-1); },
        })
    );
  }

  const float avatarSize = homeAvatarSize(scale);
  const auto openAvatarPicker = [this]() {
    if (m_config == nullptr) {
      return;
    }
    FileDialogOptions options;
    options.mode = FileDialogMode::Open;
    options.defaultViewMode = FileDialogViewMode::Grid;
    options.title = i18n::tr("control-center.home.select-avatar");
    options.extensions = {".png", ".jpg", ".jpeg", ".webp", ".bmp", ".gif"};
    options.startDirectory = avatarStartDirectory(m_accounts, m_config);

    (void)FileDialog::open(std::move(options), [this](std::optional<std::filesystem::path> pickedPath) {
      if (!pickedPath.has_value() || m_config == nullptr) {
        return;
      }
      const auto applyResult = shell::applyAvatarPath(m_accounts, m_config, pickedPath->string());
      if (applyResult.success()) {
        m_loadedAvatarPath.clear();
        DeferredCall::callLater([]() {
          PanelManager::instance().refresh();
          PanelManager::instance().requestRedraw();
        });
        return;
      }
      notify::error(
          "Noctalia", i18n::tr("control-center.home.avatar-error-title"),
          i18n::tr(shell::avatarApplyErrorTranslationKey(applyResult.error))
      );
    });
  };

  auto avatarArea = std::make_unique<InputArea>();
  avatarArea->setSize(avatarSize, avatarSize);
  avatarArea->setHitShape(InputArea::HitShape::Circle);
  avatarArea->setFocusable(true);
  avatarArea->setCursorShape(WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_POINTER);
  avatarArea->setOnClick([openAvatarPicker](const InputArea::PointerData&) { openAvatarPicker(); });
  avatarArea->setOnKeyDown([openAvatarPicker](const InputArea::KeyData& key) {
    if (key.pressed && KeybindMatcher::matches(KeybindAction::Validate, key.sym, key.modifiers)) {
      openAvatarPicker();
    }
  });
  m_userAvatarArea = avatarArea.get();
  avatarArea->addChild(
      ui::image({
          .out = &m_userAvatar,
          .fit = ImageFit::Cover,
          .radius = avatarSize * 0.5f,
          .padding = 1.0f * scale,
          .width = avatarSize,
          .height = avatarSize,
          .configure = [](Image& image) {
            image.setBorder(colorSpecFromRole(ColorRole::Primary), Style::borderWidth * 3.0f);
            image.setHitTestVisible(false);
          },
      })
  );
  const auto syncAvatarChrome = [this]() {
    applyAvatarChrome(m_userAvatar);
    PanelManager::instance().requestRedraw();
  };
  avatarArea->setOnEnter([syncAvatarChrome](const InputArea::PointerData&) { syncAvatarChrome(); });
  avatarArea->setOnLeave([syncAvatarChrome]() { syncAvatarChrome(); });
  avatarArea->setOnFocusGain(syncAvatarChrome);
  avatarArea->setOnFocusLoss(syncAvatarChrome);

  const auto configureUserDetailLabel = [scale](Label& label) {
    label.setShadow(Color{0.0f, 0.0f, 0.0f, 0.7f}, 0.0f, 1.5f * scale);
  };
  auto userRow = ui::row(
      {.align = FlexAlign::Center, .gap = Style::spaceMd * scale, .padding = Style::spaceSm * scale,
       .fillWidth = true},
      std::move(avatarArea),
      ui::column(
          {.out = &m_userMain,
           .align = FlexAlign::Stretch,
           .justify = FlexJustify::Center,
           .gap = Style::spaceXs * 0.4f * scale,
           .minHeight = avatarSize,
           .width = 0.0f,
           .height = avatarSize,
           .flexGrow = 1.0f},
          ui::label({
              .text = sessionDisplayName(),
              .fontSize = Style::fontSizeBody * 1.05f * scale,
              .color = colorSpecFromRole(ColorRole::OnSurface),
              .fontWeight = FontWeight::Bold,
              .configure =
                  [scale](Label& label) { label.setShadow(Color{0.0f, 0.0f, 0.0f, 0.75f}, 0.0f, 1.5f * scale); },
          }),
          ui::label({
              .out = &m_userHost,
              .text = userHostLine(),
              .fontSize = Style::fontSizeMini * scale,
              .color = colorSpecFromRole(ColorRole::OnSurfaceVariant),
              .configure = configureUserDetailLabel,
          }),
          ui::label({
              .out = &m_userUptime,
              .text = "…",
              .fontSize = Style::fontSizeMini * scale,
              .color = colorSpecFromRole(ColorRole::OnSurfaceVariant),
              .configure = configureUserDetailLabel,
          }),
          ui::label({
              .out = &m_userVersion,
              .text = noctaliaVersionLine(),
              .fontSize = Style::fontSizeMini * scale,
              .color = colorSpecFromRole(ColorRole::OnSurfaceVariant),
              .configure = configureUserDetailLabel,
          })
      )
  );
  userCard->addChild(std::move(userRow));

  const auto openWallpaperPanel = []() { PanelManager::instance().togglePanel("wallpaper"); };
  m_userCardKeyboardArea =
      addCardOverlay(*m_userCard, openWallpaperPanel, {.keyboardFocus = true, .pointerHitTest = false});
  m_userCardArea = addCardOverlay(*m_userCard, openWallpaperPanel, {.keyboardFocus = false, .pointerHitTest = true});

  tab->addChild(std::move(userCard));

  // ── Slider card: volume + brightness ────────────────────────────────────
  const float labelMinWidth = Style::controlHeightLg * scale;

  auto sliderCard = ui::column({
      .align = FlexAlign::Stretch,
      .gap = Style::spaceSm * scale,
      .configure = [scale, opacity = panelCardOpacity(), borders = panelBordersEnabled()](Flex& card) {
        applySectionCardStyle(card, scale, opacity, borders);
      },
  });

  // Volume row
  {
    const AudioNode* sink = m_audio != nullptr ? m_audio->defaultSink() : nullptr;
    const float currentVolume = sink != nullptr ? std::clamp(sink->volume, 0.0f, 1.5f) : 0.0f;
    const bool muted = sink != nullptr && sink->muted;
    const std::string volGlyph = muted ? "volume-mute" : "volume-high";

    auto volRow = ui::row({.align = FlexAlign::Center, .gap = Style::spaceSm * scale});

    auto baseGlyph = ui::glyph({
        .out = &m_volumeGlyph,
        .glyph = volGlyph,
        .glyphSize = Style::fontSizeTitle * scale,
        .color = muted ? colorSpecFromRole(ColorRole::OnSurfaceVariant)
                       : colorSpecFromRole(ColorRole::OnSurface),
    });

    auto muteArea = std::make_unique<InputArea>();
    const float muteHitSize = Style::controlHeight * scale;
    muteArea->setSize(muteHitSize, muteHitSize);
    muteArea->setHitShape(InputArea::HitShape::Circle);
    muteArea->setFocusable(true);
    muteArea->setCursorShape(WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_POINTER);
    muteArea->setParticipatesInLayout(false);

    const float offsetDistance = (muteHitSize - Style::fontSizeTitle * scale) * 0.5f;
    muteArea->setPosition(-offsetDistance, -offsetDistance);

    const auto toggleMute = [this]() {
      const AudioNode* currentSink = m_audio != nullptr ? m_audio->defaultSink() : nullptr;
      if (m_audio == nullptr || currentSink == nullptr) {
        return;
      }
      m_audio->setSinkMuted(currentSink->id, !currentSink->muted);
      PanelManager::instance().refresh();
    };
    muteArea->setOnClick([toggleMute](const InputArea::PointerData&) { toggleMute(); });
    muteArea->setOnKeyDown([toggleMute](const InputArea::KeyData& key) {
      if (key.pressed && KeybindMatcher::matches(KeybindAction::Validate, key.sym, key.modifiers)) {
        toggleMute();
      }
    });

    baseGlyph->addChild(std::move(muteArea));
    volRow->addChild(std::move(baseGlyph));

    Slider* volSliderPtr = nullptr;
    auto volSlider = ui::slider({
        .out = &volSliderPtr,
        .minValue = 0.0,
        .maxValue = 1.0,
        .step = 0.01,
        .value = static_cast<double>(currentVolume),
        .enabled = true,
        .trackHeight = Style::sliderTrackHeight * scale,
        .thumbSize = Style::sliderThumbSize * scale,
        .controlHeight = Style::controlHeight * scale,
        .flexGrow = 1.0f,
        .onValueChanged = [this](double value) {
          if (m_syncingVolume) {
            return;
          }
          const AudioNode* currentSink = m_audio != nullptr ? m_audio->defaultSink() : nullptr;
          m_pendingSinkId = currentSink != nullptr ? currentSink->id : 0;
          m_pendingSinkVolume = static_cast<float>(std::clamp(value, 0.0, 1.0));
          if (m_volumeLabel != nullptr) {
            m_volumeLabel->setText(formatPercent(m_pendingSinkVolume));
          }
          m_volumeDebounceTimer.start(std::chrono::milliseconds(80), [this]() {
            if (m_audio != nullptr && m_pendingSinkVolume >= 0.0f) {
              m_audio->setSinkVolume(m_pendingSinkId, m_pendingSinkVolume);
              m_pendingSinkVolume = -1.0f;
            }
          });
        },
        .onDragEnd = [this]() {
          m_volumeDebounceTimer.stop();
          if (m_audio != nullptr && m_pendingSinkVolume >= 0.0f) {
            m_audio->setSinkVolume(m_pendingSinkId, m_pendingSinkVolume);
            m_pendingSinkVolume = -1.0f;
          }
        },
    });
    m_volumeSlider = volSliderPtr;
    volRow->addChild(std::move(volSlider));
    volRow->addChild(ui::label({
        .out = &m_volumeLabel,
        .text = formatPercent(currentVolume),
        .fontSize = Style::fontSizeBody * scale,
        .color = colorSpecFromRole(ColorRole::OnSurfaceVariant),
        .minWidth = labelMinWidth,
    }));
    sliderCard->addChild(std::move(volRow));
  }

  // Brightness row
  {
    float currentBrightness = 0.0f;
    if (m_brightness != nullptr && !m_brightness->displays().empty()) {
      for (const auto& d : m_brightness->displays()) {
        if (d.controllable) {
          m_primaryDisplayId = d.id;
          currentBrightness = d.brightness;
          break;
        }
      }
    }
    const bool brightnessAvailable = !m_primaryDisplayId.empty();

    auto brightRow = ui::row({.align = FlexAlign::Center, .gap = Style::spaceSm * scale});
    brightRow->addChild(ui::glyph({
        .out = &m_brightnessGlyph,
        .glyph = "brightness-high",
        .glyphSize = Style::fontSizeTitle * scale,
        .color = colorSpecFromRole(ColorRole::OnSurfaceVariant),
    }));

    Slider* brightSliderPtr = nullptr;
    auto brightSlider = ui::slider({
        .out = &brightSliderPtr,
        .minValue = 0.0,
        .maxValue = 1.0,
        .step = 0.01,
        .value = static_cast<double>(currentBrightness),
        .enabled = brightnessAvailable,
        .trackHeight = Style::sliderTrackHeight * scale,
        .thumbSize = Style::sliderThumbSize * scale,
        .controlHeight = Style::controlHeight * scale,
        .flexGrow = 1.0f,
        .onValueChanged = [this](double value) {
          if (m_syncingBrightness || m_primaryDisplayId.empty()) {
            return;
          }
          const float brightness = static_cast<float>(std::clamp(value, 0.0, 1.0));
          m_pendingBrightnessValue = brightness;
          m_pendingBrightness = true;
          if (m_brightnessLabel != nullptr) {
            m_brightnessLabel->setText(formatPercent(brightness));
          }
          m_brightnessDebounceTimer.start(std::chrono::milliseconds(80), [this]() {
            if (m_brightness != nullptr && m_pendingBrightness && !m_primaryDisplayId.empty()) {
              m_brightness->setBrightness(m_primaryDisplayId, m_pendingBrightnessValue);
              m_pendingBrightness = false;
            }
          });
        },
        .onDragEnd = [this]() {
          m_brightnessDebounceTimer.stop();
          if (m_brightness != nullptr && m_pendingBrightness && !m_primaryDisplayId.empty()) {
            m_brightness->setBrightness(m_primaryDisplayId, m_pendingBrightnessValue);
            m_pendingBrightness = false;
          }
        },
    });
    m_brightnessSlider = brightSliderPtr;
    brightRow->addChild(std::move(brightSlider));
    brightRow->addChild(ui::label({
        .out = &m_brightnessLabel,
        .text = formatPercent(currentBrightness),
        .fontSize = Style::fontSizeBody * scale,
        .color = colorSpecFromRole(ColorRole::OnSurfaceVariant),
        .minWidth = labelMinWidth,
    }));
    sliderCard->addChild(std::move(brightRow));
  }

  tab->addChild(std::move(sliderCard));

  // ── Bottom section: Info Card (Left) + Shortcuts Grid (Right) ──────────
  auto bottomSectionRow = ui::row({
      .align = FlexAlign::Stretch,
      .gap = Style::spaceMd * scale,
      .fillWidth = true,
  });

  // 1. Time, Date & Weather Card (Left) – with weather icon
  auto infoCard = ui::column({
      .align = FlexAlign::Center,
      .justify = FlexJustify::Center,
      .gap = Style::spaceXs * scale,
      .padding = Style::spaceMd * scale,
      .flexGrow = 1.0f,
      .configure = [scale, opacity = panelCardOpacity(), borders = panelBordersEnabled()](Flex& card) {
        applySectionCardStyle(card, scale, opacity, borders);
      },
  });

  // Time – larger
  infoCard->addChild(ui::label({
      .out = &m_timeLabel,
      .text = "12:00 PM",
      .fontSize = Style::fontSizeTitle * 1.8f * scale,
      .color = colorSpecFromRole(ColorRole::OnSurface),
      .fontWeight = FontWeight::Bold,
      .textAlign = TextAlign::Center,
  }));

  // Date – same size as weather
  const float dateWeatherFontSize = Style::fontSizeBody * 1.1f * scale;
  infoCard->addChild(ui::label({
      .out = &m_dateLabel,
      .text = formatShellDate(),
      .fontSize = dateWeatherFontSize,
      .color = colorSpecFromRole(ColorRole::OnSurfaceVariant),
      .textAlign = TextAlign::Center,
  }));

  // Weather row: icon + label, centered as a group
  auto weatherRow = ui::row({
      .align = FlexAlign::Center,
      .justify = FlexJustify::Center,   // <-- before .gap
      .gap = Style::spaceXs * scale,
      .fillWidth = true,
  });
  weatherRow->addChild(ui::glyph({
      .out = &m_weatherGlyph,
      .glyph = "weather-cloud",
      .glyphSize = dateWeatherFontSize,
      .color = colorSpecFromRole(ColorRole::Primary),
  }));
  weatherRow->addChild(ui::label({
      .out = &m_weatherLabel,
      .text = "—",
      .fontSize = dateWeatherFontSize,
      .color = colorSpecFromRole(ColorRole::Primary),
      .textAlign = TextAlign::Center,
  }));
  infoCard->addChild(std::move(weatherRow));

  bottomSectionRow->addChild(std::move(infoCard));

  // 2. Shortcuts Grid (Right) - unchanged
  const auto& shortcuts =
      m_config != nullptr ? m_config->config().controlCenter.shortcuts : std::vector<ShortcutConfig>{};
  const std::size_t count = std::min(shortcuts.size(), std::size_t{6});

  auto grid = std::make_unique<GridView>();
  grid->setColumns(kHomeShortcutGridColumns);
  grid->setColumnGap(Style::spaceMd * scale);
  grid->setRowGap(Style::spaceMd * scale);
  grid->setPadding(0.0f);
  grid->setUniformCellSize(true);
  grid->setStretchItems(true);
  grid->setSquareCells(true);
  grid->setMinCellHeight(Style::controlHeightLg * 1.9f * scale);
  grid->setMinCellWidth(Style::controlHeightLg * 1.9f * scale);
  grid->setFlexGrow(0.0f);
  m_shortcutsGrid = grid.get();
  m_shortcutPads.clear();

  for (std::size_t i = 0; i < count; ++i) {
    const auto& sc = shortcuts[i];
    auto shortcut = ShortcutRegistry::create(sc.type, m_services);
    if (shortcut == nullptr) {
      continue;
    }

    const std::string label = shortcut->displayLabel();
    const bool enabled = shortcut->enabled();
    const bool isActive = shortcut->isToggle() && shortcut->active();

    const std::size_t padIdx = m_shortcutPads.size();
    auto btn = ui::button({
        .text = label,
        .glyph = shortcut->displayIcon(),
        .glyphSize = Style::fontSizeTitle * 1.25f * scale,
        .minHeight = 0.0f,
        .padding = Style::spaceSm * scale,
        .gap = Style::spaceXs * scale,
        .radius = 9999.0f,
        .onClick =
            [this, padIdx]() {
              if (padIdx < m_shortcutPads.size()) {
                m_shortcutPads[padIdx].shortcut->onClick();
                kickShortcutResync();
              }
            },
        .onRightClick =
            [this, padIdx]() {
              if (padIdx < m_shortcutPads.size()) {
                m_shortcutPads[padIdx].shortcut->onRightClick();
                kickShortcutResync();
              }
            },
        .configure =
            [enabled, isActive, fillOpacity = panelCardOpacity(), scale](Button& button) {
              button.setAlign(FlexAlign::Stretch);
              button.label()->setFontSize(Style::fontSizeMini * scale);
              button.label()->setBaselineMode(LabelBaselineMode::InkCentered);
              button.label()->setMaxLines(1);
              button.label()->setTextAlign(TextAlign::Center);
              button.setDirection(FlexDirection::Vertical);
              applyShortcutButtonStyle(button, enabled, isActive, fillOpacity);
            },
    });

    Button* btnPtr = btn.get();
    if (auto* ia = btnPtr->inputArea(); ia != nullptr) {
      ia->setOnAxisHandler([this, padIdx](const InputArea::PointerData& data) -> bool {
        if (data.axis != WL_POINTER_AXIS_VERTICAL_SCROLL || padIdx >= m_shortcutPads.size()) {
          return false;
        }
        m_shortcutPads[padIdx].shortcut->onScroll(data.scrollDelta(1.0f) > 0 ? -1 : 1);
        kickShortcutResync();
        return true;
      });
    }
    ShortcutPad pad;
    pad.shortcut = std::move(shortcut);
    pad.button = btnPtr;
    pad.glyph = btnPtr->glyph();
    pad.label = btnPtr->label();
    // Mirrors the initial style applied by the .configure callback above, so the
    // first syncShortcuts() pass doesn't immediately redo work that was just done.
    pad.styleInitialized = true;
    pad.styleEnabled = enabled;
    pad.styleActive = isActive;
    pad.styleFillOpacity = panelCardOpacity();
    m_shortcutPads.push_back(std::move(pad));
    grid->addChild(std::move(btn));
  }

  if (m_shortcutPads.size() <= kHomeStackedShortcutMax) {
    grid->setColumns(1);
    grid->setFlexGrow(0.0f);
  }

  if (!m_shortcutPads.empty()) {
    auto gridWrapper = ui::row({
        .align = FlexAlign::Center,
        .justify = FlexJustify::Center,
        .flexGrow = 0.0f,
    });
    gridWrapper->addChild(std::move(grid));
    bottomSectionRow->addChild(std::move(gridWrapper));
  } else {
    m_shortcutsGrid = nullptr;
  }

  tab->addChild(std::move(bottomSectionRow));

  return tab;
}

std::unique_ptr<Flex> HomeTab::createHeaderActions() {
  const float scale = contentScale();
  return ui::row(
      {.align = FlexAlign::Center, .gap = Style::spaceSm * scale},
      ui::button({
          .out = &m_settingsButton,
          .glyph = "settings",
          .onClick = []() { PanelManager::instance().openSettingsWindow(); },
          .configure = [scale](Button& button) { panel_button_style::configureHeaderIconButton(button, scale); },
      }),
      ui::button({
          .out = &m_sessionButton,
          .glyph = "shutdown",
          .onClick = []() { PanelManager::instance().togglePanel("session"); },
          .configure = [scale](Button& button) { panel_button_style::configureHeaderIconButton(button, scale); },
      })
  );
}

void HomeTab::doLayout(Renderer& renderer, float contentWidth, float bodyHeight) {
  (void)bodyHeight;
  if (m_rootLayout == nullptr) {
    return;
  }

  // Avatar size
  if (m_userAvatar != nullptr) {
    const float scale = contentScale();
    const float avatarSize = homeAvatarSize(scale);
    if (std::abs(m_userAvatar->width() - avatarSize) > 0.5f) {
      m_userAvatar->setSize(avatarSize, avatarSize);
      m_userAvatar->setRadius(avatarSize * 0.5f);
      m_userAvatar->setPadding(1.0f * scale);
    }
  }

  // Wallpaper background layout
  layoutWallpaperBackground(renderer);
  layoutCardOverlays();

  // Shortcut label capping
  if (!m_shortcutPads.empty() && m_shortcutsGrid != nullptr) {
    const float scale = contentScale();
    for (auto& pad : m_shortcutPads) {
      if (pad.label == nullptr) {
        continue;
      }
      float inner = 1.0f;
      if (pad.button != nullptr && pad.button->width() > 1.0f) {
        inner = std::max(1.0f, pad.button->width() - pad.button->paddingLeft() - pad.button->paddingRight());
      } else {
        const float gridW = m_shortcutsGrid->width();
        const float innerGrid =
            std::max(1.0f, gridW - m_shortcutsGrid->paddingLeft() - m_shortcutsGrid->paddingRight());
        const std::size_t cols =
            std::max<std::size_t>(1, std::min(m_shortcutsGrid->columns(), m_shortcutPads.size()));
        const float cellWidth =
            (innerGrid - static_cast<float>(cols - 1) * m_shortcutsGrid->columnGap()) / static_cast<float>(cols);
        inner = std::max(1.0f, cellWidth - 2.0f * Style::spaceSm * scale);
      }
      pad.label->setMaxWidth(inner);
    }
  }

  m_rootLayout->setSize(contentWidth, bodyHeight);
  m_rootLayout->layout(renderer);
}

void HomeTab::doUpdate(Renderer& renderer) {
  if (!m_active) {
    return;
  }
  sync(renderer);
}

void HomeTab::onFrameTick(float /*deltaMs*/) {}

void HomeTab::setActive(bool active) {
  const bool becameActive = active && !m_active;
  m_active = active;
  if (becameActive) {
    DeferredCall::callLater([]() {
      PanelManager::instance().requestLayout();
      PanelManager::instance().requestUpdateOnly();
    });
  }
}

void HomeTab::onClose() {
  m_volumeDebounceTimer.stop();
  m_brightnessDebounceTimer.stop();
  m_shortcutSyncTimer.stop();
  m_rootLayout = nullptr;
  m_userAvatarArea = nullptr;
  m_userAvatar = nullptr;
  m_userCard = nullptr;
  m_userCardArea = nullptr;
  m_userCardKeyboardArea = nullptr;
  m_userMain = nullptr;
  m_userHost = nullptr;
  m_userUptime = nullptr;
  m_userVersion = nullptr;
  m_wallpaperPlaceholder = nullptr;
  m_wallpaperBg = nullptr;
  m_wallpaperGradient = nullptr;
  cancelCrispFade();
  m_crispWorkingPath.clear();
  m_crispWorkingSize = 0;
  m_crispShown = false;
  m_crispNeedsFade = false;
  m_volumeSlider = nullptr;
  m_volumeLabel = nullptr;
  m_volumeGlyph = nullptr;
  m_brightnessGlyph = nullptr;
  m_brightnessSlider = nullptr;
  m_brightnessLabel = nullptr;
  m_settingsButton = nullptr;
  m_sessionButton = nullptr;
  m_shortcutsGrid = nullptr;
  m_shortcutPads.clear();
  m_loadedAvatarPath.clear();
  m_loadedAvatarSize = 0;
  m_pendingSinkId = 0;
  m_pendingSinkVolume = -1.0f;
  m_pendingBrightness = false;
  m_lastBrightness = -1.0f;
  m_timeLabel = nullptr;
  m_dateLabel = nullptr;
  m_weatherLabel = nullptr;
  m_weatherGlyph = nullptr;
}

void HomeTab::onPanelCardOpacityChanged(float /*opacity*/) {
  syncShortcuts();
}

void HomeTab::syncScaledFonts() {
  const float s = contentScale();
  for (Label* label : {m_userHost, m_userUptime, m_userVersion}) {
    if (label != nullptr) {
      label->setFontSize(Style::fontSizeMini * s);
    }
  }
  for (auto& pad : m_shortcutPads) {
    if (pad.label != nullptr) {
      pad.label->setFontSize(Style::fontSizeMini * s);
    }
    if (pad.glyph != nullptr) {
      pad.glyph->setGlyphSize(Style::fontSizeTitle * 1.25f * s);
    }
  }
}

void HomeTab::syncVolumeSlider() {
  if (m_volumeSlider == nullptr || m_audio == nullptr) {
    return;
  }
  const AudioNode* sink = m_audio->defaultSink();
  if (sink == nullptr) {
    m_volumeSlider->setEnabled(false);
    return;
  }

  // Update glyph and color
  if (m_volumeGlyph != nullptr) {
    m_volumeGlyph->setGlyph(sink->muted ? "volume-mute" : "volume-high");
    m_volumeGlyph->setColor(
        sink->muted ? colorSpecFromRole(ColorRole::OnSurfaceVariant) : colorSpecFromRole(ColorRole::OnSurface)
    );
  }

  // Don't interfere if the user is dragging or we have a pending write
  if (m_volumeSlider->dragging() || m_pendingSinkVolume >= 0.0f) {
    return;
  }

  const float displayVolume = std::clamp(sink->volume, 0.0f, 1.0f);

  // Muted: set slider to 0 and disable it (greyed out)
  // Unmuted: set slider to actual volume and enable it
  m_syncingVolume = true;
  if (sink->muted) {
    m_volumeSlider->setValue(0.0);
    m_volumeSlider->setEnabled(false);
    if (m_volumeLabel != nullptr) {
      m_volumeLabel->setText("0%");
    }
  } else {
    m_volumeSlider->setValue(static_cast<double>(displayVolume));
    m_volumeSlider->setEnabled(true);
    if (m_volumeLabel != nullptr) {
      m_volumeLabel->setText(formatPercent(displayVolume));
    }
  }
  m_syncingVolume = false;
}

void HomeTab::syncBrightnessSlider() {
  if (m_brightnessSlider == nullptr || m_brightness == nullptr) {
    return;
  }
  if (m_primaryDisplayId.empty()) {
    for (const auto& d : m_brightness->displays()) {
      if (d.controllable) {
        m_primaryDisplayId = d.id;
        break;
      }
    }
  }
  if (m_primaryDisplayId.empty()) {
    m_brightnessSlider->setEnabled(false);
    return;
  }
  const auto* display = m_brightness->findDisplay(m_primaryDisplayId);
  if (display == nullptr) {
    m_brightnessSlider->setEnabled(false);
    return;
  }
  m_brightnessSlider->setEnabled(display->controllable);

  if (m_brightnessSlider->dragging() || m_pendingBrightness) {
    return;
  }
  if (std::abs(display->brightness - m_lastBrightness) < 0.005f) {
    return;
  }
  m_lastBrightness = display->brightness;
  m_syncingBrightness = true;
  m_brightnessSlider->setValue(static_cast<double>(display->brightness));
  m_syncingBrightness = false;
  if (m_brightnessLabel != nullptr) {
    m_brightnessLabel->setText(formatPercent(display->brightness));
  }
  if (m_brightnessGlyph != nullptr) {
    m_brightnessGlyph->setGlyph("brightness-high");
  }
}

void HomeTab::sync(Renderer& renderer) {
  syncScaledFonts();
  syncShortcuts();
  syncVolumeSlider();
  syncBrightnessSlider();

  syncWallpaperBackground(renderer);

  if (m_userAvatar != nullptr && m_config != nullptr) {
    const std::string displayPath = shell::avatarDisplayPath(m_accounts, m_config->config());
    const int avatarSize = static_cast<int>(std::round(m_userAvatar->width()));
    if (displayPath != m_loadedAvatarPath || avatarSize != m_loadedAvatarSize) {
      if (displayPath.empty()) {
        m_userAvatar->clear(renderer);
      } else {
        (void)m_userAvatar->setSourceFile(renderer, displayPath, avatarSize, false);
      }
      m_loadedAvatarPath = displayPath;
      m_loadedAvatarSize = avatarSize;
    }
  }

  if (m_userHost != nullptr) {
    m_userHost->setText(userHostLine());
  }
  if (m_userUptime != nullptr) {
    const auto uptime = systemUptime();
    const std::string uptimeText =
        uptime.has_value() ? formatDuration(*uptime) : i18n::tr("control-center.home.unknown");
    m_userUptime->setText(i18n::tr("control-center.home.uptime", "uptime", uptimeText));
  }
  if (m_userVersion != nullptr) {
    m_userVersion->setText(noctaliaVersionLine());
  }

  // Update time, date, weather with icon
  if (m_timeLabel != nullptr) {
    m_timeLabel->setText(formatShellTime(m_config));
  }
  if (m_dateLabel != nullptr) {
    m_dateLabel->setText(formatShellDate());
  }

  if (m_weatherGlyph != nullptr && m_weatherLabel != nullptr) {
    if (m_weather == nullptr || !m_weather->enabled()) {
      m_weatherGlyph->setGlyph("weather-cloud-off");
      m_weatherGlyph->setColor(colorSpecFromRole(ColorRole::OnSurfaceVariant));
      m_weatherLabel->setText(i18n::tr("control-center.home.weather.disabled"));
    } else if (!m_weather->locationConfigured()) {
      m_weatherGlyph->setGlyph("weather-cloud");
      m_weatherGlyph->setColor(colorSpecFromRole(ColorRole::OnSurfaceVariant));
      m_weatherLabel->setText(i18n::tr("control-center.weather.no-location-title"));
    } else {
      const auto& snapshot = m_weather->snapshot();
      if (!snapshot.valid) {
        m_weatherGlyph->setGlyph("weather-cloud");
        m_weatherGlyph->setColor(colorSpecFromRole(ColorRole::OnSurfaceVariant));
        m_weatherLabel->setText(
            m_weather->loading() ? i18n::tr("control-center.home.weather.fetching")
                                 : i18n::tr("control-center.home.weather.data-unavailable")
        );
      } else {
        m_weatherGlyph->setGlyph(WeatherService::glyphForCode(snapshot.current.weatherCode, snapshot.current.isDay));
        m_weatherGlyph->setColor(colorSpecFromRole(ColorRole::Primary));
        const int temp = static_cast<int>(std::lround(m_weather->displayTemperature(snapshot.current.temperatureC)));
        const std::string desc = WeatherService::descriptionForCode(snapshot.current.weatherCode);
        m_weatherLabel->setText(std::format("{}°{} · {}", temp, m_weather->displayTemperatureUnit(), desc));
      }
    }
  }
}

void HomeTab::kickShortcutResync() {
  PanelManager::instance().refresh();
  m_shortcutSyncTicksRemaining = 8;
  m_shortcutSyncTimer.startRepeating(std::chrono::milliseconds(150), [this]() {
    syncShortcuts();
    PanelManager::instance().refresh();
    if (--m_shortcutSyncTicksRemaining <= 0) {
      m_shortcutSyncTimer.stop();
    }
  });
}

void HomeTab::syncShortcuts() {
  const float fillOpacity = panelCardOpacity();
  for (auto& pad : m_shortcutPads) {
    auto& sc = *pad.shortcut;
    const bool enabled = sc.enabled();
    const bool on = sc.isToggle() && sc.active();

    if (pad.button != nullptr) {
      const bool styleChanged = !pad.styleInitialized || pad.styleEnabled != enabled || pad.styleActive != on ||
                                 pad.styleFillOpacity != fillOpacity;
      if (styleChanged) {
        applyShortcutButtonStyle(*pad.button, enabled, on, fillOpacity);
        pad.styleInitialized = true;
        pad.styleEnabled = enabled;
        pad.styleActive = on;
        pad.styleFillOpacity = fillOpacity;
      }
    }
    if (pad.glyph != nullptr) {
      pad.glyph->setGlyph(sc.displayIcon());
    }
    if (pad.button != nullptr && pad.label != nullptr) {
      const std::string label = sc.displayLabel();
      if (pad.label->text() != label) {
        pad.button->setText(label);
      }
    }
  }
}

InputArea* HomeTab::addCardOverlay(Flex& card, std::function<void()> onActivate) {
  return addCardOverlay(card, std::move(onActivate), CardOverlayOptions{});
}

InputArea* HomeTab::addCardOverlay(Flex& card, std::function<void()> onActivate, CardOverlayOptions options) {
  auto area = std::make_unique<InputArea>();
  area->setParticipatesInLayout(false);
  area->setZIndex(3);
  area->setCursorShape(WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_POINTER);
  if (!options.pointerHitTest) {
    area->setHitTestVisible(false);
  }
  if (options.keyboardFocus) {
    area->setFocusable(true);
  } else {
    area->setFocusable(false);
    area->setTabStop(false);
  }

  Flex* cardPtr = &card;
  const bool borders = panelBordersEnabled();
  InputArea* areaPtr = area.get();
  std::function<void()> activate = std::move(onActivate);

  const auto setHovered = [cardPtr, borders](bool hovered) {
    applyHomeCardHover(*cardPtr, hovered, borders);
    PanelManager::instance().requestRedraw();
  };

  if (options.pointerHitTest) {
    area->setOnEnter([setHovered](const InputArea::PointerData&) { setHovered(true); });
    area->setOnLeave([setHovered, areaPtr]() {
      if (areaPtr->focused()) {
        return;
      }
      setHovered(false);
    });
    area->setOnClick([activate](const InputArea::PointerData&) { activate(); });
  }
  if (options.keyboardFocus) {
    area->setOnFocusGain([setHovered]() { setHovered(true); });
    area->setOnFocusLoss([setHovered, areaPtr]() {
      if (areaPtr->hovered()) {
        return;
      }
      setHovered(false);
    });
    area->setOnKeyDown([activate](const InputArea::KeyData& key) {
      if (key.pressed && KeybindMatcher::matches(KeybindAction::Validate, key.sym, key.modifiers)) {
        activate();
      }
    });
  }

  return static_cast<InputArea*>(card.addChild(std::move(area)));
}

void HomeTab::layoutCardOverlays() {
  const auto cover = [](Flex* card, InputArea* area) {
    if (card == nullptr || area == nullptr) {
      return;
    }
    area->setPosition(0.0f, 0.0f);
    area->setSize(card->width(), card->height());
  };
  cover(m_userCard, m_userCardKeyboardArea);

  if (m_userCard != nullptr && m_userCardArea != nullptr) {
    float left = 0.0f;
    if (m_userAvatar != nullptr) {
      float ax = 0.0f, ay = 0.0f, cx = 0.0f, cy = 0.0f;
      Node::absolutePosition(m_userAvatar, ax, ay);
      Node::absolutePosition(m_userCard, cx, cy);
      left = std::max(0.0f, (ax - cx) + m_userAvatar->width() + Style::spaceMd * contentScale());
    }
    m_userCardArea->setPosition(left, 0.0f);
    m_userCardArea->setSize(std::max(0.0f, m_userCard->width() - left), m_userCard->height());
  }
}

void HomeTab::layoutWallpaperBackground(Renderer& renderer) {
  if (m_userCard == nullptr || m_wallpaperBg == nullptr) {
    return;
  }

  const float bw = Style::borderWidth;
  const float cw = std::max(0.0f, m_userCard->width() - bw * 2.0f);
  const float ch = std::max(0.0f, m_userCard->height() - bw * 2.0f);
  m_wallpaperBg->setPosition(bw, bw);
  m_wallpaperBg->setSize(cw, ch);
  if (m_wallpaperPlaceholder != nullptr) {
    m_wallpaperPlaceholder->setPosition(bw, bw);
    m_wallpaperPlaceholder->setSize(cw, ch);
  }

  if (m_wallpaperGradient != nullptr) {
    const float radius = std::max(0.0f, Style::scaledRadiusXl(contentScale()) - bw);
    m_wallpaperGradient->setPosition(bw, bw);
    m_wallpaperGradient->setFrameSize(cw, ch);
    const Color surface = colorForRole(ColorRole::Surface);
    const Color translucentSurface = rgba(surface.r, surface.g, surface.b, surface.a * 0.92f);
    const Color transparentSurface = rgba(surface.r, surface.g, surface.b, 0.0f);
    m_wallpaperGradient->setStyle(
        RoundedRectStyle{
            .fill = surface,
            .fillMode = FillMode::LinearGradient,
            .gradientDirection = GradientDirection::Horizontal,
            .gradientStops =
                {GradientStop{0.0f, translucentSurface}, GradientStop{0.55f, translucentSurface},
                 GradientStop{1.0f, transparentSurface}},
            .radius = radius,
        }
    );
  }

  syncWallpaperBackground(renderer);
}

void HomeTab::ensureWallpaperThumbnail(const std::string& path, int targetPx) {
  if (m_thumbnails == nullptr) {
    return;
  }
  if (path == m_loadedWallpaperPath && targetPx == m_loadedWallpaperSize) {
    return;
  }
  if (!m_loadedWallpaperPath.empty() && m_loadedWallpaperSize > 0) {
    m_thumbnails->release(m_loadedWallpaperPath, m_loadedWallpaperSize);
  }
  if (!path.empty() && targetPx > 0) {
    (void)m_thumbnails->acquire(path, targetPx);
  }
  m_loadedWallpaperPath = path;
  m_loadedWallpaperSize = targetPx;
}

void HomeTab::syncWallpaperBackground(Renderer& renderer) {
  if (m_wallpaperBg == nullptr || m_wallpaperPlaceholder == nullptr) {
    return;
  }

  const std::string path = m_wallpaper != nullptr ? m_wallpaper->currentPath() : std::string{};
  const float renderScale = std::max(1.0f, renderer.renderScale());
  const int targetPx =
      static_cast<int>(std::lround(std::max(m_wallpaperBg->width(), m_wallpaperBg->height()) * renderScale));

  ensureWallpaperThumbnail(path, targetPx);

  if (path.empty()) {
    m_wallpaperPlaceholder->setVisible(false);
    m_wallpaperBg->setVisible(false);
    cancelCrispFade();
    m_wallpaperBg->setOpacity(0.0f);
    m_crispWorkingPath.clear();
    m_crispWorkingSize = 0;
    m_crispShown = false;
    m_crispNeedsFade = false;
    return;
  }

  const TextureHandle resident = m_wallpaper != nullptr ? m_wallpaper->currentTexture() : TextureHandle{};
  if (resident.valid()) {
    m_wallpaperPlaceholder->setExternalTexture(renderer, resident);
    m_wallpaperPlaceholder->setVisible(true);
  } else {
    m_wallpaperPlaceholder->setVisible(false);
  }

  if (path != m_crispWorkingPath || targetPx != m_crispWorkingSize) {
    m_crispWorkingPath = path;
    m_crispWorkingSize = targetPx;
    m_crispShown = false;
    m_crispNeedsFade = false;
    cancelCrispFade();
    m_wallpaperBg->setOpacity(0.0f);
    m_wallpaperBg->setVisible(false);
  }

  if (m_thumbnails == nullptr || targetPx <= 0 || m_crispShown) {
    return;
  }

  (void)m_thumbnails->uploadPending(renderer.textureManager());
  const TextureHandle crisp = m_thumbnails->peek(path, targetPx);
  if (!crisp.valid()) {
    m_crispNeedsFade = true;
    return;
  }

  m_wallpaperBg->setExternalTexture(renderer, crisp);
  m_wallpaperBg->setVisible(true);
  m_crispShown = true;
  if (m_crispNeedsFade) {
    startCrispFade();
  } else {
    cancelCrispFade();
    m_wallpaperBg->setOpacity(1.0f);
  }
}

void HomeTab::startCrispFade() {
  if (m_wallpaperBg == nullptr) {
    return;
  }
  AnimationManager* animations = m_wallpaperBg->animationManager();
  if (animations == nullptr) {
    m_wallpaperBg->setOpacity(1.0f);
    return;
  }
  cancelCrispFade();
  Image* crisp = m_wallpaperBg;
  m_wallpaperCrispAnimId = animations->animate(
      0.0f, 1.0f, static_cast<float>(Style::animNormal), Easing::EaseOutCubic,
      [crisp](float v) { crisp->setOpacity(v); }, [this]() { m_wallpaperCrispAnimId = 0; }, crisp
  );
}

void HomeTab::cancelCrispFade() {
  if (m_wallpaperCrispAnimId != 0 && m_wallpaperBg != nullptr) {
    if (AnimationManager* animations = m_wallpaperBg->animationManager()) {
      animations->cancel(m_wallpaperCrispAnimId);
    }
  }
  m_wallpaperCrispAnimId = 0;
}
