# Changelog

All notable changes to PSV Cleaner will be documented in this file.

## [1.05] - 2025-11-02

### Added
- **Intelligent Caching System** - Revolutionary performance improvement for file scanning
  - **Smart Cache Storage** - Saves scan results to avoid rescanning unchanged directories
  - **Directory Change Detection** - Automatically detects when directories have been modified
  - **Cache Expiration** - 24-hour cache validity with automatic refresh
  - **Instant Loading** - Up to 5x faster startup times for subsequent app launches
  - **Memory Efficient** - Optimized cache structure with validation and error handling

- **Code Optimization & Cleanup** - Streamlined codebase for better maintainability
  - **Removed Legacy Code** - Eliminated CloneDVD sound references and unused functions
  - **Splash Screen Removal** - Removed unnecessary splash screen for faster app startup
  - **Comment Cleanup** - Removed obsolete comments and improved code documentation
  - **Resource Optimization** - Eliminated unused assets and reduced package size

### Performance Improvements
- **Intelligent Scanning** - Cache-based directory scanning with change detection
- **Faster Startup** - Reduced initialization time through caching system
- **Memory Optimization** - Better memory management for large file operations
- **I/O Reduction** - Minimized disk access through intelligent caching

### Technical Enhancements
- **Cache Management** - Automatic cache invalidation after cleaning operations
- **Error Handling** - Robust cache validation and fallback mechanisms
- **Version Control** - Cache versioning for future compatibility
- **Resource Cleanup** - Proper cleanup of cache files and temporary resources

### UI/UX Improvements
- **Faster Response** - Instant app responsiveness through caching
- **Cleaner Interface** - Removed visual clutter and unnecessary elements
- **Improved Performance** - Smoother operation with reduced loading times

---

## [1.04] - 2025-10-13

### Added
- **Emergency Stop System** - Safe interruption of cleaning operations
  - **Real-time Stop** - Press Circle button during cleaning to stop safely
  - **Operation Tracking** - Monitors active operations and cleanup state
  - **Emergency Screen** - Clear feedback when operation is interrupted
  - **Partial Results Display** - Shows statistics of files actually deleted before interruption
  - **Safe Cleanup** - Proper resource cleanup when stopping mid-operation
  - **Visual Feedback** - Progress bar shows "Stopping operation..." when stopping

- **Enhanced User Safety** - Improved operation control
  - **Graceful Interruption Handling** - Clear user feedback during emergency stops
  - **System State Preservation** - Recovery options after emergency stop
  - **Informative Emergency Screen** - Details about what happened during interruption

### Technical
- **Emergency Stop Flags** - Global flags for operation state tracking
- **Safe Operation Lifecycle** - Start/end/cleanup operation management functions
- **Improved Progress Bar** - Visual feedback for emergency stop requests
- **Resource Cleanup Functions** - Proper cleanup when operations are interrupted

### UI & UX Improvements
- **Operation Management Indicators** - Better handling of long-running operations
- **Emergency Stop Request Handling** - Safe state transitions during interruptions
- **User Confirmation for Interrupted Operations** - Clear messaging about operation status

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
