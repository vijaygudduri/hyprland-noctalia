***Follow this after a fresh OS installation without any DE (This is tested only on CachyOS)***

1.  **Install hyprland, sddm, chrome and kitty**

      ```bash
      sudo pacman -S hyprland sddm kitty ; sudo systemctl enable sddm
      ```

2.  **Install necessaries**

      ```bash      
      sudo pacman -S --needed --noconfirm nwg-drawer nwg-look gnome-keyring xdg-desktop-portal-hyprland hypridle hyprlock wl-clipboard wl-clip-persist gnome-calculator gnome-text-editor gnome-clocks blueman nautilus smplayer swappy evince brightnessctl playerctl python-dbus-next jq xorg-xrdb just
      ```

      ```bash
      paru -S --needed --noconfirm sddm-astronaut-theme catppuccin-gtk-theme-mocha bibata-cursor-theme visual-studio-code-bin
      ```

4.  **Clone the dotfiles repo**

      ```bash
      git clone --depth=1 https://github.com/vijaygudduri/hyprland-noctalia.git
      ```

5.  **Copy the configs from cloned repo to ~/.config**

      ```bash
      cd ~/hyprland-noctalia #cd to cloned repo
      ```
      
      ```bash
      cp -r Wallpapers/* ~/Pictures/Wallpapers/ && cp .zshrc_myconfigs ~ && cp -r fastfetch hypr kitty nwg-drawer scripts chrome-flags.conf ~/.config/
      ```

      ```bash
      chmod +x ~/.config/scripts/*.{sh,py}
      ```

6.  **Install noctalia-shell**

      ```bash
      paru -S noctalia-git
      ```

      ```bash
      cp ~/hyprland-noctalia/noctalia-config.toml ~/.config/noctalia
      ```

7.  **Download Candy icon theme & extract it to ~/.icons and Apply themes from nwg-look**

      candy icons --> https://www.gnome-look.org/p/1305251/

      ```bash
      mkdir -p ~/.icons && tar -xJf ~/Downloads/candy-icons.tar.xz -C ~/.icons
      ```

9.  **To apply astronaut theme on sddm, run below commands**

      ```bash
      sudo mkdir -p /etc/sddm.conf.d ; sudo touch /etc/sddm.conf.d/sddm.conf
      ```
      
      ```bash
      bash -c "sudo tee /etc/sddm.conf.d/sddm.conf > /dev/null <<'EOF'
      [General]
      Numlock=on
      
      [Theme]
      Current=sddm-astronaut-theme
      CursorTheme=Bibata-Modern-Ice
      CursorSize=24
      EOF"
      ```

10.  **To decrease boot order timeout prompt of systemd while rebooting, switch to root and change timeout to 2 (or 0 to disable completly) in /boot/loader/loader.conf**

11.  **Change to cloudflare dns, replace 'Interstellar' with your connection name**

      ```bash
      nmcli con mod 'Interstellar' ipv4.dns '1.1.1.1 1.0.0.1'
      nmcli con mod 'Interstellar' ipv6.dns '2606:4700:4700::1111 2606:4700:4700::1001'
      
      nmcli con mod 'Interstellar' ipv4.ignore-auto-dns yes
      nmcli con mod 'Interstellar' ipv6.ignore-auto-dns yes
      
      nmcli con up 'Interstellar'
      ```

12.  **Change the shell to zsh**

      ```bash
      chsh -s $(which zsh)
      ```

13.  **Copy some custom configs to .zshrc**

      ```bash
      printf '# My custom configs\n[[ -f ~/.zshrc_myconfigs ]] && source ~/.zshrc_myconfigs\n\n' | cat - ~/.zshrc > ~/.zshrc.tmp && mv ~/.zshrc.tmp ~/.zshrc
      ```


***Reboot after all the process is done***
