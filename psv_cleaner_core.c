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

#define CACHE_FILE_PATH "ux0:data/PSV_Cleaner/scan_cache.bin"
#define CACHE_VERSION 1
#define CACHE_EXPIRY_HOURS 24
#define MAX_CACHE_ENTRIES 100

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
    "ux0:data/autoplugin/cache/",
    "ux0:data/autoplugin/logs/",
    "ux0:data/autoplugin2/cache/",
    "ux0:data/autoplugin2/logs/",
    "ux0:data/henkaku/cache/",
    "ux0:data/henkaku/logs/",
    "ux0:data/VitaGrafix/cache/",
    "ux0:data/VitaGrafix/logs/",
    "ux0:data/reF00D/cache/",
    "ux0:data/NoNpDrm/temp/",
    "ux0:data/0syscall6/cache/",
    "ux0:data/tai/cache/",
    "ux0:data/PSVshell/logs/",
    "ux0:data/PSVshell/cache/",
    "ux0:data/savemgr/log/",
    "ux0:data/vitacheat/logs/",
    "ux0:data/rinCheat/logs/",
    "ux0:data/TropHAX/logs/",
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
    "ux0:data/psp2core*",
    "ux0:data/*.psp2core",
    "ux0:data/psp2dmp*",
    "ux0:data/*.psp2dmp",
    "ux0:data/crash_dumps/",
    "ux0:data/dumps/",
    "ux0:data/psp2core",
    "ux0:data/psp2dmp"
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

const char* lang_titles_screen[MAX_LANGUAGES][10] = {
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

const char* lang_ui_text[MAX_LANGUAGES][20] = {
    {
        "System Status:", "Ready for Cleanup",
        "Controls:", "■ Change Profile", "X Preview & Clean", "△ Advanced Options", "O Exit App",
        "Space to free:", "Space Freed:", "Files Deleted:",
        "D-Pad: Navigate | X: Select Profile | O: Exit",
        "Cleanup #", "No temporary files found!",
        "System Ready", "Version 1.05"
    }
};

const char* L(const char* lang_array[][20], int lang_index, int text_index) {
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
    g_emergencyStop = 0;
    g_operationInProgress = 0;
    sceKernelDelayThread(500 * 1000);
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
    strncpy(entry.path, path, sizeof(entry.path) - 1);

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
                    char fullPath[512];
                    snprintf(fullPath, sizeof(fullPath), "%s%s", searchPaths[path], filename);

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
            if (len > 4 && strcmp(filename + len - 4, ".vpk") == 0) {
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
    SceUID dfd;
    SceIoDirent dir;
    memset(&dir, 0, sizeof(SceIoDirent));
    dfd = sceIoDopen(path);
    if (dfd >= 0) {
        while (sceIoDread(dfd, &dir) > 0) {

            char newPath[1024];
            snprintf(newPath, sizeof(newPath), "%s%s%s", path,
                     (path[strlen(path)-1] == '/') ? "" : "/",
                     dir.d_name);
            if (SCE_S_ISDIR(dir.d_stat.st_mode)) {
                if (strcmp(dir.d_name, ".") && strcmp(dir.d_name, "..")) {
                    deleteRecursive(newPath);
                    sceIoRmdir(newPath);
                }
            } else {
                if (sceIoRemove(newPath) >= 0) {
                    g_deletedFilesCount++;
                }
            }
        }
        sceIoDclose(dfd);
        sceIoRmdir(path);
    } else {
        if (sceIoRemove(path) >= 0) {
            g_deletedFilesCount++;
        }
    }
}

unsigned long long calculateTempSizeRecursive(const char *path) {
    unsigned long long total = 0;
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
                total += calculateTempSizeRecursive(newPath);
            } else {
                total += dir.d_stat.st_size;
            }
        }
        sceIoDclose(dfd);
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

    for(size_t i = 0; i < TEMP_PATHS_COUNT; i++){
        unsigned long long pathSize = 0;

        if (cache && cache->entries[i].isValid && !hasDirectoryChanged(TEMP_PATHS[i], &cache->entries[i].lastModified)) {
            pathSize = cache->entries[i].totalSize;
        } else {
            pathSize = calculateTempSizeRecursive(TEMP_PATHS[i]);

            if (newCache) {
                newCache->entries[i] = createCacheEntry(TEMP_PATHS[i]);
            }
        }

        total += pathSize;
    }

    if (newCache) {
        saveScanCache(newCache);
        free(newCache);
    }

    if (cache) {
        free(cache);
    }

    return total;
}

unsigned long long cleanTemporaryFiles() {
    resetDeletedFilesCount();

    unsigned long long before = calculateTempSize();

    forceDeleteDumpFiles();

    aggressiveDumpCleanup();

    cleanupVpkFiles();

    for(size_t i=0;i<TEMP_PATHS_COUNT;i++){
        deleteRecursive(TEMP_PATHS[i]);
    }

    forceDeleteDumpFiles();
    aggressiveDumpCleanup();

    cleanupVpkFiles();

    sceIoRemove(CACHE_FILE_PATH);

    unsigned long long after = calculateTempSize();
    return before - after;
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

    strncpy(list->files[list->count].path, path, sizeof(list->files[list->count].path) - 1);
    list->files[list->count].path[sizeof(list->files[list->count].path) - 1] = '\0';
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

void scanFilesForPreview(FileList *list) {
    if (!list) return;

    for(size_t i=0; i<TEMP_PATHS_COUNT; i++){
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
