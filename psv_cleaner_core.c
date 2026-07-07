#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/rtc.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "psv_cleaner_core.h"

SceRtcTick rtcTick;
int g_deletedFilesCount = 0;
int g_emergencyStop = 0;
int g_operationInProgress = 0;

ProgressCallback g_progressCallback = NULL;

unsigned long long g_cachedSpaceSize = 0;
int g_spaceCalculationNeeded = 1;
int g_lastCalculationFrame = 0;
#define CALCULATION_INTERVAL_FRAMES 60

// Scanning progress tracking
int g_scanProgress = 0;
int g_totalScanItems = 0;
int g_currentScanItem = 0; 

// Exclusion settings
int excludePictureFolder = 0;
int excludeVpkFiles = 0;
int excludeVitaDBCache = 0;
int excludeVideoFolder = 0;

// Selective app cleaning settings
int cleanVitaShell = 1;
int cleanRetroArch = 1;
int cleanAdrenaline = 1;
int cleanBrowser = 1;
int cleanSystem = 1;
int cleanOrphanedData = 0;
int cleanAllAppsTempFiles = 0;

// NEW: Additional homebrew/emulator cleaning settings
int cleanEasyVpK = 1;
int cleanDaemon = 1;
int cleanVitaGrafix = 1;
int cleanOnemenu = 1;
int cleanPCSX = 1;
int cleanMGBA = 1;
int cleanFlycast = 1;
int cleanShellbat = 1;
int cleanSwitchUser = 1;
int cleanITLS = 1;
int cleanVHBB = 1;
int cleanPSVitaDB = 1;
int cleanDownloadEnabler = 1;
int cleanMoonlight = 1;
int cleanRetroFlow = 1;
int cleanHenkaku = 1;
int cleanPSVshell = 1;
int cleanCheatTools = 1;

// NEW: Additional system cleaning categories
int cleanThemeCache = 1;
int cleanNotificationCache = 1;
int cleanActivityLog = 1;
int cleanPhotoMusicCache = 1;
int cleanSceShellCache = 1;
int cleanFontCache = 1;
int cleanRegistryTemp = 1;
int cleanNetworkCache = 1;
int cleanLicenseCache = 1;
int cleanOrphanedLicenseFiles = 0;
int cleanOrphanedDLC = 0;
int cleanOrphanedAddcont = 0;
int cleanEmptyLiveareaBubbles = 0;

#define CACHE_FILE_PATH "ux0:data/PSV_Cleaner/scan_cache.bin"
#define CACHE_VERSION 1
#define CACHE_EXPIRY_HOURS 24
#define MAX_CACHE_ENTRIES 256

typedef struct {
    char path[256];
    SceDateTime lastModified;
    unsigned long long totalSize;
    int fileCount;
    int isValid;
} CacheEntry;

typedef struct {
    int version;
    SceDateTime created;
    CacheEntry entries[MAX_CACHE_ENTRIES];
} ScanCache;

const char *TEMP_PATHS[] = {
    "ux0:temp/",
    "ux0:data/temp/",
    "ux0:cache/",
    "ux0:log/",
    "ur0:temp/",
    "ur0:temp/sqlite/",
    "uma0:temp/",
    
    "ux0:VitaShell/cache/",
    "ux0:VitaShell/temp/",
    "ux0:VitaShell/log/",
    "ux0:VitaShell/recent/",
    "ux0:VitaShell/backup/temp/",
    "ux0:VitaShell/trash/",
    
    "ux0:pkgi/tmp/",
    "ux0:pkgi/cache/",
    "ux0:pkgi/log.txt",
    "ux0:pkgi/downloads/temp/",
    "ux0:pkgi/backup/temp/",
    "ux0:pkgi/*.vpk",
    
    "ux0:pkgj/tmp/",
    "ux0:pkgj/cache/",
    "ux0:pkgj/log.txt",
    "ux0:pkgj/downloads/temp/",
    
    "ux0:*.vpk",
    
    "ux0:data/retroarch/cache/",
    "ux0:data/retroarch/logs/",
    "ux0:data/retroarch/temp/",
    "ux0:data/retroarch/thumbnails/cache/",
    "ux0:data/retroarch/shaders/cache/",
    "ux0:data/retroarch/database/rdb/temp/",
    
    "ux0:pspemu/temp/",
    "ux0:pspemu/cache/",
    
    "ux0:data/Adrenaline/cache/",
    "ux0:data/Adrenaline/logs/",
    "ux0:data/Adrenaline/temp/",
    "ux0:data/Adrenaline/crash/",
    "ux0:data/Adrenaline/dumps/",
    
    "ux0:data/moonlight/cache/",
    "ux0:data/moonlight/logs/",
    
    "ux0:data/AutoPlugin/cache/",
    "ux0:data/AutoPlugin/logs/",
    "ux0:data/AUTOPLUGIN2/cache/",
    "ux0:data/AUTOPLUGIN2/logs/",
    
    "ux0:data/RetroFlow/CACHE/",
    
    "ux0:data/henkaku/cache/",
    "ux0:data/henkaku/logs/",
    
    "ux0:data/PSVshell/logs/",
    "ux0:data/PSVshell/cache/",
    
    "ux0:data/savemgr/log/",
    "ux0:data/vitacheat/logs/",
    "ux0:data/rinCheat/logs/",
    
    "ux0:data/VitaDB/temp.tmp",
    "ux0:vdb_data/",
    "ux0:data/vdb_vpk/",
    
    "ux0:data/browser/cache/",
    "ux0:data/browser/temp/",
    "ux0:data/browser/logs/",
    "ux0:data/webkit/cache/",
    "ux0:data/webkit/localstorage/temp/",
    
    "ux0:data/net/temp/",
    
    "ux0:download/",
    "ux0:download/temp/",
    "ux0:downloads/",
    "ux0:downloads/temp/",
    "ux0:bgdl/t/",
    
    "ux0:data/pkg/temp/",
    "ux0:package/temp/",
    "ux0:appmeta/temp/",
    "ur0:appmeta/temp/",
    "ux0:license/temp/",
    "ux0:patch_temp/",
    "ux0:update_temp/",
    
    "ux0:picture/.thumbnails/",
    "ux0:picture/SCREENSHOT/",
    "ux0:video/.thumbnails/",
    "ux0:music/.cache/",
    "ux0:photo/cache/",
    
    "ux0:shaderlog/",
    
    "ux0:data/logs/temp/",
    "ux0:data/cache/",
    
    "uma0:cache/",
    "uma0:log/",
    "uma0:data/temp/",
    "uma0:data/cache/",
    "uma0:VitaShell/cache/",
    "uma0:VitaShell/temp/",
    "uma0:VitaShell/log/",
    "uma0:pkgi/cache/",
    "uma0:pkgi/log/",
    "uma0:data/retroarch/cache/",
    "uma0:data/retroarch/logs/",
    "uma0:data/retroarch/temp/",
    "uma0:data/Adrenaline/cache/",
    "uma0:data/Adrenaline/logs/",
    "uma0:data/Adrenaline/temp/",
    "uma0:data/moonlight/cache/",
    "uma0:data/moonlight/logs/",
    "uma0:data/AutoPlugin/cache/",
    "uma0:data/AutoPlugin/logs/",
    "uma0:data/AUTOPLUGIN2/cache/",
    "uma0:data/AUTOPLUGIN2/logs/",
    "uma0:data/RetroFlow/CACHE/",
    "uma0:data/henkaku/cache/",
    "uma0:data/henkaku/logs/",
    "uma0:data/PSVshell/logs/",
    "uma0:data/PSVshell/cache/",
    "uma0:data/savemgr/log/",
    "uma0:data/vitacheat/logs/",
    "uma0:data/rinCheat/logs/",
    "uma0:data/browser/cache/",
    "uma0:data/browser/temp/",
    "uma0:data/browser/logs/",
    "uma0:data/webkit/cache/",
    "uma0:data/webkit/localstorage/temp/",
    "uma0:data/net/temp/",
    "uma0:download/",
    "uma0:download/temp/",
    "uma0:downloads/",
    "uma0:downloads/temp/",
    "uma0:bgdl/t/",
    "uma0:data/pkg/temp/",
    "uma0:package/temp/",
    "uma0:appmeta/temp/",
    "uma0:data/logs/temp/",
    
    "ux0:data/psp2core*",
    "ux0:data/*.psp2core",
    "ux0:data/psp2dmp*",
    "ux0:data/*.psp2dmp",
    "ux0:data/crash_dumps/",
    "ux0:data/dumps/",
    "ux0:data/psp2core",
    "ux0:data/psp2dmp",
    
    "ux0:data/mgba/logs/",
    "uma0:data/mgba/cache/",
    "uma0:data/mgba/logs/",
    
    "ux0:data/pcsx_rearmed/logs/",
    "uma0:data/pcsx_rearmed/cache/",
    "uma0:data/pcsx_rearmed/logs/",
    
    "ux0:data/flycast/logs/",
    "ux0:data/flycast/temp/",
    "uma0:data/flycast/cache/",
    "uma0:data/flycast/logs/",
    "uma0:data/flycast/temp/",
    
    "ux0:data/vitaquake/logs/",
    "uma0:data/vitaquake/cache/",
    "uma0:data/vitaquake/logs/",
    
    "ux0:data/EasyVPK/cache/",
    "ux0:data/EasyVPK/temp/",
    
    "ux0:data/DAEMON/logs/",
    "uma0:data/DAEMON/cache/",
    "uma0:data/DAEMON/logs/",
    
    "uma0:data/iTLS/cache/",
    
    "uma0:data/DE/cache/",
    "ux0:data/ONEMenu/cache/",
    "uma0:data/ONEMenu/cache/",
    
    "uma0:data/shellbat/cache/",
    
    "uma0:data/switchuser/cache/",
    
    "uma0:data/PSVitaDB/cache/",
    
    "uma0:data/VitaGrafix/cache/",
    
    "ux0:VHBB/temp/",
    "uma0:VHBB/cache/",
    "uma0:VHBB/temp/",
    
    "ur0:shell/theme/cache/",
    "ux0:data/theme/temp/",
    
    "ur0:license/cache/",
    
    "ur0:config/np/",
    
    "ur0:temp/font/",
    "ux0:data/font/cache/",
    "ux0:data/activity/cache/",
    "ux0:notification/"
};

