#ifndef PSV_CLEANER_CORE_H
#define PSV_CLEANER_CORE_H

#include <stddef.h>
#include <psp2/apputil.h>

// Define RGBA macro for color values
#define RGBA(r,g,b,a) ((a) << 24 | (r) << 16 | (g) << 8 | (b))

// List of temporary folders to clean
extern const char *TEMP_PATHS[];
extern const size_t TEMP_PATHS_COUNT;

// Structure for file preview
typedef struct {
    char path[512];
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

// Preview functions
FileList* createFileList();
void freeFileList(FileList *list);
void addFileToList(FileList *list, const char *path, unsigned long long size);
void scanFilesForPreview(FileList *list);

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
