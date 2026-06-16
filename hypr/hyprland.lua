-- This is an example Hyprland Lua config file.
-- Refer to the wiki for more information.
-- https://wiki.hypr.land/Configuring/Start/

-- Please note not all available settings / options are set here.
-- For a full list, see the wiki

-- You can (and should!!) split this configuration into multiple files
-- Create your files separately and then require them like this:
-- require("myColors")


------------------
---- MONITORS ----
------------------

-- See https://wiki.hypr.land/Configuring/Basics/Monitors/
hl.monitor({
    output   = "",
    mode     = "preferred",
    position = "auto",
    scale    = "auto",
})


---------------------
---- MY PROGRAMS ----
---------------------

-- Set programs that you use
local terminal    = "kitty"
local fileManager = "nautilus"
local menu        = "nwg-drawer"


-------------------
---- AUTOSTART ----
-------------------

-- See https://wiki.hypr.land/Configuring/Basics/Autostart/

-- Autostart necessary processes (like notifications daemons, status bars, etc.)
-- Or execute your favorite apps at launch like this:
--
-- hl.on("hyprland.start", function ()
--   hl.exec_cmd(terminal)
--   hl.exec_cmd("nm-applet")
--   hl.exec_cmd("waybar & hyprpaper & firefox")
-- end)


hl.on("hyprland.start", function()

    hl.exec_cmd("noctalia")

    hl.exec_cmd("/usr/lib/polkit-gnome/polkit-gnome-authentication-agent-1")

    hl.exec_cmd("gnome-keyring-daemon --start --components=pkcs11,secrets")

    -- hl.exec_cmd("~/.config/scripts/media-pause.sh")

    hl.exec_cmd("wl-paste --watch clipvault store --max-entries 200 --max-entry-age 2d")

    hl.exec_cmd("wl-clip-persist --clipboard both")

end)

-------------------------------
---- ENVIRONMENT VARIABLES ----
-------------------------------

-- See https://wiki.hypr.land/Configuring/Advanced-and-Cool/Environment-variables/

hl.env("XCURSOR_SIZE", "24")
hl.env("XCURSOR_THEME","Bibata-Modern-Ice")
hl.env("HYPRCURSOR_SIZE", "24")
hl.env("HYPRCURSOR_THEME","Bibata-Modern-Ice")

hl.env("XDG_CURRENT_DESKTOP", "Hyprland")
hl.env("XDG_SESSION_TYPE", "wayland")
hl.env("XDG_SESSION_DESKTOP", "Hyprland")
hl.env("QT_QPA_PLATFORMTHEME", "gtk3")

-----------------------
----- PERMISSIONS -----
-----------------------

-- See https://wiki.hypr.land/Configuring/Advanced-and-Cool/Permissions/
-- Please note permission changes here require a Hyprland restart and are not applied on-the-fly
-- for security reasons

-- hl.config({
--   ecosystem = {
--     enforce_permissions = true,
--   },
-- })

-- hl.permission("/usr/(bin|local/bin)/grim", "screencopy", "allow")
-- hl.permission("/usr/(lib|libexec|lib64)/xdg-desktop-portal-hyprland", "screencopy", "allow")
-- hl.permission("/usr/(bin|local/bin)/hyprpm", "plugin", "allow")


-----------------------
---- LOOK AND FEEL ----
-----------------------

-- Refer to https://wiki.hypr.land/Configuring/Basics/Variables/
hl.config({
    general = {
        gaps_in  = 3,
        gaps_out = 5,

        border_size = 2,

        col = {
            -- active_border   = "rgba(33ccffee)",
            active_border   = "rgba(d3d3d3ee)",
            inactive_border = "rgba(595959aa)",
        },

        -- Set to true to enable resizing windows by clicking and dragging on borders and gaps
        resize_on_border = false,

        -- Please see https://wiki.hypr.land/Configuring/Advanced-and-Cool/Tearing/ before you turn this on
        allow_tearing = false,

        -- layout = "dwindle",
        layout = "scrolling",
    },

    decoration = {
        rounding       = 2,
        rounding_power = 2,

        -- Change transparency of focused and unfocused windows
        active_opacity   = 0.95,
        inactive_opacity = 0.95,

        shadow = {
            enabled      = true,
            range        = 4,
            render_power = 3,
            color        = 0xee1a1a1a,
        },

        blur = {
            enabled   = true,
            size      = 6,
            passes    = 3,
            vibrancy  = 0.1696,
        },
    },

    animations = {
        enabled = true,
    },
})

-- -- Curves and animations, see https://wiki.hypr.land/Configuring/Advanced-and-Cool/Animations/