const size_t TEMP_PATHS_COUNT = sizeof(TEMP_PATHS)/sizeof(TEMP_PATHS[0]);

ThemeColors themes[2] = {
    {
        .bg_primary = RGBA(15, 25, 50, 255),
        .bg_secondary = RGBA(25, 35, 55, 240),
        .text_primary = RGBA(255, 255, 255, 255),
        .text_secondary = RGBA(200, 200, 200, 255),
        .accent = RGBA(100, 200, 255, 255),
        .success = RGBA(100, 255, 100, 255),
        .border = RGBA(100, 200, 255, 255)
    },
    {
        .bg_primary = RGBA(5, 10, 15, 255),
        .bg_secondary = RGBA(15, 20, 30, 240),
        .text_primary = RGBA(255, 255, 255, 255),
        .text_secondary = RGBA(180, 180, 180, 255),
        .accent = RGBA(150, 100, 200, 255),
        .success = RGBA(0, 255, 150, 255),
        .border = RGBA(150, 100, 200, 255)
    }
};

int currentLanguage = LANGUAGE_EN;
AppTheme currentTheme = THEME_LIGHT;

const char* lang_titles_screen[MAX_LANGUAGES][15] = {
    {
        "PSV Cleaner",
        "Temporary Files Cleaner for PS Vita",
        "Select Cleaning Profile",
        "Advanced Cleaning Options",
        "Preview - Files to Delete",
        "Scanning files...",
        "✓ CLEANING COMPLETED! ✓",
        "Great job! Your PS Vita is cleaner!"
    }
};

const char* lang_profile_options[MAX_LANGUAGES][6] = {
    {"Quick Clean", "Safe cache files only", "Complete Clean", "All temporary files", "Selective Clean", "Choose categories"}
};

const char* lang_ui_text[MAX_LANGUAGES][30] = {
    {
        "System Status:", "Ready for Cleanup",
        "Controls:", "Change Profile", "Preview & Clean", "Advanced Options", "Exit App",
        "Space to free:", "Space Freed:", "Files Deleted:",
        "D-Pad: Navigate | X: Select Profile | O: Exit",
        "Cleanup #", "No temporary files found!",
        "System Ready", "Version 1.14"
    }
};

const char* L(const char* lang_array[][30], int lang_index, int text_index) {
    if (lang_index >= 0 && lang_index < MAX_LANGUAGES) {
        return lang_array[lang_index][text_index];
    }
    return lang_array[0][text_index];
}

void detectSystemLanguage() {
    currentLanguage = LANGUAGE_EN;
    currentTheme = THEME_LIGHT;
}

void setTheme(AppTheme theme) {
    currentTheme = theme;
}

void applyThemeColors(ThemeColors* colors) {
    *colors = themes[currentTheme];
}

int getDeletedFilesCount() {
    return g_deletedFilesCount;
}

int loadCleanupCounter() {
    int count = 0;
    SceUID fd = sceIoOpen("ux0:data/PSV_Cleaner/counter.txt", SCE_O_RDONLY, 0777);
    if (fd >= 0) {
        sceIoRead(fd, &count, sizeof(int));
        sceIoClose(fd);
    }
    return count;
}

