***Follow this after a fresh OS installation without any DE (This is tested only on CachyOS)***

1.  **Install hyprland, sddm, chrome and kitty**

      ```bash
      sudo pacman -S hyprland sddm kitty ; sudo systemctl enable sddm
      ```

      ```bash
      paru -S google-chrome
      ```

2.  **Install necessaries**

      ```bash      
      sudo pacman -S --needed --noconfirm nwg-drawer nwg-look polkit-gnome gnome-keyring xdg-desktop-portal-hyprland hypridle wl-clipboard starship network-manager-applet gnome-calculator gnome-text-editor gnome-clocks blueman nautilus onlyoffice-bin telegram-desktop transmission-gtk smplayer swappy evince brightnessctl hyprpicker wlsunset cachyos-kernel-manager grimblast python-pydbus python-gobject python-dbus-next
      ```

      ```bash
      paru -S --needed --noconfirm sddm-sugar-candy-git catppuccin-gtk-theme-mocha bibata-cursor-theme visual-studio-code-bin zoom clipvault-bin
      ```

4.  **Clone the dotfiles repo**

      ```bash
      git clone --depth=1 git@github.com:vijaygudduri/hyprland-noctalia.git
      ```

5.  **Copy the configs from cloned repo to ~/.config**

      ```bash
      cd ~/hyprland-noctalia #cd to cloned repo
      ```
      
      ```bash
      cp -r wallpapers ~/ && cp -r fastfetch hypr kitty nwg-drawer scripts chrome-flags.conf ~/.config/
      ```   

6.  **Install noctalia-shell**

      ```bash
      sudo pacman -S noctalia-shell
      ```

7.  **Apply themes from nwg-look (theme is 'catppuccin mocha' and cursor theme is 'bibata modern ice')**

8.  **To apply sugar-candy theme on sddm, run below commands**

      ```bash
      sudo mkdir -p /etc/sddm.conf.d ; sudo touch /etc/sddm.conf.d/sddm.conf
      ```
      
      ```bash
      bash -c "sudo tee /etc/sddm.conf.d/sddm.conf > /dev/null <<'EOF'
      [General]
      Numlock=on
      
      [Theme]
      Current=sugar-candy
      CursorTheme=Bibata-Modern-Ice
      CursorSize=24
      EOF"
      ```

10.  **To decrease boot order timeout prompt of systemd while rebooting, switch to root and change timeout to 2 (or 0 to disable completly) in /boot/loader/loader.conf**

11.  **Change to google dns, replace 'Android' with your connection name**

      ```bash
      nmcli con mod 'Android' ipv4.dns '8.8.8.8 8.8.4.4'
      nmcli con mod 'Android' ipv6.dns '2001:4860:4860::8888 2001:4860:4860::8844'
      nmcli con up 'Android'
      ```

12.  **Add starship config and modify ls alias in fish**

      ```bash
      echo -e "\n\nalias ls='eza --color=always --group-directories-first --icons'\n\nstarship init fish | source" >> ~/.config/fish/config.fish
      ```


***Reboot after all the process is done***