-- Curves
hl.curve("overshoot", { type = "bezier", points = { { 0.05, 0.9 }, { 0.1, 1.1 } } })
hl.curve("gnomeOut",  { type = "bezier", points = { { 0.25, 1.0 }, { 0.5, 1.0 } } })
hl.curve("easeOut",   { type = "bezier", points = { { 0.16, 1 },   { 0.3, 1 } } })
hl.curve("snapOut",   { type = "bezier", points = { { 0.3, 1 },    { 0.5, 1 } } })

-- Animations
hl.animation({ leaf = "windows", enabled = true, speed = 7, bezier = "overshoot" })
hl.animation({ leaf = "windowsIn", enabled = true, speed = 5, bezier = "easeOut", style = "slide right" })
hl.animation({ leaf = "windowsOut", enabled = true, speed = 3, bezier = "snapOut", style = "gnome" })
hl.animation({ leaf = "windowsMove", enabled = true, speed = 6, bezier = "easeOut" })
hl.animation({ leaf = "workspaces", enabled = true, speed = 6, bezier = "overshoot", style = "slidefadevert 20%" })
hl.animation({ leaf = "layersIn", enabled = true, speed = 5, bezier = "easeOut", style = "slide" })
hl.animation({ leaf = "layersOut", enabled = true, speed = 4, bezier = "snapOut", style = "slide" })
hl.animation({ leaf = "fadeIn", enabled = true, speed = 5, bezier = "easeOut" })
hl.animation({ leaf = "fadeOut", enabled = true, speed = 3, bezier = "snapOut" })


-- Ref https://wiki.hypr.land/Configuring/Basics/Workspace-Rules/
-- "Smart gaps" / "No gaps when only"
-- uncomment all if you wish to use that.
-- hl.workspace_rule({ workspace = "w[tv1]", gaps_out = 0, gaps_in = 0 })
-- hl.workspace_rule({ workspace = "f[1]",   gaps_out = 0, gaps_in = 0 })
-- hl.window_rule({
--     name  = "no-gaps-wtv1",
--     match = { float = false, workspace = "w[tv1]" },
--     border_size = 0,
--     rounding    = 0,
-- })
-- hl.window_rule({
--     name  = "no-gaps-f1",
--     match = { float = false, workspace = "f[1]" },
--     border_size = 0,
--     rounding    = 0,
-- })

-- See https://wiki.hypr.land/Configuring/Layouts/Dwindle-Layout/ for more
hl.config({
    dwindle = {
        preserve_split = true, -- You probably want this
    },
})

-- See https://wiki.hypr.land/Configuring/Layouts/Master-Layout/ for more
hl.config({
    master = {
        new_status = "master",
    },
})

-- See https://wiki.hypr.land/Configuring/Layouts/Scrolling-Layout/ for more
hl.config({
    scrolling = {
        fullscreen_on_one_column = true,
        column_width = 1
    },
})

----------------
----  MISC  ----
----------------

