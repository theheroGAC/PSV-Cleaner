# Changelog

All notable changes to PSV Cleaner will be documented in this file.

## [1.03] - 2025-10-04

### Added
- **Cleaning Profiles System** - Three intelligent cleaning modes
  - **Quick Profile** - Only safe cache files (VitaShell, PKGi, RetroArch, Browser, etc.)
  - **Complete Profile** - All temporary files and system cleanup (default mode)
  - **Selective Profile** - User manually chooses which categories to clean

- **Profile Selection Screen** - Elegant initial screen at app startup
  - Navigate profiles with D-Pad Up/Down
  - Detailed descriptions for each profile mode
  - One-time selection that configures the entire session

- **Cleanup Counter System** - Persistent statistics tracking
  - Saves total number of cleaning sessions
  - Displays "Cleanup #X" on completion screen
  - Persistent storage in ux0:data/PSV_Cleaner/counter.txt

- **Additional Cleaning Paths** - Extended coverage
  - ux0:download/ and ux0:downloads/ complete directory scanning
  - Enhanced download temporary file detection

### Improved
- **User Interface Centering** - Professional text positioning
  - All main titles perfectly centered on screen
  - Consistent button alignment and spacing
  - Improved visual hierarchy

- **Profile-Based Menu Initialization** - Dynamic menu configuration
  - Menu options automatically set based on selected profile
  - Quick mode: only cache categories enabled by default
  - Complete mode: all cleaning options enabled
  - Selective mode: nothing pre-selected for manual choice

- **Navigation Experience** - Enhanced user flow
  - **Square Button**: Change cleaning profile anytime
  - **Seamless switching** between profile modes
  - **Persistent profile memory** during session

### Technical
- Added PROFILE_QUICK, PROFILE_COMPLETE, PROFILE_SELECTIVE enum system
- Dynamic menu initialization with profile parameter
- Persistent counter file management
- Enhanced download path coverage (now 95 cleaning paths total)
- Improved text positioning calculations for perfect centering

---

## [1.02] - 2025-10-01

### Added
- **File Preview System** - View all files that will be deleted before cleaning with detailed list
  - Scrollable file list supporting thousands of files
  - Individual file sizes displayed
  - Total files count and size summary
  - Navigate with D-Pad Up/Down
  - Confirm with X or cancel with O
  
- **Enhanced File Detection** - Added 40+ new cleaning paths
  - VitaGrafix cache and logs
  - reF00D cache
  - NoNpDrm temporary files
  - 0syscall6 cache
  - TAI plugin system cache
  - PSVshell logs and cache
  - SaveManager logs
  - VitaCheat, rinCheat, TropHAX logs
  - WebKit cache and localstorage
  - Network temporary files
  - SQLite temporary databases
  - Package installation temp files
  - Media thumbnails (picture, video, music)
  - RetroArch shader cache
  - Background download temp files
  - And more...

- **Statistics Display** - Detailed completion screen showing:
  - Total space freed
  - Number of files deleted
  - Visual success indicator

- **Dump File Detection** - Special scanning for crash dump files
  - Detects .psp2dmp files in ux0:data/
  - Detects .psp2core files in ux0:data/
  - Shows in preview list before deletion

### Improved
- **Recursive Space Calculation** - Fixed space calculation to scan subdirectories
  - Now correctly calculates all temporary files in nested folders
  - More accurate space estimation before cleaning
  
- **Auto-Refresh After Cleaning** - Space available automatically recalculates after each cleaning session
  - No more stuck at zero after cleaning
  - Fresh calculation on every app launch

- **Memory Management** - Dynamic file list with automatic expansion
  - Efficient memory allocation for large file lists
  - Proper cleanup to prevent memory leaks

### Fixed
- Space calculation showing zero after cleaning
- Missing files in subdirectories during scan
- Crash dump files not appearing in preview

### Technical
- Total cleaning paths increased from 54 to 94
- Added global file counter for deletion tracking
- Improved recursive directory scanning algorithm
- Enhanced preview system with scrollbar for large lists

---

## [1.01] - Previous Release

### Features
- Basic temporary file cleaning
- VitaShell, PKGi, RetroArch cache cleaning
- Progress bar with percentage
- GPU-optimized rendering
- Options menu for selective cleaning

---

## [1.00] - Initial Release

### Features
- First public release
- Clean system temporary files
- Clean homebrew cache and logs
- Simple UI with progress indication
