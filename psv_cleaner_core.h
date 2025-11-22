#ifndef PSV_CLEANER_CORE_H
#define PSV_CLEANER_CORE_H

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <psp2/apputil.h>

// Define RGBA macro for color values
#define RGBA(r,g,b,a) ((a) << 24 | (r) << 16 | (g) << 8 | (b))

// Safe buffer sizes to prevent overflow
#define MAX_PATH_LENGTH 1024
#define MAX_FILENAME_LENGTH 256
#define MAX_FILE_FILTER_LENGTH 32

// Safe string functions to prevent buffer overflows
#define safe_strncpy(dest, src, size) do { \
    if (size > 0) { \
        dest[0] = '\0'; \
        size_t len = strlen(src); \
        if (len < size) { \
            memcpy(dest, src, len); \
            dest[len] = '\0'; \
        } else if (size > 1) { \
            memcpy(dest, src, size - 1); \
            dest[size - 1] = '\0'; \
        } \
    } \
} while(0)

#define safe_strncat(dest, src, dest_size) do { \
    if (dest_size > 0 && strlen(dest) < dest_size - 1) { \
        size_t remaining = dest_size - strlen(dest) - 1; \
        size_t copy_len = strlen(src); \
        if (copy_len > remaining) copy_len = remaining; \
        memcpy(dest + strlen(dest), src, copy_len); \
        dest[strlen(dest) + copy_len] = '\0'; \
    } \
} while(0)

// Safe sprintf wrapper
static inline int safe_snprintf(char *buffer, size_t size, const char *format, ...) {
    if (!buffer || size == 0) return -1;

    va_list args;
    va_start(args, format);
    int result = vsnprintf(buffer, size, format, args);
    va_end(args);

    return result;
}

// Path validation function
static inline int is_safe_path(const char *path) {
    if (!path) return 0;

    // Check for null bytes and other potentially dangerous characters
    size_t len = strlen(path);
    if (len == 0 || len >= MAX_PATH_LENGTH) return 0;

    // Check for directory traversal attempts
    if (strstr(path, "..") != NULL) return 0;

    // Check for absolute path requirements if needed
    if (path[0] != 'u' && path[0] != 'm') return 0;

    return 1;
}

typedef enum {
    SORT_BY_NAME = 0,
    SORT_BY_SIZE = 1
} SortMode;

extern const char *TEMP_PATHS[];
extern const size_t TEMP_PATHS_COUNT;

typedef struct {
    char path[MAX_PATH_LENGTH];
    unsigned long long size;
} FileInfo;

typedef struct {
    FileInfo *files;
    int count;
    int capacity;
    unsigned long long totalSize;
} FileList;

// Global counter for deleted files
extern int g_deletedFilesCount;

// Emergency stop system
extern int g_emergencyStop;
extern int g_operationInProgress;

// Exclusion settings
extern int excludePictureFolder;
extern int excludeVpkFiles;
extern int excludeVitaDBCache;

// Selective app cleaning settings
extern int cleanVitaShell;
extern int cleanRetroArch;
extern int cleanAdrenaline;
extern int cleanBrowser;
extern int cleanSystem;
extern int cleanOrphanedData;

// Cleanup counter functionality
int loadCleanupCounter();
void saveCleanupCounter(int count);

// Function declarations
unsigned long long calculateTempSize();
unsigned long long calculateTempSizeRecursive(const char *path);
unsigned long long cleanTemporaryFiles();
int getDeletedFilesCount();
void resetDeletedFilesCount();
void deleteRecursive(const char *path);
void forceDeleteDumpFiles();
void aggressiveDumpCleanup();

// Orphaned data cleanup functions
void getInstalledAppsList(char ***apps, int *count);
int isAppInstalled(const char *title_id);
void findOrphanedDataDirectories();
void findOrphanedLicenseDirectories();
void findOrphanedPatchDirectories();
unsigned long long calculateOrphanedDataSize();

// Preview functions
FileList* createFileList();
void freeFileList(FileList *list);
void addFileToList(FileList *list, const char *path, unsigned long long size);
void scanFilesForPreview(FileList *list);
void sortFileList(FileList *list, SortMode sortMode);
void filterAndSortFileList(FileList *list, SortMode sortMode, const char *fileFilter, unsigned long long *totalVisibleSize);
int deleteSingleFileFromList(FileList *list, int index);

// Emergency stop system functions
void initEmergencyStop();
void startOperation();
void endOperation();
void requestEmergencyStop();
int isEmergencyStopRequested();
int isOperationInProgress();
void cleanupAfterEmergencyStop();

#endif // PSV_CLEANER_CORE_H

// Localization and themes module
#define MAX_LANGUAGES 1
#define LANGUAGE_EN 0

typedef enum {
    THEME_LIGHT = 0,
    THEME_DARK = 1
} AppTheme;

// Theme colors
typedef struct {
    int bg_primary;
    int bg_secondary;
    int text_primary;
    int text_secondary;
    int accent;
    int success;
    int border;
} ThemeColors;

// Current language and theme
extern int currentLanguage;
extern AppTheme currentTheme;

// Theme definitions
extern ThemeColors themes[2];

// Language arrays - fixed declarations
extern const char* lang_titles_screen[MAX_LANGUAGES][10];
extern const char* lang_profile_options[MAX_LANGUAGES][6];
extern const char* lang_ui_text[MAX_LANGUAGES][20];



// Functions
const char* L(const char* lang_array[][20], int lang_index, int text_index);
void detectSystemLanguage();
void setTheme(AppTheme theme);
void applyThemeColors(ThemeColors* colors);

// Language detection info
#define SCE_SYSTEM_PARAM_ID_LANG 0x00000005