hl.config({
    misc = {
        force_default_wallpaper = -1,    -- Set to 0 or 1 to disable the anime mascot wallpapers
        disable_hyprland_logo   = false, -- If true disables the random hyprland logo / anime girl background. :(
    },
})

---------------
---- INPUT ----
---------------

hl.config({
    input = {
        kb_layout  = "us",
        kb_variant = "",
        kb_model   = "",
        kb_options = "",
        kb_rules   = "",
        numlock_by_default = true,
        follow_mouse = 1,

        sensitivity = 0, -- -1.0 - 1.0, 0 means no modification.

        touchpad = {
            natural_scroll = true,
        },
    },
})

-- Example per-device config
-- See https://wiki.hypr.land/Configuring/Advanced-and-Cool/Devices/ for more
hl.device({
    name        = "epic-mouse-v1",
    sensitivity = -0.5,
})

---------------------
---- Gestures ----
---------------------

-- switch workspaces
hl.gesture({
    fingers = 3,
    direction = "vertical",
    action = "workspace",
    scale = 1.5
})

-- switch windows in scrolling layout
hl.gesture({
    fingers = 3,
    direction = "horizontal",
    action = "scroll_move",
    scale = 2.5
})

-- close window
hl.gesture({
    fingers = 4,
    direction = "down",
    action = "close",
    -- scale = 3.5
})

-- nwg-drawer
hl.gesture({
    fingers = 4,
    direction = "up",
    action = function()
        hl.exec_cmd("nwg-drawer")
    end,
    scale = 5.0
})


---------------------
---- KEYBINDINGS ----
---------------------

local mainMod = "SUPER" -- Sets "Windows" key as main modifier

-- Example binds, see https://wiki.hypr.land/Configuring/Basics/Binds/ for more
hl.bind(mainMod .. " + T", hl.dsp.exec_cmd(terminal))
local closeWindowBind = hl.bind(mainMod .. " + Q", hl.dsp.window.close())
-- closeWindowBind:set_enabled(false)
hl.bind(mainMod .. " + M", hl.dsp.exec_cmd("command -v hyprshutdown >/dev/null 2>&1 && hyprshutdown || hyprctl dispatch 'hl.dsp.exit()'"))
hl.bind(mainMod .. " + E", hl.dsp.exec_cmd(fileManager))
hl.bind(mainMod .. " + V", hl.dsp.window.float({ action = "toggle" }))
hl.bind(mainMod .. " + R", hl.dsp.exec_cmd(menu))
hl.bind(mainMod .. " + P", hl.dsp.window.pseudo())
hl.bind(mainMod .. " + J", hl.dsp.layout("togglesplit"))    -- dwindle only

-- Move focus with mainMod + arrow keys
hl.bind(mainMod .. " + left",  hl.dsp.focus({ direction = "left" }))
hl.bind(mainMod .. " + right", hl.dsp.focus({ direction = "right" }))
hl.bind(mainMod .. " + up",    hl.dsp.focus({ direction = "up" }))
hl.bind(mainMod .. " + down",  hl.dsp.focus({ direction = "down" }))

-- Switch workspaces with mainMod + [0-9]
-- Move active window to a workspace with mainMod + SHIFT + [0-9]
for i = 1, 10 do
    local key = i % 10 -- 10 maps to key 0
    hl.bind(mainMod .. " + " .. key,             hl.dsp.focus({ workspace = i}))
    hl.bind(mainMod .. " + SHIFT + " .. key,     hl.dsp.window.move({ workspace = i }))
end

-- Example special workspace (scratchpad)
hl.bind(mainMod .. " + S",         hl.dsp.workspace.toggle_special("magic"))
hl.bind(mainMod .. " + SHIFT + S", hl.dsp.window.move({ workspace = "special:magic" }))

-- Scroll through existing workspaces with mainMod + scroll
hl.bind(mainMod .. " + mouse_down", hl.dsp.focus({ workspace = "e+1" }))
hl.bind(mainMod .. " + mouse_up",   hl.dsp.focus({ workspace = "e-1" }))

-- Move/resize windows with mainMod + LMB/RMB and dragging
hl.bind(mainMod .. " + mouse:272", hl.dsp.window.drag(),   { mouse = true })
hl.bind(mainMod .. " + mouse:273", hl.dsp.window.resize(), { mouse = true })

-- Laptop multimedia keys for volume and LCD brightness
hl.bind("XF86AudioRaiseVolume", hl.dsp.exec_cmd("wpctl set-volume -l 1 @DEFAULT_AUDIO_SINK@ 5%+"), { locked = true, repeating = true })
hl.bind("XF86AudioLowerVolume", hl.dsp.exec_cmd("wpctl set-volume @DEFAULT_AUDIO_SINK@ 5%-"),      { locked = true, repeating = true })
hl.bind("XF86AudioMute",        hl.dsp.exec_cmd("wpctl set-mute @DEFAULT_AUDIO_SINK@ toggle"),     { locked = true, repeating = true })
hl.bind("XF86AudioMicMute",     hl.dsp.exec_cmd("wpctl set-mute @DEFAULT_AUDIO_SOURCE@ toggle"),   { locked = true, repeating = true })
hl.bind("XF86MonBrightnessUp",  hl.dsp.exec_cmd("brightnessctl -e4 -n2 set 5%+"),                  { locked = true, repeating = true })
hl.bind("XF86MonBrightnessDown",hl.dsp.exec_cmd("brightnessctl -e4 -n2 set 5%-"),                  { locked = true, repeating = true })

-- Requires playerctl
hl.bind("XF86AudioNext",  hl.dsp.exec_cmd("playerctl next"),       { locked = true })
hl.bind("XF86AudioPause", hl.dsp.exec_cmd("playerctl play-pause"), { locked = true })
hl.bind("XF86AudioPlay",  hl.dsp.exec_cmd("playerctl play-pause"), { locked = true })
hl.bind("XF86AudioPrev",  hl.dsp.exec_cmd("playerctl previous"),   { locked = true })

-- My binds

hl.bind(mainMod .. " + D", hl.dsp.exec_cmd("nwg-drawer"))

hl.bind(mainMod .. " + C", hl.dsp.exec_cmd("code"))

hl.bind(mainMod .. " + X", hl.dsp.exec_cmd("google-chrome-stable"))

-- full screenshot
hl.bind(mainMod .. " + SHIFT + Insert", function()
    hl.exec_cmd("noctalia msg screenshot-fullscreen")
end)

-- area screenshot
hl.bind(mainMod .. " + Insert", function()
    hl.exec_cmd("noctalia msg screenshot-region")
end)

-- lock
hl.bind(mainMod .. " + L", hl.dsp.exec_cmd(
    "sh -c 'playerctl pause 2>/dev/null; noctalia msg session lock'"
))

-- move window
hl.bind(mainMod .. " + W", hl.dsp.window.drag())

-- resize window
hl.bind(mainMod .. " + A", hl.dsp.window.resize())

-- resize window
hl.bind(mainMod .. " + period", hl.dsp.layout("colresize 0.5"))
hl.bind(mainMod .. " + comma", hl.dsp.layout("colresize 1"))

-- Lock the screen when closing the lid
hl.bind(
    "switch:on:Lid Switch",
    hl.dsp.exec_cmd("hyprctl dispatch 'hl.dsp.dpms({ action = \"disable\" })' && noctalia msg session lock"),
    { locked = true }
)

hl.bind(
    "switch:off:Lid Switch",
    hl.dsp.exec_cmd("hyprctl dispatch 'hl.dsp.dpms({ action = \"enable\" })'"),
    { locked = true }
)

--------------------------------
---- WINDOWS AND WORKSPACES ----
--------------------------------

-- See https://wiki.hypr.land/Configuring/Basics/Window-Rules/
-- and https://wiki.hypr.land/Configuring/Basics/Workspace-Rules/

-- Example window rules that are useful

local suppressMaximizeRule = hl.window_rule({
    -- Ignore maximize requests from all apps. You'll probably like this.
    name  = "suppress-maximize-events",
    match = { class = ".*" },

    suppress_event = "maximize",
})
-- suppressMaximizeRule:set_enabled(false)

hl.window_rule({
    -- Fix some dragging issues with XWayland
    name  = "fix-xwayland-drags",
    match = {
        class      = "^$",
        title      = "^$",
        xwayland   = true,
        float      = true,
        fullscreen = false,
        pin        = false,
    },

    no_focus = true,
})

-- Layer rules also return a handle.
-- local overlayLayerRule = hl.layer_rule({
--     name  = "no-anim-overlay",
--     match = { namespace = "^my-overlay$" },
--     no_anim = true,
-- })
-- overlayLayerRule:set_enabled(false)

-- Hyprland-run windowrule
hl.window_rule({
    name  = "move-hyprland-run",
    match = { class = "hyprland-run" },

    move  = "20 monitor_h-120",
    float = true,
})

-- screen sharing popup
hl.window_rule({
    match = { class = "hyprland-share-picker" },
    float = true,
    size  = {800, 600}
})

-- file/folder dialogues
hl.window_rule({
    match = {
        xwayland = 1,
        title = "^([Oo]pen|[Ss]ave|[Cc]hoose|[Ss]elect|[Pp]ick) .*([Ff]ile|[Ff]older|[Dd]irectory|[Ff]iles).*",
    },
    float = true,
    size = {800, 500},
    move = {560, 290}
})

-- Pavucontrol
hl.window_rule({
    name = "pavucontrol-float",

    match = {
        class = "org.pulseaudio.pavucontrol",
    },

    float = true,
    pin = true,

    size = "600 600"
})

-- Blueman Manager
hl.window_rule({
    name = "blueman-float",

    match = {
        class = "blueman-manager",
    },

    float = true,

    size = "600 600"
})

-- GNOME Calculator
hl.window_rule({
    name = "calculator-float",

    match = {
        class = "org.gnome.Calculator",
    },

    float = true,

    size = "500 500"
})

-- Network Manager
hl.window_rule({
    name = "nm-editor-float",

    match = {
        class = "nm-connection-editor",
    },

    float = true,

    size = "600 600"
})

-- WhatsApp Web (Chrome)
hl.window_rule({
    name = "whatsapp-web",

    match = {
        class = "Google-chrome",
        title = "WhatsApp Web",
    },

    float = true,

    size = "1000 1000"
})

-- Blur nwg-drawer
hl.layer_rule({
    name = "nwg-drawer-blur",

    match = {
        namespace = "nwg-drawer",
    },

    blur = true,
})

-- Blur noctalia-shell
hl.layer_rule({
  name = "noctalia",
  match = {
    namespace = "^noctalia-(bar-.+|notification|dock|panel)$",
  },
  ignore_alpha = 0.5,
  blur = true,
  blur_popups = true,
})
