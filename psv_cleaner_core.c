#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/threadmgr.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "psv_cleaner_core.h"

// Global counter for deleted files
int g_deletedFilesCount = 0;

// List of temporary folders to clean
const char *TEMP_PATHS[] = {
    // System temporary folders
    "ux0:temp/",
    "ux0:data/temp/",
    "ux0:cache/",
    "ux0:log/",
    "ur0:temp/",
    "ur0:temp/sqlite/",
    "uma0:temp/",

    // VitaShell cleanup
    "ux0:VitaShell/cache/",
    "ux0:VitaShell/temp/",
    "ux0:VitaShell/log/",
    "ux0:VitaShell/recent/",
    "ux0:VitaShell/backup/temp/",
    "ux0:VitaShell/trash/",

    // PKGi cleanup
    "ux0:pkgi/tmp/",
    "ux0:pkgi/cache/",
    "ux0:pkgi/log.txt",
    "ux0:pkgi/downloads/temp/",
    "ux0:pkgi/backup/temp/",

    // RetroArch cleanup (cache and logs only, never saves or configs)
    "ux0:data/retroarch/cache/",
    "ux0:data/retroarch/logs/",
    "ux0:data/retroarch/temp/",
    "ux0:data/retroarch/thumbnails/cache/",
    "ux0:data/retroarch/shaders/cache/",
    "ux0:data/retroarch/database/rdb/temp/",

    // PSP Emulator cleanup
    "ux0:pspemu/temp/",
    "ux0:pspemu/cache/",

    // Adrenaline PSP emulator cleanup
    "ux0:data/Adrenaline/cache/",
    "ux0:data/Adrenaline/logs/",
    "ux0:data/Adrenaline/temp/",
    "ux0:data/Adrenaline/crash/",
    "ux0:data/Adrenaline/dumps/",

    // Homebrew applications - Moonlight
    "ux0:data/moonlight/cache/",
    "ux0:data/moonlight/logs/",

    // Homebrew applications - Autoplugin
    "ux0:data/autoplugin/cache/",
    "ux0:data/autoplugin/logs/",
    "ux0:data/autoplugin2/cache/",
    "ux0:data/autoplugin2/logs/",

    // Homebrew applications - Henkaku
    "ux0:data/henkaku/cache/",
    "ux0:data/henkaku/logs/",

    // Homebrew applications - VitaGrafix
    "ux0:data/VitaGrafix/cache/",
    "ux0:data/VitaGrafix/logs/",

    // Homebrew applications - reF00D
    "ux0:data/reF00D/cache/",

    // Homebrew applications - NoNpDrm
    "ux0:data/NoNpDrm/temp/",

    // Homebrew applications - 0syscall6
    "ux0:data/0syscall6/cache/",

    // Homebrew applications - TAI plugin system
    "ux0:data/tai/cache/",

    // Homebrew applications - PSVshell
    "ux0:data/PSVshell/logs/",
    "ux0:data/PSVshell/cache/",

    // Homebrew applications - SaveManager
    "ux0:data/savemgr/log/",

    // Homebrew applications - VitaCheat
    "ux0:data/vitacheat/logs/",

    // Homebrew applications - rinCheat
    "ux0:data/rinCheat/logs/",

    // Homebrew applications - TropHAX
    "ux0:data/TropHAX/logs/",

    // Browser and webkit cache
    "ux0:data/browser/cache/",
    "ux0:data/browser/temp/",
    "ux0:data/browser/logs/",
    "ux0:data/webkit/cache/",
    "ux0:data/webkit/localstorage/temp/",

    // Network temporary files
    "ux0:data/net/temp/",

    // Download cleanup (complete directories and temp files)
    "ux0:download/",
    "ux0:download/temp/",
    "ux0:downloads/",
    "ux0:downloads/temp/",
    "ux0:bgdl/t/",

    // Package and installation temporary files
    "ux0:data/pkg/temp/",
    "ux0:package/temp/",
    "ux0:appmeta/temp/",
    "ur0:appmeta/temp/",
    "ux0:license/temp/",

    // System update and patch cleanup (safe temp files)
    "ux0:patch_temp/",
    "ux0:update_temp/",

    // Media thumbnails (regenerable)
    "ux0:picture/.thumbnails/",
    "ux0:video/.thumbnails/",
    "ux0:music/.cache/",
    "ux0:photo/cache/",

    // Shader logs
    "ux0:shaderlog/",

    // Additional log folders (safe)
    "ux0:data/logs/temp/",
    "ux0:data/cache/",

    // USB and external storage cleanup (safe)
    "uma0:cache/",
    "uma0:log/",
    "uma0:data/temp/",
    "uma0:data/cache/",

    // Crash dump files (safe to delete - never contains user data)
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

// Get deleted files count
int getDeletedFilesCount() {
    return g_deletedFilesCount;
}

// Load cleanup counter from file
int loadCleanupCounter() {
    int count = 0;
    SceUID fd = sceIoOpen("ux0:data/PSV_Cleaner/counter.txt", SCE_O_RDONLY, 0777);
    if (fd >= 0) {
        sceIoRead(fd, &count, sizeof(int));
        sceIoClose(fd);
    }
    return count;
}

// Save cleanup counter to file
void saveCleanupCounter(int count) {
    // Ensure directory exists
    sceIoMkdir("ux0:data/PSV_Cleaner", 0777);
    SceUID fd = sceIoOpen("ux0:data/PSV_Cleaner/counter.txt", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (fd >= 0) {
        sceIoWrite(fd, &count, sizeof(int));
        sceIoClose(fd);
    }
}

// Reset deleted files count
void resetDeletedFilesCount() {
    g_deletedFilesCount = 0;
}

// Force delete dump files that might be locked by system
void forceDeleteDumpFiles() {
    // Try to delete common dump file patterns with multiple attempts
    const char* dumpPatterns[] = {
        "ux0:data/psp2core",
        "ux0:data/psp2dmp",
        "ux0:data/psp2core.*",
        "ux0:data/psp2dmp.*",
        "ux0:data/*.psp2core",
        "ux0:data/*.psp2dmp"
    };
    
    for (int i = 0; i < 6; i++) {
        // Try multiple removal attempts for locked files
        for (int attempt = 0; attempt < 3; attempt++) {
            sceIoRemove(dumpPatterns[i]);
            sceKernelDelayThread(100 * 1000); // 100ms delay between attempts
        }
    }
    
    // Also try to delete any dump files in subdirectories
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

// Aggressive dump file deletion - tries to find and delete .psp2dmp files
void aggressiveDumpCleanup() {
    // Try to find and delete .psp2dmp files in common locations
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
                // Check if file ends with .psp2dmp
                char* filename = dir.d_name;
                int len = strlen(filename);
                if (len > 8 && strcmp(filename + len - 8, ".psp2dmp") == 0) {
                    char fullPath[512];
                    snprintf(fullPath, sizeof(fullPath), "%s%s", searchPaths[path], filename);
                    
                    // Try multiple times to delete the file
                    for (int attempt = 0; attempt < 5; attempt++) {
                        if (sceIoRemove(fullPath) >= 0) {
                            break; // Successfully deleted
                        }
                        sceKernelDelayThread(200 * 1000); // 200ms delay
                    }
                }
            }
            sceIoDclose(dfd);
        }
    }
}

