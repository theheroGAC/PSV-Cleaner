#ifndef PSV_CLEANER_TOOLS_H
#define PSV_CLEANER_TOOLS_H

#include <stddef.h>
#include "psv_cleaner_core.h"

#define WHITELIST_MAX_ITEMS 128
#define HISTORY_MAX_ENTRIES 100
#define TAI_MAX_ITEMS 128
#define LARGE_MAX_ITEMS 192

typedef struct {
    char timestamp[24];
    char mode[32];
    int deleted;
    int failed;
    int interrupted;
    unsigned long long bytesFreed;
} HistoryEntry;

typedef struct {
    unsigned int totalCleanups;
    unsigned long long totalFilesDeleted;
    unsigned long long totalBytesFreed;
} LifetimeStats;

typedef enum {
    TAI_OK = 0,
    TAI_UNUSED = 1,
    TAI_BROKEN = 2,
    TAI_DUPLICATE = 3,
    TAI_BACKUP = 4,
    TAI_CONFLICT = 5
} TaiStatus;

typedef struct {
    char path[MAX_PATH_LENGTH];
    char section[32];
    TaiStatus status;
    unsigned long long size;
    unsigned char userSelected;
} TaiAuditEntry;

typedef struct {
    int hasUx0Tai;
    int hasUr0Tai;
    int hasUx0Config;
    int hasUr0Config;
    int conflictDetected;
    char activeConfig[64];
    int entryCount;
    int unusedCount;
    int brokenCount;
    int duplicateCount;
    TaiAuditEntry entries[TAI_MAX_ITEMS];
} TaiAuditState;

typedef enum {
    LARGE_KIND_SIZE = 0,
    LARGE_KIND_VPK = 1,
    LARGE_KIND_PKG = 2,
    LARGE_KIND_ARCHIVE = 3
} LargeFileKind;

typedef struct {
    char path[MAX_PATH_LENGTH];
    unsigned long long size;
    LargeFileKind kind;
    unsigned char selected;
} LargeFileItem;

extern TaiAuditState g_taiAudit;
extern LargeFileItem g_largeItems[LARGE_MAX_ITEMS];
extern volatile int g_largeItemCount;
extern volatile int g_largeTruncated;
extern int g_largeMinSizeMB;

int sfoReadTitle(const char *sfoPath, char *outTitle, size_t outSize);
void resolveTitleName(const char *titleId, char *outName, size_t outSize);
void clearTitleNameCache();

void whitelistLoad();
void whitelistSave();
int whitelistAdd(const char *entry);
int whitelistRemove(int index);
void whitelistClear();
int whitelistCheck(const char *path, const char *titleId);
int whitelistGetCount();
const char *whitelistGetItem(int index);

void historyLoad();
void historyAppend(const char *mode, int deleted, int failed, int interrupted, unsigned long long bytesFreed);
int historyGetCount();
const HistoryEntry *historyGetEntry(int index);
void historyGetLifetimeStats(LifetimeStats *outStats);
void historyClear();

int backupDirToZip(const char *srcDir, const char *zipPath, unsigned long long *outUncompressed, unsigned long long *outCompressed);
int backupOrphanSaveZip(const char *titleId, const char *saveDir, int deleteAfterBackup, char *outZipPath, size_t outZipSize);

void taiAuditRun();
int taiBackupConfig(char *outBackupPath, size_t outSize);
int taiDeleteUnusedSelected();

void largeFilesScanRun(int minSizeMB);
void largeFilesDeleteSelected();

void analyzerBrowseDir(const char *dirPath);
int analyzerDeleteEntry(const char *path);

#endif
