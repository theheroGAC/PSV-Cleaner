#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/threadmgr.h>
#include <stdio.h>
#include <string.h>
#include "psv_cleaner_core.h"

// List of temporary folders to clean
const char *TEMP_PATHS[] = {
    // System temporary folders
    "ux0:temp/",
    "ux0:data/temp/",
    "ux0:cache/",
    "ux0:log/",
    "ur0:temp/",
    "uma0:temp/",
    
    // VitaShell cleanup
    "ux0:VitaShell/cache/",
    "ux0:VitaShell/temp/",
    "ux0:VitaShell/log/",
    "ux0:VitaShell/recent/",
    
    // PKGi cleanup
    "ux0:pkgi/tmp/",
    "ux0:pkgi/cache/",
    "ux0:pkgi/log.txt",
    
    // RetroArch cleanup
    "ux0:data/retroarch/cache/",
    "ux0:data/retroarch/logs/",
    "ux0:data/retroarch/temp/",
    
    // PSP Emulator cleanup
    "ux0:pspemu/temp/",
    "ux0:pspemu/cache/",
    
    // Additional homebrew cleanup
    "ux0:data/moonlight/cache/",
    "ux0:data/moonlight/logs/",
    "ux0:data/autoplugin/cache/",
    "ux0:data/autoplugin/logs/",
    "ux0:data/autoplugin2/cache/",
    "ux0:data/autoplugin2/logs/",
    "ux0:data/henkaku/cache/",
    "ux0:data/henkaku/logs/",
    
    // Adrenaline cleanup
    "ux0:data/Adrenaline/cache/",
    "ux0:data/Adrenaline/logs/",
    "ux0:data/Adrenaline/temp/",
    "ux0:data/Adrenaline/crash/",
    "ux0:data/Adrenaline/dumps/",
    
    // Browser and web cache
    "ux0:data/browser/cache/",
    "ux0:data/browser/temp/",
    "ux0:data/browser/logs/",
    
    // Download cleanup (safe temp files only)
    "ux0:download/temp/",
    "ux0:downloads/temp/",
    
    // Additional log folders (safe)
    "ux0:data/logs/temp/",
    "ux0:data/temp/",
    "ux0:data/cache/",
    
    // USB and external storage cleanup (safe)
    "uma0:cache/",
    "uma0:log/",
    "uma0:data/temp/",
    "uma0:data/cache/",
    
    // Additional VitaShell paths (safe)
    "ux0:VitaShell/backup/temp/",
    "ux0:VitaShell/trash/",
    
    // PKGi additional paths (safe)
    "ux0:pkgi/downloads/temp/",
    "ux0:pkgi/backup/temp/",
    
    // RetroArch additional paths (safe - only cache)
    "ux0:data/retroarch/thumbnails/cache/",
    
    // System update cleanup (safe temp files)
    "ux0:patch_temp/",
    "ux0:update_temp/",
    
    // Crash dump files (safe to delete)
    "ux0:data/psp2core*",
    "ux0:data/*.psp2core",
    "ux0:data/psp2dmp*",
    "ux0:data/*.psp2dmp",
    "ux0:data/crash_dumps/",
    "ux0:data/dumps/",
    
    // Specific dump file patterns
    "ux0:data/psp2core",
    "ux0:data/psp2dmp"
};

const size_t TEMP_PATHS_COUNT = sizeof(TEMP_PATHS)/sizeof(TEMP_PATHS[0]);

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
                sceIoRemove(newPath);
            }
        }
        sceIoDclose(dfd);
        sceIoRmdir(path);
    } else {
        // Try to remove file
        sceIoRemove(path);
    }
}

// Calculate total size of temporary folders
unsigned long long calculateTempSize() {
    unsigned long long total = 0;
    for(size_t i=0;i<TEMP_PATHS_COUNT;i++){
        // Continue with calculation (PS Vita handles suspension automatically)
        
        SceUID dfd;
        SceIoDirent dir;
        memset(&dir,0,sizeof(SceIoDirent));
        dfd = sceIoDopen(TEMP_PATHS[i]);
        if(dfd >= 0){
            while(sceIoDread(dfd,&dir) > 0){
                if(!SCE_S_ISDIR(dir.d_stat.st_mode)){
                    total += dir.d_stat.st_size;
                }
            }
            sceIoDclose(dfd);
        }
    }
    return total;
}

// Clean all temporary folders
unsigned long long cleanTemporaryFiles() {
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
