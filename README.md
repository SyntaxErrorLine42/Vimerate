# Vimerate

⚡ **Blazing fast Vim-style grid launcher for Windows**  
🚀 Ideal for power users and fans of Vim/Neovim workflows

---

## 🔥 Why Vimerate?

Vimerate is a lightning-fast, keyboard-driven screen grid launcher for Windows, built with performance in mind using native C++ and Win32 APIs. Whether you're a die-hard terminal user, a Vim/Neovim fan, or just someone who values speed and efficiency, Vimerate delivers a responsive experience that blends seamlessly with your setup.

---
![image](https://github.com/user-attachments/assets/027610da-5a11-49fc-b930-ce9943ff4016)
![image](https://github.com/user-attachments/assets/124e20bc-8d37-4741-a6b0-d965986e420d)


## ✨ Features

- ⚡ **Blazing Fast**: Written in highly optimized C++, leveraging Win32 and GDI+ APIs directly.
- 🎯 **Perfect for Vim/Neovim users**: Use 2-character grid codes like `aj` or `a.j` to instantly jump to and interact with screen locations—just like Vim motions.
- 🛠️ **Fully Configurable**: Adjust hotkeys, cell color, and grid density directly via a GUI settings window.
- 📐 **Screen-Adaptive**: Works beautifully across all screen resolutions and multi-monitor setups.
- 🧰 **Persistent Settings**: All customizations are saved to an INI file, so your preferences stay intact.
- 🧪 **Open Source**: Modify, extend, and improve it. Contributions welcome!

---

## 🛠️ Building Vimerate

To compile the project, use the following `g++` command:

```sh
g++ Vimerate.cpp Vimerate.res -o Vimerate -lgdi32 -lgdiplus -static-libgcc -static-libstdc++ -mwindows -lcomctl32 -lshell32
```

📝 Make sure you have:
- `g++` from [MinGW](http://mingw.org/) or a similar Windows-compatible compiler
- Resource file `Vimerate.res` (must include icons and other Windows resources)
- Static linking options ensure no runtime dependencies for redistribution

---

## 🧑‍💻 Usage

1. Launch Vimerate — it runs in the background and sits quietly in your system tray.
2. Press the global hotkey (default: **Win + Shift + Z**) to activate the grid overlay.
3. Start typing a grid code (e.g., `az` or `a.z`) to jump to a screen location.
4. Once a match is made, press:
   - `1` for Left Click
   - `2` for Right Click
   - `3` for Double Click
5. Use the tray icon to access **Settings** or exit the app.

---

## ⚙️ Settings Panel

From the system tray:
- 🎨 **Choose Cell Color**: Set a semi-transparent background for better visibility.
- 🔢 **Grid Pool Size**: Define how many characters are used in grid combinations.
- 🎚️ **Custom Hotkeys**: Select modifier keys and main key via dropdowns.
- 🔄 **Reset Defaults**: Instantly revert to the original configuration.

Settings are saved to an INI file located in `./Settings/VimerateSettings.ini`.

![image](https://github.com/user-attachments/assets/58a56c1f-fa3b-455b-be6b-f45701a38eec)

---

## 🧾 License

Vimerate is open source and licensed under the **MIT License**.

---

## 🤝 Contribute

Pull requests, suggestions, and bug reports are welcome! Feel free to fork and improve.

---

## 🙏 Acknowledgements

Vimerate is inspired by the minimalist efficiency of Vim and the power of native Windows development.

---

> 💡 Tip: Pair Vimerate with Neovim and AutoHotKey for the ultimate keyboard-driven Windows workflow.