void saveCleanupCounter(int count) {
    sceIoMkdir("ux0:data/PSV_Cleaner", 0777);
    SceUID fd = sceIoOpen("ux0:data/PSV_Cleaner/counter.txt", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (fd >= 0) {
        sceIoWrite(fd, &count, sizeof(int));
        sceIoClose(fd);
    }
}

void initEmergencyStop() {
    g_emergencyStop = 0;
    g_operationInProgress = 0;
}

void startOperation() {
    g_operationInProgress = 1;
    g_emergencyStop = 0;
}

void endOperation() {
    g_operationInProgress = 0;
    g_emergencyStop = 0;
}

void requestEmergencyStop() {
    g_emergencyStop = 1;
}

int isEmergencyStopRequested() {
    return g_emergencyStop;
}

int isOperationInProgress() {
    return g_operationInProgress;
}

void cleanupAfterEmergencyStop() {
    // Reset emergency stop flags
    g_emergencyStop = 0;
    g_operationInProgress = 0;
    
    // Invalidate space cache to ensure fresh calculation after interruption
    invalidateSpaceCache();
    
    // Clear scan cache to prevent stale data
    clearScanCache();
    
    // Reset deleted files counter for accurate tracking
    resetDeletedFilesCount();
    
    // Reset scan progress
    resetScanProgress();
    
    // Ensure all I/O operations are completed
    sceKernelDelayThread(500 * 1000);
    
    
}

void initScanProgress(int totalItems) {
    g_totalScanItems = totalItems;
    g_currentScanItem = 0;
    g_scanProgress = 0;
}

void updateScanProgress(int currentItem) {
    g_currentScanItem = currentItem;
    if (g_totalScanItems > 0) {
        g_scanProgress = (g_currentScanItem * 100) / g_totalScanItems;
    } else {
        g_scanProgress = 0;
    }
}

int getScanProgress() {
    return g_scanProgress;
}

void resetScanProgress() {
    g_scanProgress = 0;
    g_currentScanItem = 0;
    g_totalScanItems = 0;
}

void resetDeletedFilesCount() {
    g_deletedFilesCount = 0;
}

void getCurrentTime(SceDateTime* dt) {
    SceRtcTick tick;
    sceRtcGetCurrentTick(&tick);
    sceRtcSetTick(dt, &tick);
}

int isCacheExpired(SceDateTime* cacheTime) {
    SceDateTime now;
    getCurrentTime(&now);

    SceRtcTick cacheTicks, nowTicks, expiryTicks;
    sceRtcGetTick(cacheTime, &cacheTicks);
    sceRtcGetTick(&now, &nowTicks);

    sceRtcTickAddHours(&expiryTicks, &cacheTicks, CACHE_EXPIRY_HOURS);

    return sceRtcCompareTick(&nowTicks, &expiryTicks) >= 0;
}

ScanCache* loadScanCache() {
    ScanCache* cache = (ScanCache*)malloc(sizeof(ScanCache));
    if (!cache) return NULL;

    SceUID fd = sceIoOpen(CACHE_FILE_PATH, SCE_O_RDONLY, 0777);
    if (fd < 0) {
        free(cache);
        return NULL;
    }

    int readSize = sceIoRead(fd, cache, sizeof(ScanCache));
    sceIoClose(fd);

    if (readSize != sizeof(ScanCache) || cache->version != CACHE_VERSION || isCacheExpired(&cache->created)) {
        free(cache);
        return NULL;
    }

    return cache;
}

void saveScanCache(ScanCache* cache) {
    if (!cache) return;

    sceIoMkdir("ux0:data/PSV_Cleaner", 0777);

    SceUID fd = sceIoOpen(CACHE_FILE_PATH, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (fd >= 0) {
        sceIoWrite(fd, cache, sizeof(ScanCache));
        sceIoClose(fd);
    }
}

int getDirectoryModTime(const char* path, SceDateTime* modTime) {
    SceIoStat stat;
    if (sceIoGetstat(path, &stat) >= 0) {
        *modTime = stat.st_mtime;
        return 1;
    }
    return 0;
}

int hasDirectoryChanged(const char* path, SceDateTime* cachedTime) {
    SceDateTime currentTime;
    if (!getDirectoryModTime(path, &currentTime)) {
        return 1;
    }

    SceRtcTick cachedTicks, currentTicks;
    sceRtcGetTick(cachedTime, &cachedTicks);
    sceRtcGetTick(&currentTime, &currentTicks);

    return sceRtcCompareTick(&currentTicks, &cachedTicks) > 0;
}

CacheEntry createCacheEntry(const char* path) {
    CacheEntry entry;
    memset(&entry, 0, sizeof(CacheEntry));
    safe_strncpy(entry.path, path, sizeof(entry.path));

    if (getDirectoryModTime(path, &entry.lastModified)) {
        entry.totalSize = calculateTempSizeRecursive(path);
        entry.isValid = 1;
    } else {
        entry.isValid = 0;
    }

    return entry;
}

void clearScanCache() {
    sceIoRemove(CACHE_FILE_PATH);
}

void forceDeleteDumpFiles() {
    const char* dumpPatterns[] = {
        "ux0:data/psp2core",
        "ux0:data/psp2dmp",
        "ux0:data/psp2core.*",
        "ux0:data/psp2dmp.*",
        "ux0:data/*.psp2core",
        "ux0:data/*.psp2dmp"
    };

    for (int i = 0; i < 6; i++) {
        for (int attempt = 0; attempt < 3; attempt++) {
            sceIoRemove(dumpPatterns[i]);
            sceKernelDelayThread(100 * 1000);
        }
    }

    const char* dumpDirs[] = {
        "ux0:data/crash_dumps/",
        "ux0:data/dumps/",
        "ux0:data/Adrenaline/dumps/",
        "ux0:data/Adrenaline/crash/"
    };

    for (int i = 0; i < 4; i++) {
        deleteRecursive(dumpDirs[i]);
    }
}

void aggressiveDumpCleanup() {
    const char* searchPaths[] = {
        "ux0:data/",
        "ux0:temp/",
        "ux0:cache/",
        "ux0:log/"
    };

    for (int path = 0; path < 4; path++) {
        SceUID dfd = sceIoDopen(searchPaths[path]);
        if (dfd >= 0) {
            SceIoDirent dir;
            memset(&dir, 0, sizeof(SceIoDirent));

            while (sceIoDread(dfd, &dir) > 0) {
                char* filename = dir.d_name;
                int len = strlen(filename);
                if ((len > 8 && strcmp(filename + len - 8, ".psp2dmp") == 0) ||
                    (len > 12 && strcmp(filename + len - 12, ".psp2dmp.tmp") == 0)) {
                    char fullPath[MAX_PATH_LENGTH];
                    safe_snprintf(fullPath, sizeof(fullPath), "%s%s", searchPaths[path], filename);

                    for (int attempt = 0; attempt < 5; attempt++) {
                        if (sceIoRemove(fullPath) >= 0) {
                            g_deletedFilesCount++;
                            break;
                        }
                        sceKernelDelayThread(200 * 1000);
                    }
                }
            }
            sceIoDclose(dfd);
        }
    }
}

void cleanupVpkFiles() {
    SceUID dfd = sceIoDopen("ux0:/");
    if (dfd >= 0) {
        SceIoDirent dir;
        memset(&dir, 0, sizeof(SceIoDirent));

        while (sceIoDread(dfd, &dir) > 0) {
            if (SCE_S_ISDIR(dir.d_stat.st_mode)) continue;

            char* filename = dir.d_name;
            int len = strlen(filename);
            if (len > 4 && (strcmp(filename + len - 4, ".vpk") == 0 || strcmp(filename + len - 4, ".pkg") == 0)) {
                char fullPath[512];
                snprintf(fullPath, sizeof(fullPath), "ux0:/%s", filename);

                if (sceIoRemove(fullPath) >= 0) {
                    g_deletedFilesCount++;
                }
            }
        }
        sceIoDclose(dfd);
    }
}

void deleteRecursive(const char *path) {
    if (isEmergencyStopRequested()) return;
    SceUID dfd;
    dfd = sceIoDopen(path);
    if (dfd >= 0) {
        SceIoDirent *dir = malloc(sizeof(SceIoDirent));
        if (!dir) {
            sceIoDclose(dfd);
            return;
        }

        while (sceIoDread(dfd, dir) > 0) {
            if (isEmergencyStopRequested()) break;
            if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
                continue;

            char *newPath = malloc(MAX_PATH_LENGTH);
            if (!newPath) continue;

            snprintf(newPath, MAX_PATH_LENGTH, "%s%s%s", path,
                     (path[strlen(path)-1] == '/') ? "" : "/",
                     dir->d_name);

            if (SCE_S_ISDIR(dir->d_stat.st_mode)) {
                deleteRecursive(newPath);
                sceIoRmdir(newPath);
            } else {
                if (sceIoRemove(newPath) >= 0) {
                    g_deletedFilesCount++;
                }
            }
            free(newPath);
        }
        sceIoDclose(dfd);
        sceIoRmdir(path);
        free(dir);
    } else {
        if (sceIoRemove(path) >= 0) {
            g_deletedFilesCount++;
        }
    }
}

unsigned long long calculateTempSizeRecursive(const char *path) {
    unsigned long long total = 0;
    SceUID dfd;
    dfd = sceIoDopen(path);
    if (dfd >= 0) {
        SceIoDirent *dir = malloc(sizeof(SceIoDirent));
        if (!dir) {
            sceIoDclose(dfd);
            return 0;
        }

        while (sceIoDread(dfd, dir) > 0) {
            if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
                continue;

            char *newPath = malloc(MAX_PATH_LENGTH);
            if (!newPath) continue;

            snprintf(newPath, MAX_PATH_LENGTH, "%s%s%s", path,
                     (path[strlen(path)-1] == '/') ? "" : "/",
                     dir->d_name);

            if (SCE_S_ISDIR(dir->d_stat.st_mode)) {
                total += calculateTempSizeRecursive(newPath);
            } else {
                total += dir->d_stat.st_size;
            }
            free(newPath);
        }
        sceIoDclose(dfd);
        free(dir);
    } else {
        SceIoStat stat;
        if (sceIoGetstat(path, &stat) >= 0) {
            if (SCE_S_ISREG(stat.st_mode)) {
                total += stat.st_size;
            }
        }
    }

    return total;
}

unsigned long long calculateTempSizeOptimized() {
    if (g_spaceCalculationNeeded) {
        g_cachedSpaceSize = calculateTempSize();
        g_spaceCalculationNeeded = 0;
    }
    return g_cachedSpaceSize;
}

void invalidateSpaceCache() {
    g_spaceCalculationNeeded = 1;
}

void updateSpaceCacheIfNeeded(int currentFrame) {
    if (g_spaceCalculationNeeded || 
        (currentFrame - g_lastCalculationFrame) > CALCULATION_INTERVAL_FRAMES) {
        g_cachedSpaceSize = calculateTempSize();
        g_spaceCalculationNeeded = 0;
        g_lastCalculationFrame = currentFrame;
    }
}

int shouldCleanPath(const char *path) {
    if (excludePictureFolder && strncmp(path, "ux0:picture/", 12) == 0) return 0;
    if (excludeVideoFolder && strncmp(path, "ux0:video/", 10) == 0) return 0;
    if (excludeVpkFiles && (strstr(path, ".vpk") != NULL || strstr(path, ".pkg") != NULL)) return 0;
    if (excludeVitaDBCache && strncmp(path, "ux0:data/VitaDB/", 17) == 0) return 0;

    if (strstr(path, "VitaShell/")) return cleanVitaShell;
    if (strstr(path, "retroarch/")) return cleanRetroArch;
    if (strstr(path, "Adrenaline/") || strstr(path, "pspemu/")) return cleanAdrenaline;
    if (strstr(path, "browser/") || strstr(path, "webkit/")) return cleanBrowser;
    
    if (strstr(path, "EasyVPK/")) return cleanEasyVpK;
    if (strstr(path, "DAEMON/")) return cleanDaemon;
    if (strstr(path, "VitaGrafix/")) return cleanVitaGrafix;
    if (strstr(path, "ONEMenu/")) return cleanOnemenu;
    if (strstr(path, "pcsx_rearmed/")) return cleanPCSX;
    if (strstr(path, "mgba/")) return cleanMGBA;
    if (strstr(path, "flycast/")) return cleanFlycast;
    if (strstr(path, "shellbat/")) return cleanShellbat;
    if (strstr(path, "switchuser/")) return cleanSwitchUser;
    if (strstr(path, "PSVitaDB/")) return cleanPSVitaDB;
    if (strstr(path, "iTLS/")) return cleanITLS;
    if (strstr(path, "DE/")) return cleanDownloadEnabler;
    if (strstr(path, "VHBB/")) return cleanVHBB;
    if (strstr(path, "moonlight/")) return cleanMoonlight;
    if (strstr(path, "RetroFlow/")) return cleanRetroFlow;
    if (strstr(path, "henkaku/") || strstr(path, "tai/")) return cleanHenkaku;
    if (strstr(path, "PSVshell/")) return cleanPSVshell;
    if (strstr(path, "vitacheat/") || strstr(path, "rinCheat/") || strstr(path, "savemgr/")) return cleanCheatTools;
    
    if (strstr(path, "theme/")) return cleanThemeCache;
    if (strstr(path, "font/")) return cleanFontCache;
    if (strstr(path, "notification/")) return cleanNotificationCache;
    if (strstr(path, "activity/")) return cleanActivityLog;
    if (strstr(path, "picture/") || strstr(path, "photo/") || strstr(path, "music/") || strstr(path, "video/")) return cleanPhotoMusicCache;
    if (strstr(path, "net/")) return cleanNetworkCache;
    if (strstr(path, "license/cache/") || strstr(path, "license/")) return cleanLicenseCache;

    if (strstr(path, "temp/") || strstr(path, "cache/") || strstr(path, "log/") || 
        strstr(path, "AutoPlugin/") || strstr(path, "AUTOPLUGIN2/") ||
        strstr(path, "bgdl/t/") || strstr(path, "package/temp/") ||
        strstr(path, "appmeta/temp/") || strstr(path, "patch_temp/") ||
        strstr(path, "update_temp/") || strstr(path, "shaderlog/") ||
        strstr(path, "np/")) {
        return cleanSystem;
    }

    return 1;
}

unsigned long long calculateTempSize() {
    unsigned long long total = 0;
    ScanCache* cache = loadScanCache();
    ScanCache* newCache = NULL;

    if (!cache) {
        newCache = (ScanCache*)malloc(sizeof(ScanCache));
        if (newCache) {
            memset(newCache, 0, sizeof(ScanCache));
            newCache->version = CACHE_VERSION;
            getCurrentTime(&newCache->created);
        }
    }

    initScanProgress(TEMP_PATHS_COUNT);

    for(size_t i = 0; i < TEMP_PATHS_COUNT; i++){
        if (isEmergencyStopRequested()) break;
        
        if (!shouldCleanPath(TEMP_PATHS[i])) {
            updateScanProgress(i + 1);
            continue;
        }

        unsigned long long pathSize = 0;

        if (cache && i < MAX_CACHE_ENTRIES && cache->entries[i].isValid && !hasDirectoryChanged(TEMP_PATHS[i], &cache->entries[i].lastModified)) {
            pathSize = cache->entries[i].totalSize;
        } else {
            pathSize = calculateTempSizeRecursive(TEMP_PATHS[i]);

            if (newCache && i < MAX_CACHE_ENTRIES) {
                newCache->entries[i] = createCacheEntry(TEMP_PATHS[i]);
            }
        }

        total += pathSize;
        updateScanProgress(i + 1);
    }

    if (newCache) {
        saveScanCache(newCache);
        free(newCache);
    }

    if (cache) {
        free(cache);
    }

    total += calculateOrphanedDataSize();
    total += calculateAllAppsTempFilesSize();
    total += calculateOrphanedDLCDataSize();
    total += calculateOrphanedAddcontSize();
    total += calculateOrphanedLicenseFilesSize();
    total += calculateEmptyLiveareaBubblesSize();

    return total;
}

unsigned long long cleanTemporaryFiles() {
    resetDeletedFilesCount();

    if (g_progressCallback) g_progressCallback(5);
    forceDeleteDumpFiles();
    if (g_progressCallback) g_progressCallback(7);
    aggressiveDumpCleanup();
    if (g_progressCallback) g_progressCallback(10);

    if (!excludeVpkFiles) {
        cleanupVpkFiles();
    }
    if (g_progressCallback) g_progressCallback(12);

    for(size_t i=0;i<TEMP_PATHS_COUNT;i++){
        if (isEmergencyStopRequested()) break;
        
        if (!shouldCleanPath(TEMP_PATHS[i])) {
            if (g_progressCallback) {
                int pct = 15 + (i * 70) / TEMP_PATHS_COUNT;
                g_progressCallback(pct);
            }
            continue;
        }
        deleteRecursive(TEMP_PATHS[i]);
        
        if (g_progressCallback) {
            int pct = 15 + (i * 70) / TEMP_PATHS_COUNT;
            g_progressCallback(pct);
        }
    }

    if (g_progressCallback) g_progressCallback(82);
    forceDeleteDumpFiles();
    aggressiveDumpCleanup();
    if (g_progressCallback) g_progressCallback(85);

    if (!excludeVpkFiles) {
        cleanupVpkFiles();
    }
    if (g_progressCallback) g_progressCallback(87);

    if (cleanOrphanedData) {
        findOrphanedDataDirectories();
    }
    if (g_progressCallback) g_progressCallback(90);

    if (cleanAllAppsTempFiles) {
        cleanAllAppsTempFilesData();
    }
    if (g_progressCallback) g_progressCallback(93);

    // NEW orphan cleanups
    if (cleanOrphanedDLC) {
        findOrphanedDLCData();
    }
    if (g_progressCallback) g_progressCallback(95);
    if (cleanOrphanedAddcont) {
        findOrphanedAddcont();
    }
    if (g_progressCallback) g_progressCallback(96);
    if (cleanOrphanedLicenseFiles) {
        findOrphanedLicenseFiles();
    }
    if (g_progressCallback) g_progressCallback(97);
    if (cleanEmptyLiveareaBubbles) {
        removeEmptyLiveareaBubbles();
    }
    if (g_progressCallback) g_progressCallback(98);
    if (cleanOrphanedData) {
        findOrphanedLicenseDirectories();
        findOrphanedPatchDirectories();
    }
    if (g_progressCallback) g_progressCallback(99);

    sceIoRemove(CACHE_FILE_PATH);
    if (g_progressCallback) g_progressCallback(100);
    return 0;
}

FileList* createFileList() {
    FileList *list = (FileList*)malloc(sizeof(FileList));
    if (!list) return NULL;

    list->capacity = 100;
    list->count = 0;
    list->totalSize = 0;
    list->files = (FileInfo*)malloc(sizeof(FileInfo) * list->capacity);

    if (!list->files) {
        free(list);
        return NULL;
    }

    return list;
}

void freeFileList(FileList *list) {
    if (list) {
        if (list->files) {
            free(list->files);
        }
        free(list);
    }
}

void addFileToList(FileList *list, const char *path, unsigned long long size) {
    if (!list || !path) return;

    if (list->count >= list->capacity) {
        list->capacity *= 2;
        FileInfo *newFiles = (FileInfo*)realloc(list->files, sizeof(FileInfo) * list->capacity);
        if (!newFiles) return;
        list->files = newFiles;
    }

    safe_strncpy(list->files[list->count].path, path, sizeof(list->files[list->count].path));
    list->files[list->count].size = size;
    list->totalSize += size;
    list->count++;
}

void scanPathForPreview(FileList *list, const char *path) {
    SceUID dfd;
    SceIoDirent dir;
    memset(&dir, 0, sizeof(SceIoDirent));

    dfd = sceIoDopen(path);
    if (dfd >= 0) {
        while (sceIoDread(dfd, &dir) > 0) {
            if (strcmp(dir.d_name, ".") == 0 || strcmp(dir.d_name, "..") == 0)
                continue;

            char newPath[1024];
            snprintf(newPath, sizeof(newPath), "%s%s%s", path,
                     (path[strlen(path)-1] == '/') ? "" : "/",
                     dir.d_name);

            if (SCE_S_ISDIR(dir.d_stat.st_mode)) {
                scanPathForPreview(list, newPath);
            } else {
                addFileToList(list, newPath, dir.d_stat.st_size);
            }
        }
        sceIoDclose(dfd);
    } else {
        SceIoStat stat;
        if (sceIoGetstat(path, &stat) >= 0) {
            if (!SCE_S_ISDIR(stat.st_mode)) {
                addFileToList(list, path, stat.st_size);
            }
        }
    }
}

int compareFilesByName(const void *a, const void *b) {
    const FileInfo *fileA = (const FileInfo *)a;
    const FileInfo *fileB = (const FileInfo *)b;
    return strcmp(fileA->path, fileB->path);
}

int compareFilesBySize(const void *a, const void *b) {
    const FileInfo *fileA = (const FileInfo *)a;
    const FileInfo *fileB = (const FileInfo *)b;
    if (fileA->size > fileB->size) return -1;
    if (fileA->size < fileB->size) return 1;
    return 0;
}

void sortFileList(FileList *list, SortMode sortMode) {
    if (!list || list->count <= 1) return;

    switch (sortMode) {
        case SORT_BY_NAME:
            qsort(list->files, list->count, sizeof(FileInfo), compareFilesByName);
            break;
        case SORT_BY_SIZE:
            qsort(list->files, list->count, sizeof(FileInfo), compareFilesBySize);
            break;
    }
}

int matchesFileFilter(const char *filename, const char *filter) {
    if (!filter || strlen(filter) == 0) return 1;

    char *ext = strrchr(filename, '.');
    if (!ext) return 0;

    return strcasecmp(ext + 1, filter) == 0;
}

void filterAndSortFileList(FileList *list, SortMode sortMode, const char *fileFilter, unsigned long long *totalVisibleSize) {
    if (!list) return;

    sortFileList(list, sortMode);

    if (totalVisibleSize) {
        *totalVisibleSize = 0;
        for (int i = 0; i < list->count; i++) {
            char *filename = strrchr(list->files[i].path, '/');
            if (filename) filename++;
            else filename = list->files[i].path;

            if (matchesFileFilter(filename, fileFilter)) {
                *totalVisibleSize += list->files[i].size;
            }
        }
    }
}

int deleteSingleFileFromList(FileList *list, int index) {
    if (!list || index < 0 || index >= list->count) {
        return 0;
    }

    if (sceIoRemove(list->files[index].path) >= 0) {
        unsigned long long removedSize = list->files[index].size;

        for (int i = index; i < list->count - 1; i++) {
            list->files[i] = list->files[i + 1];
        }

        list->count--;
        list->totalSize -= removedSize;

        g_deletedFilesCount++;

        return 1;
    }

    return 0;
}

// Orphaned data cleanup functions
void getInstalledAppsList(char ***apps, int *count) {
    *count = 0;
    *apps = NULL;

    SceUID dfd = sceIoDopen("ux0:app/");
    if (dfd < 0) return;

    SceIoDirent dir;
    memset(&dir, 0, sizeof(SceIoDirent));

    while (sceIoDread(dfd, &dir) > 0) {
        if (!SCE_S_ISDIR(dir.d_stat.st_mode)) continue;

        if (strlen(dir.d_name) != 9) continue; 

        (*count)++;
        *apps = (char **)realloc(*apps, *count * sizeof(char *));
        (*apps)[*count - 1] = strdup(dir.d_name);
    }

    sceIoDclose(dfd);
}

int isAppInstalled(const char *title_id) {
    if (!title_id || strlen(title_id) != 9) {
        return 0;
    }

    char path[MAX_PATH_LENGTH];
    safe_snprintf(path, sizeof(path), "ux0:app/%s", title_id);
    SceIoStat stat;
    return sceIoGetstat(path, &stat) >= 0;
}

void findOrphanedDataDirectories() {
    const char *dataRoots[] = {"ux0:data/"};
    const int dataRootsCount = 1;

    for (int root = 0; root < dataRootsCount; root++) {
        SceUID dfd = sceIoDopen(dataRoots[root]);
        if (dfd < 0) continue;

        SceIoDirent dir;
        memset(&dir, 0, sizeof(SceIoDirent));

        while (sceIoDread(dfd, &dir) > 0) {
            if (!SCE_S_ISDIR(dir.d_stat.st_mode)) continue;

            char dirName[MAX_FILENAME_LENGTH];
            safe_strncpy(dirName, dir.d_name, sizeof(dirName));

            // Skip known system directories that are not app data
            if (strcmp(dirName, "Adrenaline") == 0 ||
                strcmp(dirName, "AutoPlugin") == 0 ||
                strcmp(dirName, "AUTOPLUGIN2") == 0 ||
                strcmp(dirName, "browser") == 0 ||
                strcmp(dirName, "RetroFlow") == 0 ||
                strcmp(dirName, "VitaDB") == 0 ||
                strcmp(dirName, "henkaku") == 0 ||
                strcmp(dirName, "moonlight") == 0 ||
                strcmp(dirName, "PSP2SHELL") == 0 ||
                strcmp(dirName, "PSVshell") == 0 ||
                strcmp(dirName, "RetroArch") == 0 ||
                strcmp(dirName, "VitaShell") == 0 ||
                strcmp(dirName, "savemgr") == 0 ||
                strcmp(dirName, "vitacheat") == 0 ||
                strcmp(dirName, "rinCheat") == 0 ||
                strcmp(dirName, "webkit") == 0 ||
                strcmp(dirName, "net") == 0 ||
                strcmp(dirName, "pkg") == 0 ||
                strcmp(dirName, "PSV_Cleaner") == 0 ||
                strcmp(dirName, "logs") == 0 ||
                strcmp(dirName, "cache") == 0) {
                continue;
            }

            if (strlen(dirName) == 9 && !isAppInstalled(dirName)) {
                char fullPath[512];
                snprintf(fullPath, sizeof(fullPath), "%s%s", dataRoots[root], dirName);

                SceIoStat stat;
                if (sceIoGetstat(fullPath, &stat) >= 0) {
                    SceDateTime now, modTime;
                    getCurrentTime(&now);
                    modTime = stat.st_mtime;

                    SceRtcTick nowTicks, modTicks;
                    sceRtcGetTick(&now, &nowTicks);
                    sceRtcGetTick(&modTime, &modTicks);


                    SceRtcTick thresholdTicks;
                    sceRtcTickAddDays(&thresholdTicks, &modTicks, 30);

                    if (sceRtcCompareTick(&nowTicks, &thresholdTicks) >= 0) {
                        deleteRecursive(fullPath);
                    }
                }
            }
        }
        sceIoDclose(dfd);
    }
}

unsigned long long calculateOrphanedDataSize() {
    unsigned long long total = 0;
    if (!cleanOrphanedData) return 0;

    const char *dataRoots[] = {"ux0:data/"};
    const int dataRootsCount = 1;

    for (int root = 0; root < dataRootsCount; root++) {
        SceUID dfd = sceIoDopen(dataRoots[root]);
        if (dfd < 0) continue;

        SceIoDirent dir;
        memset(&dir, 0, sizeof(SceIoDirent));

        while (sceIoDread(dfd, &dir) > 0) {
            if (!SCE_S_ISDIR(dir.d_stat.st_mode)) continue;

            char dirName[MAX_FILENAME_LENGTH];
            safe_strncpy(dirName, dir.d_name, sizeof(dirName));

            if (strcmp(dirName, "Adrenaline") == 0 ||
                strcmp(dirName, "AutoPlugin") == 0 ||
                strcmp(dirName, "AUTOPLUGIN2") == 0 ||
                strcmp(dirName, "browser") == 0 ||
                strcmp(dirName, "RetroFlow") == 0 ||
                strcmp(dirName, "VitaDB") == 0 ||
                strcmp(dirName, "henkaku") == 0 ||
                strcmp(dirName, "moonlight") == 0 ||
                strcmp(dirName, "PSP2SHELL") == 0 ||
                strcmp(dirName, "PSVshell") == 0 ||
                strcmp(dirName, "RetroArch") == 0 ||
                strcmp(dirName, "VitaShell") == 0 ||
                strcmp(dirName, "savemgr") == 0 ||
                strcmp(dirName, "vitacheat") == 0 ||
                strcmp(dirName, "rinCheat") == 0 ||
                strcmp(dirName, "webkit") == 0 ||
                strcmp(dirName, "net") == 0 ||
                strcmp(dirName, "pkg") == 0 ||
                strcmp(dirName, "PSV_Cleaner") == 0 ||
                strcmp(dirName, "logs") == 0 ||
                strcmp(dirName, "cache") == 0) {
                continue;
            }

            if (strlen(dirName) == 9 && !isAppInstalled(dirName)) {
                char fullPath[MAX_PATH_LENGTH];
                safe_snprintf(fullPath, sizeof(fullPath), "%s%s", dataRoots[root], dirName);
                total += calculateTempSizeRecursive(fullPath);
            }
        }
        sceIoDclose(dfd);
    }

    return total;
}

void findOrphanedLicenseDirectories() {
    SceUID dfd = sceIoDopen("ux0:license/");
    if (dfd < 0) return;

    SceIoDirent dir;
    memset(&dir, 0, sizeof(SceIoDirent));

    while (sceIoDread(dfd, &dir) > 0) {
        if (isEmergencyStopRequested()) break;
        if (!SCE_S_ISDIR(dir.d_stat.st_mode)) continue;

        char dirName[MAX_FILENAME_LENGTH];
        safe_strncpy(dirName, dir.d_name, sizeof(dirName));

        if (strlen(dirName) == 16 && !isAppInstalled(dirName + 7)) {
            char fullPath[MAX_PATH_LENGTH];
            safe_snprintf(fullPath, sizeof(fullPath), "ux0:license/%s", dirName);

            SceIoStat stat;
            if (sceIoGetstat(fullPath, &stat) >= 0) {
                SceDateTime now, modTime;
                getCurrentTime(&now);
                modTime = stat.st_mtime;

                SceRtcTick nowTicks, modTicks;
                sceRtcGetTick(&now, &nowTicks);
                sceRtcGetTick(&modTime, &modTicks);

                SceRtcTick thresholdTicks;
                sceRtcTickAddDays(&thresholdTicks, &modTicks, 30);

                if (sceRtcCompareTick(&nowTicks, &thresholdTicks) >= 0) {
                    deleteRecursive(fullPath);
                }
            }
        }
    }
    sceIoDclose(dfd);
}

void findOrphanedPatchDirectories() {
    SceUID dfd = sceIoDopen("ux0:patch/");
    if (dfd < 0) return;

    SceIoDirent dir;
    memset(&dir, 0, sizeof(SceIoDirent));

    while (sceIoDread(dfd, &dir) > 0) {
        if (isEmergencyStopRequested()) break;
        if (!SCE_S_ISDIR(dir.d_stat.st_mode)) continue;

        char dirName[MAX_FILENAME_LENGTH];
        safe_strncpy(dirName, dir.d_name, sizeof(dirName));

        if (strlen(dirName) == 9 && !isAppInstalled(dirName)) {
            char fullPath[MAX_PATH_LENGTH];
            safe_snprintf(fullPath, sizeof(fullPath), "ux0:patch/%s", dirName);

            SceIoStat stat;
            if (sceIoGetstat(fullPath, &stat) >= 0) {
                SceDateTime now, modTime;
                getCurrentTime(&now);
                modTime = stat.st_mtime;

                SceRtcTick nowTicks, modTicks;
                sceRtcGetTick(&now, &nowTicks);
                sceRtcGetTick(&modTime, &modTicks);

                SceRtcTick thresholdTicks;
                sceRtcTickAddDays(&thresholdTicks, &modTicks, 30);

                if (sceRtcCompareTick(&nowTicks, &thresholdTicks) >= 0) {
                    deleteRecursive(fullPath);
                }
            }
        }
    }
    sceIoDclose(dfd);
}

void scanFilesForPreview(FileList *list) {
    if (!list) return;

    for(size_t i=0; i<TEMP_PATHS_COUNT; i++){
        // Apply exclusion settings before scanning
        if (excludePictureFolder && strncmp(TEMP_PATHS[i], "ux0:picture/", 12) == 0) {
            continue;
        }
        
        if (excludeVideoFolder && strncmp(TEMP_PATHS[i], "ux0:video/", 10) == 0) {
            continue;
        }
        
        if (excludeVpkFiles && strstr(TEMP_PATHS[i], ".vpk") != NULL) {
            continue;
        }
        
        if (excludeVitaDBCache && strncmp(TEMP_PATHS[i], "ux0:data/VitaDB/", 17) == 0) {
            continue;
        }
        
        if (!cleanVitaShell && strstr(TEMP_PATHS[i], "VitaShell/")) {
            continue;
        }
        if (!cleanRetroArch && strstr(TEMP_PATHS[i], "retroarch/")) {
            continue;
        }
        if (!cleanAdrenaline && strstr(TEMP_PATHS[i], "Adrenaline/")) {
            continue;
        }
        if (!cleanBrowser && (strstr(TEMP_PATHS[i], "browser/") || strstr(TEMP_PATHS[i], "webkit/"))) {
            continue;
        }
        
        if (!cleanSystem && (strstr(TEMP_PATHS[i], "ux0:temp/") || strstr(TEMP_PATHS[i], "ux0:cache/") || strstr(TEMP_PATHS[i], "ux0:log/") ||
            strstr(TEMP_PATHS[i], "AutoPlugin/") || strstr(TEMP_PATHS[i], "AUTOPLUGIN2/"))) {
            continue;
        }
        if (excludeVideoFolder && strstr(TEMP_PATHS[i], "video/")) continue;
        
        scanPathForPreview(list, TEMP_PATHS[i]);
    }

    const char* dumpSearchPaths[] = {
        "ux0:data/",
        "ux0:temp/",
        "ux0:cache/",
        "ux0:log/"
    };

    for (int path = 0; path < 4; path++) {
        SceUID dfd = sceIoDopen(dumpSearchPaths[path]);
        if (dfd >= 0) {
            SceIoDirent dir;
            memset(&dir, 0, sizeof(SceIoDirent));

            while (sceIoDread(dfd, &dir) > 0) {
                if (SCE_S_ISDIR(dir.d_stat.st_mode)) continue;

                char* filename = dir.d_name;
                int len = strlen(filename);

                int isDumpFile = 0;
                if ((len > 8 && strcmp(filename + len - 8, ".psp2dmp") == 0) ||
                    (len > 12 && strcmp(filename + len - 12, ".psp2dmp.tmp") == 0)) {
                    isDumpFile = 1;
                } else if (len > 9 && strcmp(filename + len - 9, ".psp2core") == 0) {
                    isDumpFile = 1;
                } else if (strcmp(filename, "psp2core") == 0 || strcmp(filename, "psp2dmp") == 0) {
                    isDumpFile = 1;
                }

                if (isDumpFile) {
                    char fullPath[512];
                    snprintf(fullPath, sizeof(fullPath), "%s%s", dumpSearchPaths[path], filename);
                    addFileToList(list, fullPath, dir.d_stat.st_size);
                }
            }
            sceIoDclose(dfd);
        }
    }

    if (!excludeVpkFiles) {
        SceUID vpkDfd = sceIoDopen("ux0:/");
        if (vpkDfd >= 0) {
            SceIoDirent vpkDir;
            memset(&vpkDir, 0, sizeof(SceIoDirent));

            while (sceIoDread(vpkDfd, &vpkDir) > 0) {
                if (SCE_S_ISDIR(vpkDir.d_stat.st_mode)) continue;

                char* filename = vpkDir.d_name;
                int len = strlen(filename);
                if (len > 4 && strcmp(filename + len - 4, ".vpk") == 0) {
                    char fullPath[512];
                    snprintf(fullPath, sizeof(fullPath), "ux0:/%s", filename);
                    addFileToList(list, fullPath, vpkDir.d_stat.st_size);
                }
            }
            sceIoDclose(vpkDfd);
        }
    }

    if (cleanAllAppsTempFiles) {
        char **apps = NULL;
        int appCount = 0;
        getInstalledAppsList(&apps, &appCount);

        if (apps) {
            for (int i = 0; i < appCount; i++) {
                if (isEmergencyStopRequested()) break;
                scanAppTempFilesForPreview(list, apps[i]);
                free(apps[i]);
            }
            free(apps);
        }
    }
}

static int isTempFile(const char *filename) {
    if (!filename) return 0;

    int len = strlen(filename);
    if (len == 0) return 0;

    const char *tempExtensions[] = {".tmp", ".temp", ".log", ".cache", ".bak", ".old", ".swp"};
    int extCount = sizeof(tempExtensions) / sizeof(tempExtensions[0]);

    for (int i = 0; i < extCount; i++) {
        int extLen = strlen(tempExtensions[i]);
        if (len >= extLen && strcasecmp(filename + len - extLen, tempExtensions[i]) == 0) {
            return 1;
        }
    }

    if (strstr(filename, "temp") != NULL || strstr(filename, "tmp") != NULL ||
        strstr(filename, "cache") != NULL || strstr(filename, "log") != NULL) {
        return 1;
    }

    return 0;
}

void scanAppTempFilesForPreview(FileList *list, const char *titleId) {
    if (!list || !titleId || strlen(titleId) != 9) return;

    const char *tempDirs[] = {"cache", "temp", "tmp", "logs", "log"};
    int tempDirCount = sizeof(tempDirs) / sizeof(tempDirs[0]);

    const char *appDirs[] = {
        "ux0:data/%s",
        "ux0:patch/%s",
        "ux0:addcont/%s"
    };
    int appDirCount = sizeof(appDirs) / sizeof(appDirs[0]);

    for (int dirIdx = 0; dirIdx < appDirCount; dirIdx++) {
        char appPath[MAX_PATH_LENGTH];
        safe_snprintf(appPath, sizeof(appPath), appDirs[dirIdx], titleId);

        SceIoStat stat;
        if (sceIoGetstat(appPath, &stat) < 0) continue;
        if (!SCE_S_ISDIR(stat.st_mode)) continue;

        for (int i = 0; i < tempDirCount; i++) {
            char tempDirPath[MAX_PATH_LENGTH];
            safe_snprintf(tempDirPath, sizeof(tempDirPath), "%s/%s", appPath, tempDirs[i]);
            scanPathForPreview(list, tempDirPath);
        }

        SceUID dfd = sceIoDopen(appPath);
        if (dfd >= 0) {
            SceIoDirent dir;
            memset(&dir, 0, sizeof(SceIoDirent));

            while (sceIoDread(dfd, &dir) > 0) {
                if (strcmp(dir.d_name, ".") == 0 || strcmp(dir.d_name, "..") == 0) continue;

                int skipDir = 0;
                for (int i = 0; i < tempDirCount; i++) {
                    if (strcmp(dir.d_name, tempDirs[i]) == 0) {
                        skipDir = 1;
                        break;
                    }
                }
                if (skipDir) continue;

                if (!SCE_S_ISDIR(dir.d_stat.st_mode) && isTempFile(dir.d_name)) {
                    char fullPath[MAX_PATH_LENGTH];
                    safe_snprintf(fullPath, sizeof(fullPath), "%s/%s", appPath, dir.d_name);
                    addFileToList(list, fullPath, dir.d_stat.st_size);
                }
            }
            sceIoDclose(dfd);
        }
    }
}

unsigned long long calculateAllAppsTempFilesSize() {
    if (!cleanAllAppsTempFiles) return 0;

    unsigned long long total = 0;
    char **apps = NULL;
    int appCount = 0;
    getInstalledAppsList(&apps, &appCount);

    if (!apps) return 0;

    const char *tempDirs[] = {"cache", "temp", "tmp", "logs", "log"};
    int tempDirCount = sizeof(tempDirs) / sizeof(tempDirs[0]);

    const char *appDirs[] = {
        "ux0:data/%s",
        "ux0:patch/%s",
        "ux0:addcont/%s"
    };
    int appDirCount = sizeof(appDirs) / sizeof(appDirs[0]);

    for (int i = 0; i < appCount; i++) {
        for (int dirIdx = 0; dirIdx < appDirCount; dirIdx++) {
            char appPath[MAX_PATH_LENGTH];
            safe_snprintf(appPath, sizeof(appPath), appDirs[dirIdx], apps[i]);

            SceIoStat stat;
            if (sceIoGetstat(appPath, &stat) < 0) continue;
            if (!SCE_S_ISDIR(stat.st_mode)) continue;

            for (int j = 0; j < tempDirCount; j++) {
                char tempDirPath[MAX_PATH_LENGTH];
                safe_snprintf(tempDirPath, sizeof(tempDirPath), "%s/%s", appPath, tempDirs[j]);
                total += calculateTempSizeRecursive(tempDirPath);
            }

            SceUID dfd = sceIoDopen(appPath);
            if (dfd >= 0) {
                SceIoDirent dir;
                memset(&dir, 0, sizeof(SceIoDirent));

                while (sceIoDread(dfd, &dir) > 0) {
                    if (strcmp(dir.d_name, ".") == 0 || strcmp(dir.d_name, "..") == 0) continue;

                    int skipDir = 0;
                    for (int j = 0; j < tempDirCount; j++) {
                        if (strcmp(dir.d_name, tempDirs[j]) == 0) {
                            skipDir = 1;
                            break;
                        }
                    }
                    if (skipDir) continue;

                    if (!SCE_S_ISDIR(dir.d_stat.st_mode) && isTempFile(dir.d_name)) {
                        total += dir.d_stat.st_size;
                    }
                }
                sceIoDclose(dfd);
            }
        }

        free(apps[i]);
    }
    free(apps);

    return total;
}

unsigned long long cleanAllAppsTempFilesData() {
    if (!cleanAllAppsTempFiles) return 0;

    unsigned long long totalCleaned = 0;
    char **apps = NULL;
    int appCount = 0;
    getInstalledAppsList(&apps, &appCount);

    if (!apps) return 0;

    const char *tempDirs[] = {"cache", "temp", "tmp", "logs", "log"};
    int tempDirCount = sizeof(tempDirs) / sizeof(tempDirs[0]);

    const char *appDirs[] = {
        "ux0:data/%s",
        "ux0:patch/%s",
        "ux0:addcont/%s"
    };
    int appDirCount = sizeof(appDirs) / sizeof(appDirs[0]);

    for (int i = 0; i < appCount; i++) {
        if (isEmergencyStopRequested()) break;

        for (int dirIdx = 0; dirIdx < appDirCount; dirIdx++) {
            if (isEmergencyStopRequested()) break;

            char appPath[MAX_PATH_LENGTH];
            safe_snprintf(appPath, sizeof(appPath), appDirs[dirIdx], apps[i]);

            SceIoStat stat;
            if (sceIoGetstat(appPath, &stat) < 0) continue;
            if (!SCE_S_ISDIR(stat.st_mode)) continue;

            for (int j = 0; j < tempDirCount; j++) {
                if (isEmergencyStopRequested()) break;

                char tempDirPath[MAX_PATH_LENGTH];
                safe_snprintf(tempDirPath, sizeof(tempDirPath), "%s/%s", appPath, tempDirs[j]);
                
                unsigned long long before = calculateTempSizeRecursive(tempDirPath);
                deleteRecursive(tempDirPath);
                unsigned long long after = calculateTempSizeRecursive(tempDirPath);
                totalCleaned += (before - after);
            }

            SceUID dfd = sceIoDopen(appPath);
            if (dfd >= 0) {
                SceIoDirent dir;
                memset(&dir, 0, sizeof(SceIoDirent));

                while (sceIoDread(dfd, &dir) > 0) {
                    if (isEmergencyStopRequested()) break;

                    if (strcmp(dir.d_name, ".") == 0 || strcmp(dir.d_name, "..") == 0) continue;

                    int skipDir = 0;
                    for (int j = 0; j < tempDirCount; j++) {
                        if (strcmp(dir.d_name, tempDirs[j]) == 0) {
                            skipDir = 1;
                            break;
                        }
                    }
                    if (skipDir) continue;

                    if (!SCE_S_ISDIR(dir.d_stat.st_mode) && isTempFile(dir.d_name)) {
                        char fullPath[MAX_PATH_LENGTH];
                        safe_snprintf(fullPath, sizeof(fullPath), "%s/%s", appPath, dir.d_name);
                        
                        if (sceIoRemove(fullPath) >= 0) {
                            totalCleaned += dir.d_stat.st_size;
                            g_deletedFilesCount++;
                        }
                    }
                }
                sceIoDclose(dfd);
            }
        }

        free(apps[i]);
    }
    free(apps);

    return totalCleaned;
}

AppList* createAppList() {
    AppList *list = (AppList*)malloc(sizeof(AppList));
    if (!list) return NULL;

    list->capacity = 50;
    list->count = 0;
    list->apps = (AppInfo*)malloc(sizeof(AppInfo) * list->capacity);

    if (!list->apps) {
        free(list);
        return NULL;
    }

    return list;
}

void freeAppList(AppList *list) {
    if (list) {
        if (list->apps) {
            free(list->apps);
        }
        free(list);
    }
}

unsigned long long calculateSingleAppTempFilesSize(const char *titleId) {
    if (!titleId || strlen(titleId) != 9) return 0;

    unsigned long long total = 0;
    const char *tempDirs[] = {"cache", "temp", "tmp", "logs", "log"};
    int tempDirCount = sizeof(tempDirs) / sizeof(tempDirs[0]);

    const char *appDirs[] = {
        "ux0:data/%s",
        "ux0:patch/%s",
        "ux0:addcont/%s"
    };
    int appDirCount = sizeof(appDirs) / sizeof(appDirs[0]);

    for (int dirIdx = 0; dirIdx < appDirCount; dirIdx++) {
        char appPath[MAX_PATH_LENGTH];
        safe_snprintf(appPath, sizeof(appPath), appDirs[dirIdx], titleId);

        SceIoStat stat;
        if (sceIoGetstat(appPath, &stat) < 0) continue;
        if (!SCE_S_ISDIR(stat.st_mode)) continue;

        for (int j = 0; j < tempDirCount; j++) {
            char tempDirPath[MAX_PATH_LENGTH];
            safe_snprintf(tempDirPath, sizeof(tempDirPath), "%s/%s", appPath, tempDirs[j]);
            total += calculateTempSizeRecursive(tempDirPath);
        }

        SceUID dfd = sceIoDopen(appPath);
        if (dfd >= 0) {
            SceIoDirent dir;
            memset(&dir, 0, sizeof(SceIoDirent));

            while (sceIoDread(dfd, &dir) > 0) {
                if (strcmp(dir.d_name, ".") == 0 || strcmp(dir.d_name, "..") == 0) continue;

                int skipDir = 0;
                for (int j = 0; j < tempDirCount; j++) {
                    if (strcmp(dir.d_name, tempDirs[j]) == 0) {
                        skipDir = 1;
                        break;
                    }
                }
                if (skipDir) continue;

                if (!SCE_S_ISDIR(dir.d_stat.st_mode) && isTempFile(dir.d_name)) {
                    total += dir.d_stat.st_size;
                }
            }
            sceIoDclose(dfd);
        }
    }

    return total;
}

void populateAppListWithSizes(AppList *list) {
    if (!list) return;

    char **apps = NULL;
    int appCount = 0;
    getInstalledAppsList(&apps, &appCount);

    if (!apps) return;

    list->count = 0;

    for (int i = 0; i < appCount; i++) {
        if (list->count >= list->capacity) {
            list->capacity *= 2;
            AppInfo *newApps = (AppInfo*)realloc(list->apps, sizeof(AppInfo) * list->capacity);
            if (!newApps) break;
            list->apps = newApps;
        }

        safe_strncpy(list->apps[list->count].titleId, apps[i], sizeof(list->apps[list->count].titleId));
        list->apps[list->count].tempSize = calculateSingleAppTempFilesSize(apps[i]);
        list->count++;

        free(apps[i]);
    }
    free(apps);
}

unsigned long long cleanSingleAppTempFiles(const char *titleId) {
    if (!titleId || strlen(titleId) != 9) return 0;

    unsigned long long totalCleaned = 0;
    const char *tempDirs[] = {"cache", "temp", "tmp", "logs", "log"};
    int tempDirCount = sizeof(tempDirs) / sizeof(tempDirs[0]);

    const char *appDirs[] = {
        "ux0:data/%s",
        "ux0:patch/%s",
        "ux0:addcont/%s"
    };
    int appDirCount = sizeof(appDirs) / sizeof(appDirs[0]);

    for (int dirIdx = 0; dirIdx < appDirCount; dirIdx++) {
        if (isEmergencyStopRequested()) break;

        char appPath[MAX_PATH_LENGTH];
        safe_snprintf(appPath, sizeof(appPath), appDirs[dirIdx], titleId);

        SceIoStat stat;
        if (sceIoGetstat(appPath, &stat) < 0) continue;
        if (!SCE_S_ISDIR(stat.st_mode)) continue;

        for (int j = 0; j < tempDirCount; j++) {
            if (isEmergencyStopRequested()) break;

            char tempDirPath[MAX_PATH_LENGTH];
            safe_snprintf(tempDirPath, sizeof(tempDirPath), "%s/%s", appPath, tempDirs[j]);
            
            unsigned long long before = calculateTempSizeRecursive(tempDirPath);
            deleteRecursive(tempDirPath);
            unsigned long long after = calculateTempSizeRecursive(tempDirPath);
            totalCleaned += (before - after);
        }

        SceUID dfd = sceIoDopen(appPath);
        if (dfd >= 0) {
            SceIoDirent dir;
            memset(&dir, 0, sizeof(SceIoDirent));

            while (sceIoDread(dfd, &dir) > 0) {
                if (isEmergencyStopRequested()) break;

                if (strcmp(dir.d_name, ".") == 0 || strcmp(dir.d_name, "..") == 0) continue;

                int skipDir = 0;
                for (int j = 0; j < tempDirCount; j++) {
                    if (strcmp(dir.d_name, tempDirs[j]) == 0) {
                        skipDir = 1;
                        break;
                    }
                }
                if (skipDir) continue;

                if (!SCE_S_ISDIR(dir.d_stat.st_mode) && isTempFile(dir.d_name)) {
                    char fullPath[MAX_PATH_LENGTH];
                    safe_snprintf(fullPath, sizeof(fullPath), "%s/%s", appPath, dir.d_name);
                    
                    if (sceIoRemove(fullPath) >= 0) {
                        totalCleaned += dir.d_stat.st_size;
                        g_deletedFilesCount++;
                    }
                }
            }
            sceIoDclose(dfd);
        }
    }

    return totalCleaned;
}

// ============================================================
// NEW: Orphaned DLC data cleaning (ux0:addcont/ folder)
// DLC for apps that are no longer installed
// ============================================================

unsigned long long calculateOrphanedDLCDataSize() {
    unsigned long long total = 0;
    if (!cleanOrphanedDLC) return 0;

    SceUID dfd = sceIoDopen("ux0:addcont/");
    if (dfd < 0) return 0;

    SceIoDirent dir;
    memset(&dir, 0, sizeof(SceIoDirent));

    while (sceIoDread(dfd, &dir) > 0) {
        if (!SCE_S_ISDIR(dir.d_stat.st_mode)) continue;

        char dirName[MAX_FILENAME_LENGTH];
        safe_strncpy(dirName, dir.d_name, sizeof(dirName));

        if (strlen(dirName) == 9 && !isAppInstalled(dirName)) {
            char fullPath[MAX_PATH_LENGTH];
            safe_snprintf(fullPath, sizeof(fullPath), "ux0:addcont/%s", dirName);
            total += calculateTempSizeRecursive(fullPath);
        }
    }
    sceIoDclose(dfd);

    return total;
}

void findOrphanedDLCData() {
    if (!cleanOrphanedDLC) return;

    SceUID dfd = sceIoDopen("ux0:addcont/");
    if (dfd < 0) return;

    SceIoDirent dir;
    memset(&dir, 0, sizeof(SceIoDirent));

    while (sceIoDread(dfd, &dir) > 0) {
        if (isEmergencyStopRequested()) break;
        if (!SCE_S_ISDIR(dir.d_stat.st_mode)) continue;

        char dirName[MAX_FILENAME_LENGTH];
        safe_strncpy(dirName, dir.d_name, sizeof(dirName));

        if (strlen(dirName) == 9 && !isAppInstalled(dirName)) {
            char fullPath[MAX_PATH_LENGTH];
            safe_snprintf(fullPath, sizeof(fullPath), "ux0:addcont/%s", dirName);
            deleteRecursive(fullPath);
        }
    }
    sceIoDclose(dfd);
}

// ============================================================
// NEW: Orphaned addcont (non-TitleID folders in addcont/)
// ============================================================

unsigned long long calculateOrphanedAddcontSize() {
    unsigned long long total = 0;
    if (!cleanOrphanedAddcont) return 0;

    SceUID dfd = sceIoDopen("ux0:addcont/");
    if (dfd < 0) return 0;

    SceIoDirent dir;
    memset(&dir, 0, sizeof(SceIoDirent));

    while (sceIoDread(dfd, &dir) > 0) {
        if (!SCE_S_ISDIR(dir.d_stat.st_mode)) continue;

        char dirName[MAX_FILENAME_LENGTH];
        safe_strncpy(dirName, dir.d_name, sizeof(dirName));

        if (strlen(dirName) != 9) {
            char fullPath[MAX_PATH_LENGTH];
            safe_snprintf(fullPath, sizeof(fullPath), "ux0:addcont/%s", dirName);
            total += calculateTempSizeRecursive(fullPath);
        }
    }
    sceIoDclose(dfd);

    return total;
}

void findOrphanedAddcont() {
    if (!cleanOrphanedAddcont) return;

    SceUID dfd = sceIoDopen("ux0:addcont/");
    if (dfd < 0) return;

    SceIoDirent dir;
    memset(&dir, 0, sizeof(SceIoDirent));

    while (sceIoDread(dfd, &dir) > 0) {
        if (isEmergencyStopRequested()) break;
        if (!SCE_S_ISDIR(dir.d_stat.st_mode)) continue;

        char dirName[MAX_FILENAME_LENGTH];
        safe_strncpy(dirName, dir.d_name, sizeof(dirName));

        if (strlen(dirName) != 9) {
            char fullPath[MAX_PATH_LENGTH];
            safe_snprintf(fullPath, sizeof(fullPath), "ux0:addcont/%s", dirName);
            deleteRecursive(fullPath);
        }
    }
    sceIoDclose(dfd);
}

// ============================================================
// NEW: Orphaned license files (*.rif) - licenses for apps not installed
// ============================================================

unsigned long long calculateOrphanedLicenseFilesSize() {
    unsigned long long total = 0;
    if (!cleanOrphanedLicenseFiles) return 0;

    SceUID dfd = sceIoDopen("ux0:license/");
    if (dfd < 0) return 0;

    SceIoDirent dir;
    memset(&dir, 0, sizeof(SceIoDirent));

    while (sceIoDread(dfd, &dir) > 0) {
        if (SCE_S_ISDIR(dir.d_stat.st_mode)) continue;

        char* filename = dir.d_name;
        int len = strlen(filename);

        if (len > 4 && strcmp(filename + len - 4, ".rif") == 0) {
            char fullPath[MAX_PATH_LENGTH];
            safe_snprintf(fullPath, sizeof(fullPath), "ux0:license/%s", filename);
        }
    }
    sceIoDclose(dfd);

    return total;
}

void findOrphanedLicenseFiles() {
    SceUID dfd = sceIoDopen("ux0:license/");
    if (dfd < 0) return;

    SceIoDirent dir;
    memset(&dir, 0, sizeof(SceIoDirent));

    while (sceIoDread(dfd, &dir) > 0) {
        if (isEmergencyStopRequested()) break;
        if (SCE_S_ISDIR(dir.d_stat.st_mode)) continue;
        char* filename = dir.d_name;
        int len = strlen(filename);

        if (len > 4 && strcmp(filename + len - 4, ".rif") == 0) {
            char fullPath[MAX_PATH_LENGTH];
            safe_snprintf(fullPath, sizeof(fullPath), "ux0:license/%s", filename);

            int isOrphaned = 0;

            if (len == 20) {
                char titleId[10] = {0};
                strncpy(titleId, filename, 9);
                titleId[9] = '\0';
                if (!isAppInstalled(titleId)) {
                    isOrphaned = 1;
                }
            } else if (len == 16) {
                SceIoStat stat;
                if (sceIoGetstat(fullPath, &stat) >= 0) {
                    SceDateTime now, modTime;
                    getCurrentTime(&now);
                    modTime = stat.st_mtime;

                    SceRtcTick nowTicks, modTicks;
                    sceRtcGetTick(&now, &nowTicks);
                    sceRtcGetTick(&modTime, &modTicks);

                    SceRtcTick thresholdTicks;
                    sceRtcTickAddDays(&thresholdTicks, &modTicks, 60);

                    if (sceRtcCompareTick(&nowTicks, &thresholdTicks) >= 0) {
                        isOrphaned = 1;
                    }
                }
            }

            if (isOrphaned) {
                for (int attempt = 0; attempt < 3; attempt++) {
                    if (sceIoRemove(fullPath) >= 0) {
                        g_deletedFilesCount++;
                        break;
                    }
                    sceKernelDelayThread(100 * 1000);
                }
            }
        }
    }
    sceIoDclose(dfd);
}

// ============================================================
// NEW: Empty LiveArea bubbles cleanup
// Removes leftover bubble directories that have no app installed
// ============================================================

unsigned long long calculateEmptyLiveareaBubblesSize() {
    unsigned long long total = 0;
    if (!cleanEmptyLiveareaBubbles) return 0;

    const char* bubbleDirs[] = {
        "ux0:app/",
        "ux0:patch/",
        "ux0:addcont/"
    };
    int bubbleDirCount = sizeof(bubbleDirs) / sizeof(bubbleDirs[0]);

    for (int d = 0; d < bubbleDirCount; d++) {
        SceUID dfd = sceIoDopen(bubbleDirs[d]);
        if (dfd < 0) continue;

        SceIoDirent dir;
        memset(&dir, 0, sizeof(SceIoDirent));

        while (sceIoDread(dfd, &dir) > 0) {
            if (!SCE_S_ISDIR(dir.d_stat.st_mode)) continue;

            char dirName[MAX_FILENAME_LENGTH];
            safe_strncpy(dirName, dir.d_name, sizeof(dirName));

            // Check if it's a TitleID directory (9 chars)
            if (strlen(dirName) == 9) {
                // Check if subdirectories exist at all (might be empty bubble)
                char fullPath[MAX_PATH_LENGTH];
                safe_snprintf(fullPath, sizeof(fullPath), "%s%s", bubbleDirs[d], dirName);

                SceUID subDfd = sceIoDopen(fullPath);
                if (subDfd >= 0) {
                    int hasContent = 0;
                    SceIoDirent subDir;
                    memset(&subDir, 0, sizeof(SceIoDirent));

                    while (sceIoDread(subDfd, &subDir) > 0) {
                        if (strcmp(subDir.d_name, ".") != 0 && strcmp(subDir.d_name, "..") != 0) {
                            hasContent = 1;
                            break;
                        }
                    }
                    sceIoDclose(subDfd);

                    if (!hasContent && strlen(dirName) == 9 && !isAppInstalled(dirName)) {
                        total += 0; // Empty folder, negligible size
                    }
                }
            }
        }
        sceIoDclose(dfd);
    }

    return total;
}

void removeEmptyLiveareaBubbles() {
    if (!cleanEmptyLiveareaBubbles) return;

    const char* bubbleDirs[] = {
        "ux0:app/",
        "ux0:patch/",
        "ux0:addcont/"
    };
    int bubbleDirCount = sizeof(bubbleDirs) / sizeof(bubbleDirs[0]);

    for (int d = 0; d < bubbleDirCount; d++) {
        if (isEmergencyStopRequested()) break;
        SceUID dfd = sceIoDopen(bubbleDirs[d]);
        if (dfd < 0) continue;

        SceIoDirent dir;
        memset(&dir, 0, sizeof(SceIoDirent));

        while (sceIoDread(dfd, &dir) > 0) {
            if (isEmergencyStopRequested()) break;
            if (!SCE_S_ISDIR(dir.d_stat.st_mode)) continue;

            char dirName[MAX_FILENAME_LENGTH];
            safe_strncpy(dirName, dir.d_name, sizeof(dirName));

            if (strlen(dirName) == 9 && !isAppInstalled(dirName)) {
                char fullPath[MAX_PATH_LENGTH];
                safe_snprintf(fullPath, sizeof(fullPath), "%s%s", bubbleDirs[d], dirName);

                SceUID subDfd = sceIoDopen(fullPath);
                if (subDfd >= 0) {
                    int hasContent = 0;
                    SceIoDirent subDir;
                    memset(&subDir, 0, sizeof(SceIoDirent));

                    while (sceIoDread(subDfd, &subDir) > 0) {
                        if (strcmp(subDir.d_name, ".") != 0 && strcmp(subDir.d_name, "..") != 0) {
                            hasContent = 1;
                            break;
                        }
                    }
                    sceIoDclose(subDfd);

                    if (!hasContent) {
                        sceIoRmdir(fullPath);
                        g_deletedFilesCount++;
                    }
                }
            }
        }
        sceIoDclose(dfd);
    }
}
