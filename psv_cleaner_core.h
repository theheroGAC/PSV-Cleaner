#ifndef PSV_CLEANER_CORE_H
#define PSV_CLEANER_CORE_H

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <psp2/apputil.h>
#include <psp2/system_param.h>
#include <psp2/rtc.h>
#include "psv_cleaner_safety.h"

#define RGBA(r,g,b,a) ((a) << 24 | (r) << 16 | (g) << 8 | (b))

#define MAX_PATH_LENGTH 1024
#define MAX_FILENAME_LENGTH 256
#define MAX_FILE_FILTER_LENGTH 32

static inline void safe_strncpy(char *dest, const char *src, size_t size) {
    if (!dest || size == 0) return;
    if (!src) {
        dest[0] = '\0';
        return;
    }
    size_t i = 0;
    while (i < size - 1 && src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

#define safe_strncat(dest, src, dest_size) do { \
    size_t dest_len = strlen(dest); \
    if (dest_size > 0 && dest_len < dest_size - 1) { \
        size_t remaining = dest_size - dest_len - 1; \
        size_t copy_len = strlen(src); \
        if (copy_len > remaining) copy_len = remaining; \
        memcpy(dest + dest_len, src, copy_len); \
        dest[dest_len + copy_len] = '\0'; \
    } \
} while(0)

static inline int safe_snprintf(char *buffer, size_t size, const char *format, ...) {
    if (!buffer || size == 0) return -1;

    va_list args;
    va_start(args, format);
    int result = vsnprintf(buffer, size, format, args);
    va_end(args);

    return result;
}

static inline int is_safe_path(const char *path) {
    if (!path) return 0;

    size_t len = strlen(path);
    if (len == 0 || len >= MAX_PATH_LENGTH) return 0;

    if (strstr(path, "..") != NULL) return 0;
    if (strstr(path, "./") != NULL) return 0;
    if (strstr(path, "/.") != NULL && strstr(path, "/./") == NULL && strstr(path, "/..") == NULL) return 0;

    for (size_t i = 0; i < len; i++) {
        if (path[i] == '\0') return 0;
    }

    const char *colon = strchr(path, ':');
    if (!colon || colon == path) return 0;
    if (colon - path > 8) return 0;

    const char *invalid_chars = "<>\"|?*";
    for (size_t i = 0; i < len; i++) {
        if (strchr(invalid_chars, path[i]) != NULL) return 0;
    }
    if (strrchr(path, ':') != colon) return 0;

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
    unsigned char selected;
} FileInfo;

typedef struct {
    FileInfo *files;
    int count;
    int capacity;
    unsigned long long totalSize;
} FileList;

typedef struct {
    char titleId[10];
    char appName[64];
    unsigned long long tempSize;
} AppInfo;

typedef struct {
    AppInfo *apps;
    int count;
    int capacity;
} AppList;

extern int g_deletedFilesCount;

extern int g_emergencyStop;
extern int g_operationInProgress;

extern unsigned long long g_cachedSpaceSize;
extern int g_spaceCalculationNeeded;
extern int g_lastCalculationFrame;

extern int g_scanProgress;
extern int g_totalScanItems;
extern int g_currentScanItem;

extern int excludePictureFolder;
extern int excludeVpkFiles;
extern int excludeVitaDBCache;
extern int excludeVideoFolder;

extern int cleanVitaShell;
extern int cleanRetroArch;
extern int cleanAdrenaline;
extern int cleanBrowser;
extern int cleanSystem;
extern int cleanOrphanedData;
extern int cleanAllAppsTempFiles;
extern int cleanPkgi;
extern int cleanAutoplugin;
extern int cleanCrashDumps;

extern int cleanEasyVpK;
extern int cleanDaemon;
extern int cleanVitaGrafix;
extern int cleanOnemenu;
extern int cleanPCSX;
extern int cleanMGBA;
extern int cleanFlycast;
extern int cleanShellbat;
extern int cleanSwitchUser;
extern int cleanITLS;
extern int cleanVHBB;
extern int cleanPSVitaDB;
extern int cleanDownloadEnabler;
extern int cleanMoonlight;
extern int cleanRetroFlow;
extern int cleanHenkaku;
extern int cleanPSVshell;
extern int cleanCheatTools;

extern int cleanThemeCache;
extern int cleanNotificationCache;
extern int cleanActivityLog;
extern int cleanPhotoMusicCache;
extern int cleanSceShellCache;
extern int cleanFontCache;
extern int cleanRegistryTemp;
extern int cleanNetworkCache;
extern int cleanLicenseCache;
extern int cleanOrphanedLicenseFiles;
extern int cleanOrphanedDLC;
extern int cleanOrphanedAddcont;
extern int cleanEmptyLiveareaBubbles;
extern int cleanRePatchOrphans;

int loadCleanupCounter();
void saveCleanupCounter(int count);

unsigned long long calculateTempSize();
unsigned long long calculateTempSizeOptimized();
unsigned long long calculateTempSizeRecursive(const char *path);
unsigned long long cleanTemporaryFiles();
unsigned long long cleanSelectedPreviewFiles(FileList *list);
unsigned long long cleanFilteredPreviewFiles(FileList *list, const char *fileFilter, const char *searchQuery);
void invalidateSpaceCache();
void updateSpaceCacheIfNeeded(int currentFrame);
int getDeletedFilesCount();
void resetDeletedFilesCount();
void incrementDeletedFilesCount();
void deleteRecursive(const char *path);
void forceDeleteDumpFiles();
void aggressiveDumpCleanup();
void clearScanCache();

void getInstalledAppsList(char ***apps, int *count);
int isAppInstalled(const char *title_id);
int isTitleIdInstalled(const char *titleId);
int isSystemDataDirName(const char *dirName);
int isOlderThanDays(const SceDateTime *mtime, int days);
void findOrphanedDataDirectories();
void findOrphanedLicenseDirectories();
void findOrphanedPatchDirectories();
unsigned long long calculateOrphanedDataSize();

unsigned long long calculateAllAppsTempFilesSize();
unsigned long long cleanAllAppsTempFilesData();
void scanAppTempFilesForPreview(FileList *list, const char *titleId);

AppList* createAppList();
void freeAppList(AppList *list);
void populateAppListWithSizes(AppList *list);
void sortAppList(AppList *list, SortMode sortMode);
unsigned long long calculateSingleAppTempFilesSize(const char *titleId);
unsigned long long cleanSingleAppTempFiles(const char *titleId);
int shouldCleanPath(const char *path);

unsigned long long calculateOrphanedDLCDataSize();
void findOrphanedDLCData();
unsigned long long calculateOrphanedAddcontSize();
void findOrphanedAddcont();
unsigned long long calculateOrphanedLicenseFilesSize();
unsigned long long calculateOrphanedLicenseDirsSize();
unsigned long long calculateOrphanedPatchDirsSize();
void findOrphanedLicenseFiles();
unsigned long long calculateEmptyLiveareaBubblesSize();
void removeEmptyLiveareaBubbles();

FileList* createFileList();
void freeFileList(FileList *list);
void addFileToList(FileList *list, const char *path, unsigned long long size);
void scanFilesForPreview(FileList *list);
void sortFileList(FileList *list, SortMode sortMode);
int matchesFileFilter(const char *filename, const char *filter);
int matchesFileSearch(const char *path, const char *searchQuery);
void filterAndSortFileList(FileList *list, SortMode sortMode, const char *fileFilter, unsigned long long *totalVisibleSize);
int deleteSingleFileFromList(FileList *list, int index);

void initEmergencyStop();
void startOperation();
void endOperation();
void requestEmergencyStop();
int isEmergencyStopRequested();
int isOperationInProgress();
void cleanupAfterEmergencyStop();

void initScanProgress(int totalItems);
void updateScanProgress(int currentItem);
int getScanProgress();
int getLastProgressPercent();
void resetScanProgress();

typedef void (*ProgressCallback)(int percent);
extern ProgressCallback g_progressCallback;

typedef enum {
    BG_TASK_IDLE = 0,
    BG_TASK_CALC_SIZE = 1,
    BG_TASK_SCAN_PREVIEW = 2,
    BG_TASK_SCAN_APPS = 3,
    BG_TASK_CLEAN = 4,
    BG_TASK_CLEAN_APP = 5,
    BG_TASK_ANALYZE_VOLUME = 6,
    BG_TASK_SMART_SCAN = 7,
    BG_TASK_SMART_CLEAN = 8,
    BG_TASK_ORPHAN_SCAN = 9,
    BG_TASK_ORPHAN_DELETE = 10,
    BG_TASK_ANALYZE_DIR = 11,
    BG_TASK_LARGE_SCAN = 12,
    BG_TASK_LARGE_DELETE = 13,
    BG_TASK_TAI_AUDIT = 14,
    BG_TASK_BACKUP_SAVE = 15,
    BG_TASK_BACKUP_DELETE_SAVE = 16,
    BG_TASK_CLEAN_PREVIEW_SUBSET = 17
} BgTask;

extern volatile int g_bgTask;
extern volatile int g_bgTaskDone;
extern FileList *g_bgPreviewList;
extern AppList *g_bgAppList;
extern char g_bgCleanAppTitleId[16];
extern volatile unsigned long long g_bgSpaceFreed;
extern SortMode g_bgSortMode;
extern char g_bgFileFilter[MAX_FILE_FILTER_LENGTH];
extern char g_bgSearchQuery[64];
extern int g_bgCleanSubsetMode;
extern unsigned long long g_bgVisibleSize;
extern char g_bgTargetPath[MAX_PATH_LENGTH];
extern char g_bgTargetTitleId[16];
extern int g_bgLargeMinMB;
extern int g_bgLastActionOk;
extern char g_bgLastActionMsg[128];
extern char g_bgActiveModeLabel[32];

void loadSettings();
void saveSettings();

void startBgWorker();
void stopBgWorker();
void requestBgTask(BgTask task);
int isBgBusy();
void waitBgIdle();

#define ANALYZER_MAX_VOLUMES 4
#define ANALYZER_MAX_CATEGORIES 9
#define ANALYZER_TOP_DIRS 16
#define ANALYZER_DIR_ENTRIES 256
#define ANALYZER_MAX_DEPTH 8
#define ORPHAN_MAX_ITEMS 192

typedef struct {
    char name[16];
    unsigned long long size;
} AnalyzerCategory;

typedef struct {
    char name[48];
    unsigned long long size;
} AnalyzerTopDir;

typedef struct {
    char name[128];
    char fullPath[MAX_PATH_LENGTH];
    unsigned long long size;
    int isDir;
    int deletable;
} AnalyzerDirEntry;

typedef struct {
    char volume[8];
    int available;
    int partial;
    unsigned long long totalSize;
    unsigned long long freeSize;
    unsigned long long usedSize;
    int usedPercent;
    int categoryCount;
    AnalyzerCategory categories[ANALYZER_MAX_CATEGORIES];
    int topDirCount;
    AnalyzerTopDir topDirs[ANALYZER_TOP_DIRS];
} AnalyzerVolumeState;

extern AnalyzerVolumeState g_analyzerVolumes[ANALYZER_MAX_VOLUMES];
extern volatile int g_analyzerVolumeCount;
extern volatile int g_analyzerActiveVolume;
extern char g_analyzerBrowsePath[MAX_PATH_LENGTH];
extern int g_analyzerPathDepth;
extern AnalyzerDirEntry g_analyzerDirEntries[ANALYZER_DIR_ENTRIES];
extern volatile int g_analyzerDirEntryCount;
extern volatile int g_analyzerDirTruncated;

void analyzerPrepare();
void analyzerScanVolume(int index);

extern SmartCleanPlan g_smartCleanPlan;
extern CleanScanStats g_smartCleanStats;
extern CleanReport g_cleanReport;

void smartCleanReset();
void smartCleanScan();
void smartCleanRun();

extern SafetyItem g_orphanItems[ORPHAN_MAX_ITEMS];
extern volatile int g_orphanItemCount;
extern volatile int g_orphanTruncated;
extern volatile int g_orphanSkipped;
extern volatile int g_orphanProtected;

void orphanScanRun();
void orphanDeleteSelected();

void maintenanceInit();

#define MAX_LANGUAGES 7
#define LANGUAGE_AUTO -1
#define LANGUAGE_EN 0
#define LANGUAGE_IT 1
#define LANGUAGE_ES 2
#define LANGUAGE_FR 3
#define LANGUAGE_DE 4
#define LANGUAGE_PT 5
#define LANGUAGE_JP 6

#define MAX_THEMES 4

typedef enum {
    THEME_DARK = 0,
    THEME_LIGHT = 1,
    THEME_OLED = 2,
    THEME_VITA = 3
} AppTheme;

typedef struct {
    int bg_primary;
    int bg_secondary;
    int bg_glow1;
    int bg_glow2;
    int card;
    int card_solid;
    int card_dark;
    int text_primary;
    int text_secondary;
    int text_faint;
    int accent;
    int accent_dim;
    int accent_soft;
    int success;
    int danger;
    int warning;
    int border;
    int row_alt;
    int selected;
} ThemeColors;

extern int currentLanguage;
extern int languageMode;
extern AppTheme currentTheme;

extern ThemeColors themes[MAX_THEMES];

extern const char* lang_titles_screen[MAX_LANGUAGES][15];
extern const char* lang_profile_options[MAX_LANGUAGES][6];
extern const char* lang_ui_text[MAX_LANGUAGES][30];

const char* L(const char* lang_array[][30], int lang_index, int text_index);
void detectSystemLanguage();
void setLanguageMode(int mode);
const char* getLanguageName(int lang);
void setTheme(AppTheme theme);
const char* getThemeName(AppTheme theme);
void applyThemeColors(ThemeColors* colors);

#endif