// Delete recursively a file or directory
void deleteRecursive(const char *path) {
    SceUID dfd;
    SceIoDirent dir;
    memset(&dir, 0, sizeof(SceIoDirent));
    dfd = sceIoDopen(path);
    if (dfd >= 0) {
        while (sceIoDread(dfd, &dir) > 0) {
            // Continue with cleaning (PS Vita handles suspension automatically)
            
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
                // Try to remove file
                if (sceIoRemove(newPath) >= 0) {
                    g_deletedFilesCount++;
                }
            }
        }
        sceIoDclose(dfd);
        sceIoRmdir(path);
    } else {
        // Try to remove file
        if (sceIoRemove(path) >= 0) {
            g_deletedFilesCount++;
        }
    }
}

// Calculate size recursively for a single path
unsigned long long calculateTempSizeRecursive(const char *path) {
    unsigned long long total = 0;
    SceUID dfd;
    SceIoDirent dir;
    memset(&dir, 0, sizeof(SceIoDirent));

    // First try to open as directory
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
                // Recursively calculate subdirectory size
                total += calculateTempSizeRecursive(newPath);
            } else {
                // Add file size
                total += dir.d_stat.st_size;
            }
        }
        sceIoDclose(dfd);
    } else {
        // If not a directory, try as a single file
        SceIoStat stat;
        if (sceIoGetstat(path, &stat) >= 0) {
            if (SCE_S_ISREG(stat.st_mode)) { // Regular file
                total += stat.st_size;
            }
        }
        // If path doesn't exist at all, just return 0 (don't fail)
    }

    return total;
}

