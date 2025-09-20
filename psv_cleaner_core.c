#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
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
    "ux0:data/henkaku/cache/",
    "ux0:data/henkaku/logs/",
    
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
    // Try to delete common dump file patterns
    const char* dumpPatterns[] = {
        "ux0:data/psp2core",
        "ux0:data/psp2dmp"
    };
    
    for (int i = 0; i < 2; i++) {
        // Try to remove dump files (PS Vita will handle permissions automatically)
        sceIoRemove(dumpPatterns[i]);
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
    
    for(size_t i=0;i<TEMP_PATHS_COUNT;i++){
        // Continue with cleaning (PS Vita handles suspension automatically)
        deleteRecursive(TEMP_PATHS[i]);
    }
    
    // Try again to delete dump files after main cleaning
    forceDeleteDumpFiles();
    
    unsigned long long after = calculateTempSize();
    return before - after;
}
