# PSV Cleaner

![GitHub release (latest by date)](https://img.shields.io/github/v/release/theheroGAC/PSV-Cleaner)
![GitHub license](https://img.shields.io/github/license/theheroGAC/PSV-Cleaner)
![GitHub issues](https://img.shields.io/github/issues/theheroGAC/PSV-Cleaner)
![GitHub stars](https://img.shields.io/github/stars/theheroGAC/PSV-Cleaner?style=social)

**PSV Cleaner** is an advanced maintenance utility and temporary files cleaner designed specifically for the PlayStation Vita homebrew community. This tool helps you keep your console organized and recover valuable storage space by safely removing caches, crash dumps, and unneeded leftovers from uninstalled applications.

**Latest Version: 1.18** – Multi-Language (EN, IT, ES, FR, DE, PT, JP), Dynamic UI Themes (Dark Slate, Light, OLED Black, Vita Blue), Real Preview Filtering & Multi-Select, Real App Names (`param.sfo`), Interactive Storage Analyzer Drill-Down, Large File & Archive Finder, TaiHEN Plugin Auditor, Orphan Save ZIP Backup, Persistent Whitelist & Cleanup History!

---

## Key Features

- **Storage Analyzer** – Full partition overview for `ux0:`, `ur0:`, `uma0:`, and `imc0:`. Inspect capacity, used/free space, detailed directory categories (`app`, `data`, `patch`, `addcont`, `license`, `tai`, `temp`), and top largest folders.
- **Smart Clean** – Intelligent, multi-category maintenance mode. Scans system caches, crash dumps, and temp files with an interactive pre-clean preview and comprehensive post-cleanup metrics report.
- **Orphan Scanner** – Identifies abandoned directories and files from previously uninstalled games, patches, DLCs, and homebrews across `ux0:data/`, `ux0:patch/`, `ux0:addcont/`, `ux0:appmeta/`, and `ux0:license/`.
- **Multi-Tier Safety Engine (`psv_cleaner_safety`)** – Every path is validated and categorized (`SAFE`, `REGENERABLE`, `ORPHANED`, `REVIEW`, `PROTECTED`). System volumes (`os0:`, `vs0:`), plugin directories (`tai/`), active game assets, and user save data (`ux0:user/00/savedata/`) are strictly protected and immune from deletion.
- **Tools Menu (L Trigger)** – Dedicated tools launcher accessible directly from the dashboard to easily open the Storage Analyzer, Smart Clean, or Orphan Scanner.
- **Emergency Stop** – Safely interrupt cleaning or scanning operations at any time ($\Delta$ or $\bigcirc$) with immediate, non-blocking responsiveness.
- **Asynchronous & Non-Blocking** – All scans and deletions run on dedicated background threads with cooperative scheduling, keeping the UI rendering at a fluid 60 FPS with zero console freezes even on 512GB+ storages.
- **Classic Cleaning Modes** – Includes Quick Clean, Complete Clean, Selective Clean (custom toggles), and Per-App Cleaning.

---

## What It Cleans

### System & Operating Environment
- System temporary files, caches, and runtime buffers  
- System logs, error reports, and SQLite temporary databases  
- System update, package install, and patch temporary files  
- Shader compilation logs and runtime dumps  

### Crash & Core Dumps
- Core dump files (`psp2core`, `psp2dmp`, `.psp2dmp.tmp`)  
- Adrenaline PSP emulator crash dumps and logs  

### Package Installers & File Managers
- VitaShell file manager cache, trash bin, and temporary archives  
- PKGi / PKGj temporary downloads, package buffers, and image caches  
- Package installation temp folders and app metadata fragments  

### Emulators & Homebrew Applications
- RetroArch emulator cache, runtime logs, and shader cache  
- VitaGrafix, reF00D, and NoNpDrm caches  
- Autoplugin 1 & 2 cache and installation logs  
- PSVshell, SaveManager, VitaCheat, rinCheat, and TropHAX logs  
- Henkaku, TAI, and 0syscall6 caches  
- Moonlight streaming caches and temporary icons  
- PCSX ReARMed, mGBA, and Flycast logs and temporary caches  

### Media & Web
- Browser history, WebKit cache, and cookies  
- Picture, video, and music thumbnails (regenerable)  
- Network temporary files and download buffers  

### Orphaned Data (Residual Files)
- Leftover data folders of uninstalled games in `ux0:data/`  
- Leftover game patches in `ux0:patch/` and DLC expansions in `ux0:addcont/`  
- Orphaned `.rif` license files and unused app metadata  

---

## Controls & Usage

### Startup Mode Select
When launching the app, you are directly presented with the **Mode Selector**:
* **Quick Clean**: Fast removal of caches and logs.
* **Complete Clean**: Deep cleanup of all temp files, crash dumps, and system caches.
* **Selective Clean**: Custom toggle of categories.
* **Smart Clean**: Multi-category intelligent scan with space preview and reports.
* **Orphan Scanner**: Detection and cleanup of uninstalled games & DLC data.
* **Storage Analyzer**: Full visual partition and large folders explorer.
* **X**: Launch selected mode | **O**: Close selector and access Main Dashboard.

### Main Dashboard
* **□**: Reopen **Mode Select** menu.
* **X**: Start scan & cleaning preview.
* **△**: Access selective cleaning options, UI Theme & Language settings.
* **SEL**: Open Per-App cleaning menu (with real game/homebrew names).
* **L**: Open the dedicated **Tools Menu**.
* **START**: Open **About & Controls**.
* **O**: Exit PSV Cleaner.

### Tools Menu (`L` Trigger)
1. **Storage Analyzer**:
   * **← / → (or L / R)**: Switch active storage volume (`ux0:`, `ur0:`, `uma0:`, `imc0:`) at root.
   * **↑ / ↓ / Left Stick**: Navigate directory explorer.
   * **X**: Drill down into directory | **O**: Up one directory / Back | **□**: Rescan | **△**: Delete safe item.
2. **Smart Clean**:
   * Automatic scan across system partitions.
   * Review the **Preview Screen** (Temporary, Cache, Crash Dumps, Orphaned space).
   * Press **X** to execute cleanup or **O** to cancel.
   * Review final metrics on the **Completion Report**.
3. **Orphan Scanner**:
   * Scans for orphaned folders and files left by deleted games across `ux0:`, `uma0:`, and `imc0:` (including `rePatch/` and `reAddcont/`).
   * **↑ / ↓**: Navigate through orphan entries | **L / R**: Fast Page Up / Down.
   * **X**: Toggle selection | **□**: Select all eligible items.
   * **SEL**: Backup orphaned savedata to ZIP (with optional delete) or add to Whitelist.
   * **△ / START**: Confirm and delete selected orphans.
   * **O**: Back to Tools Menu.
4. **Large File Finder**:
   * Scans partitions for large files (`>= 10/50/100/250 MB`) and leftover `.vpk`/`.pkg`/`.zip`/`.7z`/`.rar`/`.cso` archives.
   * **X**: Toggle | **□**: Cycle threshold | **△ / START**: Delete selected | **SEL**: Add to Whitelist.
5. **TaiHEN Auditor**:
   * Checks `ur0:tai/` and `ux0:tai/` conflicts, missing plugin paths in `config.txt`, and unused/backup `.skprx`/`.suprx` files.
   * **X**: Select unused/backup file | **□**: Backup `config.txt` | **△**: Delete selected | **R**: Rescan.
6. **Cleanup History & Whitelist Manager**:
   * Review persistent cleanup sessions (`history.log`), lifetime recovered space, and manage protected paths/Title IDs (`whitelist.txt`).

### Emergency Stop
* Press **$\Delta$** or **$\bigcirc$** during any scanning or deletion operation to safely halt the process immediately.

---

## Safety First

✅ **Multi-Tier Classification** – Automatically discriminates safe cache from files requiring user review.  
✅ **Zero Data Loss Guarantee** – Savedata (`ux0:user/00/savedata/`) and active games are strictly protected.  
✅ **System Volume Immunity** – Hardware and OS partitions (`os0:`, `vs0:`, `vd0:`, `sa0:`) cannot be deleted.  
✅ **Live Preview Before Action** – See the exact number of files and recovered gigabytes before approving.  
✅ **Non-Destructive Scanning** – Read-only scans ensure zero accidental modifications during analysis.  

---

## Technical Details

- **Platform:** PlayStation Vita / PlayStation TV (Henkaku/Enso)
- **Engine:** vita2d with hardware-accelerated GXM rendering
- **Thread Architecture:** Asynchronous worker threads with thread-safe request queues
- **Scan Walker:** Bounded iterative slot traversal (512 slots) preventing stack overflows
- **Safety Module:** Dynamic path inspection with case-insensitive Fat/exFAT normalization
- **Storage Compatibility:** Supports internal storage (`imc0:`), official Sony memory cards, and SD2Vita / USB storage (`ux0:`, `uma0:`, `ur0:`)

---

## Building from Source

Requirements: [VitaSDK](https://vitasdk.org) and CMake + Ninja under MSYS2.

```bash
export VITASDK='C:/msys64/usr/local/vitasdk'
export TMP='C:/msys64/tmp' TEMP='C:/msys64/tmp' TMPDIR='C:/msys64/tmp'
export PATH="/c/msys64/mingw64/bin:/c/msys64/usr/local/vitasdk/bin:$PATH"

cd PSV-Cleaner
rm -rf build && mkdir build && cd build
cmake -G Ninja -DCMAKE_TOOLCHAIN_FILE='C:/msys64/usr/local/vitasdk/share/vita.toolchain.cmake' ..
ninja
```

The build produces `build/PSV_Cleaner.vpk` (ready to install on PS Vita) and `build/eboot.bin`.

---

## Installation

1. Download the latest `PSV_Cleaner.vpk` from the Releases section.
2. Transfer it to your PS Vita and install it using **VitaShell**.
3. Launch **PSV Cleaner** from the LiveArea.

---

## Credits

### Original Creator & Lead Developer
* **theheroGAC** – Project founder and primary developer of PSV Cleaner.

### Development Team & Contributors
* **theheroGAC** – Core development and maintenance.
* **PS Vita Homebrew Community** – Beta testing, feedback, suggestions, and ongoing support.

### Technical Infrastructure & Special Acknowledgments
* **The VitaSDK Team & Contributors** – For providing and maintaining the open-source VitaSDK toolchain.
* **Original HEN Creators** (Yifan Lu, Davee, Proxima, Team Molecule) – For opening the door to modern PS Vita homebrew.
* **Rinnegatamante** – For vita2d and pioneering essential PS Vita libraries and tools.

---

## License

This software is freely distributable for all purposes and uses, provided that all credits to the original creator **theheroGAC**, VitaSDK developers, and the PS Vita homebrew community are preserved.