// Calculate total size of temporary folders
unsigned long long calculateTempSize() {
    unsigned long long total = 0;
    for(size_t i=0;i<TEMP_PATHS_COUNT;i++){
        // Continue with calculation (PS Vita handles suspension automatically)
        unsigned long long pathSize = calculateTempSizeRecursive(TEMP_PATHS[i]);
        total += pathSize;
    }
    return total;
}

// Clean all temporary folders
unsigned long long cleanTemporaryFiles() {
    // Reset counter before cleaning
    resetDeletedFilesCount();
    
    unsigned long long before = calculateTempSize();
    
    // First try to force delete locked dump files
    forceDeleteDumpFiles();
    
    // Try aggressive dump cleanup
    aggressiveDumpCleanup();
    
    for(size_t i=0;i<TEMP_PATHS_COUNT;i++){
        // Continue with cleaning (PS Vita handles suspension automatically)
        deleteRecursive(TEMP_PATHS[i]);
    }
    
    // Try again to delete dump files after main cleaning
    forceDeleteDumpFiles();
    aggressiveDumpCleanup();
    
    unsigned long long after = calculateTempSize();
    return before - after;
}

// Create a new file list for preview
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

// Free file list
void freeFileList(FileList *list) {
    if (list) {
        if (list->files) {
            free(list->files);
        }
        free(list);
    }
}

// Add file to list
void addFileToList(FileList *list, const char *path, unsigned long long size) {
    if (!list || !path) return;

    // Expand array if needed
    if (list->count >= list->capacity) {
        list->capacity *= 2;
        FileInfo *newFiles = (FileInfo*)realloc(list->files, sizeof(FileInfo) * list->capacity);
        if (!newFiles) return; // Out of memory
        list->files = newFiles;
    }

    // Add file info
    strncpy(list->files[list->count].path, path, sizeof(list->files[list->count].path) - 1);
    list->files[list->count].path[sizeof(list->files[list->count].path) - 1] = '\0';
    list->files[list->count].size = size;
    list->totalSize += size;
    list->count++;
}

// Recursive scan for preview
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
                // Recursively scan subdirectory
                scanPathForPreview(list, newPath);
            } else {
                // Add file to list
                addFileToList(list, newPath, dir.d_stat.st_size);
            }
        }
        sceIoDclose(dfd);
    } else {
        // Try as a single file
        SceIoStat stat;
        if (sceIoGetstat(path, &stat) >= 0) {
            if (!SCE_S_ISDIR(stat.st_mode)) {
                addFileToList(list, path, stat.st_size);
            }
        }
    }
}

// Scan all temporary files for preview
void scanFilesForPreview(FileList *list) {
    if (!list) return;
    
    for(size_t i=0; i<TEMP_PATHS_COUNT; i++){
        scanPathForPreview(list, TEMP_PATHS[i]);
    }
    
    // Special handling for dump files with patterns (*.psp2dmp, *.psp2core)
    // Scan ux0:data/ for dump files specifically
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
                
                // Check for .psp2dmp or .psp2core files
                int isDumpFile = 0;
                if (len > 8 && strcmp(filename + len - 8, ".psp2dmp") == 0) {
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
}
