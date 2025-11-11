# PSV Cleaner Changelog

## Version 1.07 - November 11, 2025

Big update! Added a ton of new features that make cleaning way more useful and fun.

### New Stuff
- **Smart Preview Mode**: You can now see all files before deleting them, sort by name or size, and filter by file type (tmp, log, cache, etc.). Super helpful for knowing exactly what you're cleaning!
- **VitaDB Support**: Added full support for cleaning VitaDB's cache and temp files. You can choose to exclude VitaDB cache if you want to keep it fast.
- **Better Exclusions**: Added options to exclude picture folders and VPK files from cleaning. More control over what gets deleted.

### Improved Controls
- **Preview Screen**: Use △ to sort files, □ to filter by type, D-Pad to navigate, X to clean, O to cancel
- **Menu Scrolling**: Long menus now scroll properly with a visual scrollbar
- **Better Feedback**: Everything feels more responsive and clear
- Expanded cleanup to cover even more temp files and caches

---

## Version 1.06 - November 5, 2025

This was a big cleanup and improvement release. Made the app faster and more reliable.

### What Changed
- **Fixed VPK File Deletion**: Now actually deletes .vpk files properly from the root directory
- **Better UI**: Fixed button labels and made the interface clearer
- **More Cleanup**: Added support for RetroArch temp files, crash dumps, screenshots, and package temp files
- **Animations**: Added particle effects to the progress bar - makes waiting more fun
- **Smart Caching**: The app remembers previous scans to start up faster next time
- **Emergency Stop**: You can safely stop cleaning mid-operation with the O button

### Performance
- Much faster scanning thanks to the caching system
- Better memory management for large file operations
- Cleaner codebase with no unnecessary code

---

## Version 1.05 - November 2, 2025

Focused on making the app faster and more efficient.

### Key Changes
- **Smart Caching System**: The app now saves scan results and only rescans when directories change. Startup is way faster!
- **Code Cleanup**: Removed old unused code and comments to make everything cleaner
- **Performance**: Up to 5x faster loading on subsequent runs
- **Memory**: Better memory usage and cleanup

---

## Version 1.04 - October 13, 2025

Added safety features for long cleaning operations.

### New Features
- **Emergency Stop**: Press the O button during cleaning to safely stop the operation
- **Better Feedback**: Clear messages when operations are interrupted
- **Safe Interruption**: The app handles stops gracefully without corrupting anything

---

## Version 1.02 - October 1, 2025

Major expansion of what the app can clean!

### Big Additions
- **File Preview**: See exactly what files will be deleted before you clean them
- **Way More Cleanup Paths**: Added 40+ new locations to clean, including:
  - VitaGrafix, reF00D, NoNpDrm caches
  - WebKit and network temp files
  - Media thumbnails and RetroArch shaders
  - SQLite databases and package files
- **Statistics**: Shows how much space you freed and how many files were deleted
- **Crash Dump Detection**: Finds and shows .psp2dmp and .psp2core files

### Fixes
- Space calculation now works correctly in subdirectories
- Auto-refreshes space available after cleaning
- Better memory management for large file lists

---

## Version 1.01 - Previous Release

Basic cleaning functionality with progress indication.

---

## Version 1.00 - Initial Release

First version! Could clean basic temp files and had a simple interface.
