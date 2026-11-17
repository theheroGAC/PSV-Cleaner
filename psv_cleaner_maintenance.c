#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/io/devctl.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/rtc.h>
#include <string.h>
#include <stdio.h>
#include "psv_cleaner_core.h"
#include "psv_cleaner_tools.h"

#define SCAN_SLOT_COUNT 512
#define SCAN_MAX_RECURSION 16
#define SCAN_YIELD_INTERVAL 384

static const char *ANALYZER_VOLUMES[] = {"ux0:", "ur0:", "uma0:", "imc0:"};

static const char *ANALYZER_CATEGORY_NAMES[ANALYZER_MAX_CATEGORIES] = {
    "app", "appmeta", "data", "addcont", "patch", "license", "tai", "temp", "other"
};

static const char *DUMP_SEARCH_ROOTS[] = {
    "ux0:data/", "ux0:temp/", "ux0:cache/", "ux0:log/",
    "ur0:data/", "ur0:temp/",
    "uma0:data/", "uma0:temp/",
    "imc0:data/", "imc0:temp/"
};

static const char *APP_TEMP_DIRS[] = {"cache", "temp", "tmp", "logs", "log"};

AnalyzerVolumeState g_analyzerVolumes[ANALYZER_MAX_VOLUMES];
volatile int g_analyzerVolumeCount = 0;
volatile int g_analyzerActiveVolume = 0;
char g_analyzerBrowsePath[MAX_PATH_LENGTH] = "";
int g_analyzerPathDepth = 0;
AnalyzerDirEntry g_analyzerDirEntries[ANALYZER_DIR_ENTRIES];
volatile int g_analyzerDirEntryCount = 0;
volatile int g_analyzerDirTruncated = 0;

SmartCleanPlan g_smartCleanPlan;
CleanScanStats g_smartCleanStats;
CleanReport g_cleanReport;

SafetyItem g_orphanItems[ORPHAN_MAX_ITEMS];
volatile int g_orphanItemCount = 0;
volatile int g_orphanTruncated = 0;
volatile int g_orphanSkipped = 0;
volatile int g_orphanProtected = 0;

static int analyzerCategoryIndex(const char *name) {
    if (strcasecmp(name, "app") == 0) return 0;
    if (strcasecmp(name, "appmeta") == 0) return 1;
    if (strcasecmp(name, "data") == 0) return 2;
    if (strcasecmp(name, "addcont") == 0) return 3;
    if (strcasecmp(name, "patch") == 0) return 4;
    if (strcasecmp(name, "license") == 0) return 5;
    if (strcasecmp(name, "tai") == 0) return 6;
    if (strcasecmp(name, "temp") == 0 || strcasecmp(name, "cache") == 0 || strcasecmp(name, "log") == 0) return 7;
    return 8;
}

static void analyzerAddTopDir(AnalyzerVolumeState *state, const char *name, unsigned long long size) {
    int insertAt = state->topDirCount;

    for (int i = 0; i < state->topDirCount; i++) {
        if (size > state->topDirs[i].size) {
            insertAt = i;
            break;
        }
    }

    if (insertAt >= ANALYZER_TOP_DIRS) return;

    int last = state->topDirCount < ANALYZER_TOP_DIRS ? state->topDirCount : ANALYZER_TOP_DIRS - 1;

    for (int i = last; i > insertAt; i--) {
        state->topDirs[i] = state->topDirs[i - 1];
    }

    safe_strncpy(state->topDirs[insertAt].name, name, sizeof(state->topDirs[insertAt].name));
    state->topDirs[insertAt].size = size;

    if (state->topDirCount < ANALYZER_TOP_DIRS) state->topDirCount++;
}

void analyzerPrepare() {
    int count = 0;

    for (size_t i = 0; i < sizeof(ANALYZER_VOLUMES) / sizeof(ANALYZER_VOLUMES[0]); i++) {
        if (count >= ANALYZER_MAX_VOLUMES) break;

        AnalyzerVolumeState *state = &g_analyzerVolumes[count];
        memset(state, 0, sizeof(*state));
        safe_strncpy(state->volume, ANALYZER_VOLUMES[i], sizeof(state->volume));

        SceIoDevInfo info;
        memset(&info, 0, sizeof(info));

        int available = 0;
        if (sceIoDevctl(ANALYZER_VOLUMES[i], 0x3001, NULL, 0, &info, sizeof(info)) >= 0) {
            available = 1;
        }
        if (!available) {
            SceIoStat stat;
            if (sceIoGetstat(ANALYZER_VOLUMES[i], &stat) < 0) continue;
            available = 1;
        }

        state->available = 1;
        state->totalSize = (unsigned long long)info.max_size;
        state->freeSize = (unsigned long long)info.free_size;

        if (state->totalSize > 0) {
            if (state->freeSize > state->totalSize) state->freeSize = state->totalSize;
            state->usedSize = state->totalSize - state->freeSize;
            state->usedPercent = (int)((state->usedSize * 100) / state->totalSize);
        }

        count++;
    }

    g_analyzerVolumeCount = count;
    if (g_analyzerActiveVolume >= count) g_analyzerActiveVolume = 0;
}

typedef int (*ScanFileFn)(void *ctx, const char *path, unsigned long long size);

typedef struct {
    ScanFileFn fileFn;
    void *ctx;
    int allowDelete;
    int mode;
    int partial;
    unsigned long long filesSeen;
    unsigned long long filesIgnored;
    unsigned long long filesSkipped;
    unsigned long long filesFailed;
    unsigned long long filesDeleted;
    unsigned long long dirsFailed;
    unsigned long long dirsDeleted;
    unsigned long long bytesDeleted;
} ScanWalker;

typedef struct {
    char path[SAFETY_MAX_PATH];
    int isLeave;
} ScanSlot;

static ScanSlot s_scanSlots[SCAN_SLOT_COUNT];
static int s_scanSlotTop = 0;
static unsigned int s_scanYieldCounter = 0;

static int scanDeleteRequested(int action) {
    return action == 1;
}

static void scanHandleFile(ScanWalker *walker, const char *path, unsigned long long size) {
    int action = 0;

    walker->filesSeen++;

    if (walker->fileFn) {
        action = walker->fileFn(walker->ctx, path, size);
    }

    if (scanDeleteRequested(action)) {
        if (!walker->allowDelete) {
            walker->filesIgnored++;
            return;
        }

        if (sceIoRemove(path) >= 0) {
            walker->filesDeleted++;
            walker->bytesDeleted += size;
            incrementDeletedFilesCount();
            return;
        }

        walker->filesFailed++;
        return;
    }

    if (action == 2) {
        walker->filesSkipped++;
        return;
    }

    walker->filesIgnored++;
}

static void scanPushSlot(const char *path, int isLeave) {
    if (s_scanSlotTop >= SCAN_SLOT_COUNT) return;

    safe_strncpy(s_scanSlots[s_scanSlotTop].path, path, sizeof(s_scanSlots[s_scanSlotTop].path));
    s_scanSlots[s_scanSlotTop].isLeave = isLeave;
    s_scanSlotTop++;
}

static void scanTree(ScanWalker *walker, const char *root, int depth) {
    int base = s_scanSlotTop;

    scanPushSlot(root, 0);

    while (s_scanSlotTop > base) {
        if (isEmergencyStopRequested()) {
            walker->partial = 1;
            break;
        }

        s_scanSlotTop--;
        ScanSlot slot;
        memcpy(&slot, &s_scanSlots[s_scanSlotTop], sizeof(slot));

        if (slot.isLeave) {
            if (walker->allowDelete) {
                if (sceIoRmdir(slot.path) >= 0) walker->dirsDeleted++;
                else walker->dirsFailed++;
            }
            continue;
        }

        SceUID dfd = sceIoDopen(slot.path);
        if (dfd < 0) {
            SceIoStat stat;
            if (sceIoGetstat(slot.path, &stat) >= 0 && !SCE_S_ISDIR(stat.st_mode)) {
                scanHandleFile(walker, slot.path, (unsigned long long)stat.st_size);
            } else {
                walker->filesSkipped++;
            }
            continue;
        }

        int hasSubdir = 0;
        SceIoDirent dir;
        memset(&dir, 0, sizeof(dir));

        if (!walker->allowDelete) {
            while (sceIoDread(dfd, &dir) > 0) {
                if (isEmergencyStopRequested()) {
                    walker->partial = 1;
                    break;
                }

                if (strcmp(dir.d_name, ".") == 0 || strcmp(dir.d_name, "..") == 0) continue;

                char child[SAFETY_MAX_PATH];
                safe_snprintf(child, sizeof(child), "%s%s%s", slot.path,
                              (slot.path[strlen(slot.path) - 1] == '/') ? "" : "/",
                              dir.d_name);

                if (SCE_S_ISDIR(dir.d_stat.st_mode)) {
                    if (s_scanSlotTop < SCAN_SLOT_COUNT) {
                        scanPushSlot(child, 0);
                    } else if (depth < SCAN_MAX_RECURSION) {
                        scanTree(walker, child, depth + 1);
                    } else {
                        walker->partial = 1;
                    }
                } else {
                    scanHandleFile(walker, child, (unsigned long long)dir.d_stat.st_size);
                }
            }
            sceIoDclose(dfd);
        } else {
            typedef struct {
                char *name;
                int isDir;
                unsigned long long size;
            } CollectedEntry;

            CollectedEntry *entries = NULL;
            int count = 0, capacity = 0;

            while (sceIoDread(dfd, &dir) > 0) {
                if (isEmergencyStopRequested()) {
                    walker->partial = 1;
                    break;
                }

                if (strcmp(dir.d_name, ".") == 0 || strcmp(dir.d_name, "..") == 0) continue;

                if (count >= capacity) {
                    int newCap = capacity ? capacity * 2 : 64;
                    CollectedEntry *newEntries = (CollectedEntry *)realloc(entries, newCap * sizeof(CollectedEntry));
                    if (!newEntries) break;
                    entries = newEntries;
                    capacity = newCap;
                }

                entries[count].name = strdup(dir.d_name);
                if (entries[count].name) {
                    entries[count].isDir = SCE_S_ISDIR(dir.d_stat.st_mode);
                    entries[count].size = (unsigned long long)dir.d_stat.st_size;
                    count++;
                }
            }
            sceIoDclose(dfd);

            for (int i = 0; i < count; i++) {
                if (isEmergencyStopRequested()) {
                    walker->partial = 1;
                    free(entries[i].name);
                    continue;
                }

                char child[SAFETY_MAX_PATH];
                safe_snprintf(child, sizeof(child), "%s%s%s", slot.path,
                              (slot.path[strlen(slot.path) - 1] == '/') ? "" : "/",
                              entries[i].name);

                if (entries[i].isDir) {
                    if (!hasSubdir) {
                        hasSubdir = 1;
                        scanPushSlot(slot.path, 1);
                    }

                    if (s_scanSlotTop < SCAN_SLOT_COUNT) {
                        scanPushSlot(child, 0);
                    } else if (depth < SCAN_MAX_RECURSION) {
                        scanTree(walker, child, depth + 1);
                    } else {
                        walker->partial = 1;
                    }
                } else {
                    scanHandleFile(walker, child, entries[i].size);
                }

                free(entries[i].name);
            }

            if (entries) free(entries);

            if (!hasSubdir) {
                if (sceIoRmdir(slot.path) >= 0) walker->dirsDeleted++;
                else walker->dirsFailed++;
            }
        }

        s_scanYieldCounter++;
        if ((s_scanYieldCounter % SCAN_YIELD_INTERVAL) == 0) {
            sceKernelDelayThread(1000);
        }
    }

    s_scanSlotTop = base;
}

typedef struct {
    unsigned long long total;
} MeasureContext;

static int measureFile(void *ctx, const char *path, unsigned long long size) {
    (void)path;
    ((MeasureContext *)ctx)->total += size;
    return 0;
}

static unsigned long long measurePath(const char *path) {
    ScanWalker walker;
    MeasureContext ctx;

    memset(&walker, 0, sizeof(walker));
    memset(&ctx, 0, sizeof(ctx));

    walker.fileFn = measureFile;
    walker.ctx = &ctx;

    scanTree(&walker, path, 0);

    return ctx.total;
}

static int analyzerCountRootEntries(const char *volume) {
    SceUID dfd = sceIoDopen(volume);
    if (dfd < 0) return 0;

    int count = 0;
    SceIoDirent dir;
    memset(&dir, 0, sizeof(dir));

    while (sceIoDread(dfd, &dir) > 0) {
        if (strcmp(dir.d_name, ".") == 0 || strcmp(dir.d_name, "..") == 0) continue;
        count++;
    }

    sceIoDclose(dfd);
    return count;
}

void analyzerScanVolume(int index) {
    if (index < 0 || index >= g_analyzerVolumeCount) return;

    AnalyzerVolumeState *state = &g_analyzerVolumes[index];
    state->partial = 0;
    state->topDirCount = 0;
    state->categoryCount = ANALYZER_MAX_CATEGORIES;

    for (int k = 0; k < ANALYZER_MAX_CATEGORIES; k++) {
        safe_strncpy(state->categories[k].name, ANALYZER_CATEGORY_NAMES[k], sizeof(state->categories[k].name));
        state->categories[k].size = 0;
    }

    int totalEntries = analyzerCountRootEntries(state->volume);
    initScanProgress(totalEntries > 0 ? totalEntries : 1);

    SceUID dfd = sceIoDopen(state->volume);
    if (dfd < 0) {
        state->partial = 1;
        updateScanProgress(1);
        return;
    }

    int processed = 0;
    SceIoDirent dir;
    memset(&dir, 0, sizeof(dir));

    char rootPrefix[SAFETY_MAX_PATH];
    safe_snprintf(rootPrefix, sizeof(rootPrefix), "%s%s", state->volume,
                  (state->volume[strlen(state->volume) - 1] == '/') ? "" : "/");

    while (sceIoDread(dfd, &dir) > 0) {
        if (isEmergencyStopRequested()) {
            state->partial = 1;
            break;
        }

        if (strcmp(dir.d_name, ".") == 0 || strcmp(dir.d_name, "..") == 0) continue;

        char fullPath[SAFETY_MAX_PATH];
        safe_strncpy(fullPath, rootPrefix, sizeof(fullPath));
        safe_strncat(fullPath, dir.d_name, sizeof(fullPath));

        unsigned long long size = 0;
        if (SCE_S_ISDIR(dir.d_stat.st_mode)) {
            size = measurePath(fullPath);
        } else {
            size = (unsigned long long)dir.d_stat.st_size;
        }

        int category = analyzerCategoryIndex(dir.d_name);
        state->categories[category].size += size;
        analyzerAddTopDir(state, dir.d_name, size);

        processed++;
        updateScanProgress(processed);
    }

    sceIoDclose(dfd);

    updateScanProgress(totalEntries > 0 ? totalEntries : 1);

    for (int i = 1; i < state->categoryCount; i++) {
        AnalyzerCategory current = state->categories[i];
        int j = i - 1;

        while (j >= 0 && state->categories[j].size < current.size) {
            state->categories[j + 1] = state->categories[j];
            j--;
        }
        state->categories[j + 1] = current;
    }

    analyzerBrowseDir(state->volume);
    g_analyzerPathDepth = 0;
}

typedef enum {
    ORPHAN_SRC_DATA = 0,
    ORPHAN_SRC_ADDCONT,
    ORPHAN_SRC_PATCH,
    ORPHAN_SRC_APPMETA,
    ORPHAN_SRC_LICENSE_RIF,
    ORPHAN_SRC_LICENSE_DIR,
    ORPHAN_SRC_SAVEDATA,
    ORPHAN_SRC_REPATCH,
    ORPHAN_SRC_READDCONT
} OrphanSource;

static int orphanMakeTitleId(const char *name, int offset, char *titleOut, size_t titleSize) {
    char buffer[SAFETY_MAX_TITLE];

    if (strlen(name) < (size_t)(offset + 9)) return 0;

    memcpy(buffer, name + offset, 9);
    buffer[9] = '\0';

    if (!safetyIsValidTitleId(buffer)) return 0;

    safe_strncpy(titleOut, buffer, titleSize);
    return 1;
}

static int orphanEvaluate(OrphanSource source, const char *name, const SceDateTime *mtime, char *titleOut, size_t titleSize) {
    switch (source) {
        case ORPHAN_SRC_DATA:
            if (!orphanMakeTitleId(name, 0, titleOut, titleSize)) {
                if (isSystemDataDirName(name)) return 0;
                if (!isOlderThanDays(mtime, 30)) return 0;
                safe_strncpy(titleOut, name, titleSize);
                return 2;
            }
            if (!isOlderThanDays(mtime, 30)) return 0;
            break;
        case ORPHAN_SRC_ADDCONT:
        case ORPHAN_SRC_PATCH:
        case ORPHAN_SRC_REPATCH:
        case ORPHAN_SRC_READDCONT:
            if (!orphanMakeTitleId(name, 0, titleOut, titleSize)) return 0;
            if (source != ORPHAN_SRC_ADDCONT && source != ORPHAN_SRC_READDCONT && !isOlderThanDays(mtime, 30)) return 0;
            break;
        case ORPHAN_SRC_APPMETA:
            if (strncmp(name, "NPXS", 4) == 0) return 0;
            if (!orphanMakeTitleId(name, 0, titleOut, titleSize)) return 0;
            if (!isOlderThanDays(mtime, 30)) return 0;
            break;
        case ORPHAN_SRC_LICENSE_DIR:
            if (strlen(name) != 16) return 0;
            if (!orphanMakeTitleId(name, 7, titleOut, titleSize)) return 0;
            if (!isOlderThanDays(mtime, 30)) return 0;
            break;
        case ORPHAN_SRC_LICENSE_RIF:
            if (strlen(name) < 4 || strcasecmp(name + strlen(name) - 4, ".rif") != 0) return 0;
            if (strlen(name) == 20) {
                if (!orphanMakeTitleId(name, 0, titleOut, titleSize)) return 2;
            } else if (strlen(name) == 16) {
                if (!orphanMakeTitleId(name, 7, titleOut, titleSize)) return 2;
                if (!isOlderThanDays(mtime, 60)) return 0;
            } else {
                return 0;
            }
            break;
        default:
            return 0;
    }

    if (isTitleIdInstalled(titleOut)) return 0;
    return 1;
}

static void orphanAddItem(const char *path, const char *titleId, unsigned long long size, int certain) {
    if (g_orphanItemCount >= ORPHAN_MAX_ITEMS) {
        g_orphanTruncated = 1;
        return;
    }

    if (whitelistCheck(path, titleId)) {
        g_orphanSkipped++;
        return;
    }

    SafetyItem item;
    safetyItemInitWithTitle(&item, path, size,
                            certain ? SAFETY_ORPHAN_CERTAIN : SAFETY_ORPHAN_UNCERTAIN,
                            titleId);
    resolveTitleName(titleId, item.appName, sizeof(item.appName));
    item.user_selected = 0;
    safetyRecompute(&item);

    memcpy(&g_orphanItems[g_orphanItemCount], &item, sizeof(SafetyItem));
    g_orphanItemCount++;
}

static void orphanAddProtectedItem(const char *path, const char *titleId, unsigned long long size) {
    if (g_orphanItemCount >= ORPHAN_MAX_ITEMS) {
        g_orphanTruncated = 1;
        return;
    }

    if (whitelistCheck(path, titleId)) {
        g_orphanSkipped++;
        return;
    }

    SafetyItem item;
    safetyItemInitWithTitle(&item, path, size, SAFETY_ORPHAN_NONE, titleId);
    resolveTitleName(titleId, item.appName, sizeof(item.appName));
    item.category = CAT_PROTECTED;
    item.group = GRP_PROTECTED;
    item.certainty = SAFETY_CERTAINTY_NONE;
    item.user_selected = 0;
    safetySetItemReason(&item, "user save data, protected");
    safetyRecompute(&item);

    memcpy(&g_orphanItems[g_orphanItemCount], &item, sizeof(SafetyItem));
    g_orphanItemCount++;
    g_orphanProtected++;
}

static void orphanScanDirectoryRoot(const char *root, OrphanSource source, int dirsOnly) {
    SceUID dfd = sceIoDopen(root);
    if (dfd < 0) return;

    SceIoDirent dir;
    memset(&dir, 0, sizeof(dir));

    char rootPrefix[SAFETY_MAX_PATH];
    safe_snprintf(rootPrefix, sizeof(rootPrefix), "%s%s", root,
                  (root[strlen(root) - 1] == '/') ? "" : "/");

    while (sceIoDread(dfd, &dir) > 0) {
        if (isEmergencyStopRequested()) break;
        if (strcmp(dir.d_name, ".") == 0 || strcmp(dir.d_name, "..") == 0) continue;

        int isDir = SCE_S_ISDIR(dir.d_stat.st_mode);
        if (dirsOnly && !isDir) continue;
        if (!dirsOnly && isDir) continue;

        char titleId[SAFETY_MAX_TITLE] = {0};

        int verdict = orphanEvaluate(source, dir.d_name, &dir.d_stat.st_mtime, titleId, sizeof(titleId));
        if (verdict == 0) {
            g_orphanSkipped++;
            continue;
        }

        char fullPath[SAFETY_MAX_PATH];
        safe_strncpy(fullPath, rootPrefix, sizeof(fullPath));
        safe_strncat(fullPath, dir.d_name, sizeof(fullPath));

        if (whitelistCheck(fullPath, titleId)) {
            g_orphanSkipped++;
            continue;
        }

        unsigned long long size = isDir ? measurePath(fullPath) : (unsigned long long)dir.d_stat.st_size;

        if (verdict == 2) {
            orphanAddItem(fullPath, titleId, size, 0);
        } else {
            orphanAddItem(fullPath, titleId, size, 1);
        }
    }

    sceIoDclose(dfd);
}

static void orphanScanSaveData() {
    const char *saveRoots[] = {
        "ux0:user/00/savedata/",
        "uma0:user/00/savedata/",
        "imc0:user/00/savedata/"
    };

    for (size_t r = 0; r < sizeof(saveRoots) / sizeof(saveRoots[0]); r++) {
        SceUID dfd = sceIoDopen(saveRoots[r]);
        if (dfd < 0) continue;

        SceIoDirent dir;
        memset(&dir, 0, sizeof(dir));

        while (sceIoDread(dfd, &dir) > 0) {
            if (isEmergencyStopRequested()) break;
            if (strcmp(dir.d_name, ".") == 0 || strcmp(dir.d_name, "..") == 0) continue;
            if (!SCE_S_ISDIR(dir.d_stat.st_mode)) continue;
            if (!safetyIsValidTitleId(dir.d_name)) continue;
            if (isTitleIdInstalled(dir.d_name)) continue;

            char fullPath[SAFETY_MAX_PATH];
            safe_snprintf(fullPath, sizeof(fullPath), "%s%s", saveRoots[r], dir.d_name);

            orphanAddProtectedItem(fullPath, dir.d_name, measurePath(fullPath));

            if (g_orphanItemCount >= ORPHAN_MAX_ITEMS) break;
        }

        sceIoDclose(dfd);
    }
}

typedef struct {
    const char *root;
    OrphanSource source;
    int dirsOnly;
} OrphanRootSpec;

static const OrphanRootSpec ORPHAN_ROOTS[] = {
    {"ux0:data/", ORPHAN_SRC_DATA, 1},
    {"ux0:addcont/", ORPHAN_SRC_ADDCONT, 1},
    {"ux0:patch/", ORPHAN_SRC_PATCH, 1},
    {"ux0:appmeta/", ORPHAN_SRC_APPMETA, 1},
    {"ux0:license/", ORPHAN_SRC_LICENSE_RIF, 0},
    {"ux0:license/", ORPHAN_SRC_LICENSE_DIR, 1},
    {"ux0:rePatch/", ORPHAN_SRC_REPATCH, 1},
    {"ux0:reAddcont/", ORPHAN_SRC_READDCONT, 1},
    {"uma0:data/", ORPHAN_SRC_DATA, 1},
    {"uma0:addcont/", ORPHAN_SRC_ADDCONT, 1},
    {"uma0:patch/", ORPHAN_SRC_PATCH, 1},
    {"uma0:appmeta/", ORPHAN_SRC_APPMETA, 1},
    {"uma0:license/", ORPHAN_SRC_LICENSE_RIF, 0},
    {"uma0:license/", ORPHAN_SRC_LICENSE_DIR, 1},
    {"uma0:rePatch/", ORPHAN_SRC_REPATCH, 1},
    {"imc0:data/", ORPHAN_SRC_DATA, 1},
    {"imc0:addcont/", ORPHAN_SRC_ADDCONT, 1},
    {"imc0:patch/", ORPHAN_SRC_PATCH, 1},
    {"imc0:appmeta/", ORPHAN_SRC_APPMETA, 1},
    {"imc0:license/", ORPHAN_SRC_LICENSE_RIF, 0},
    {"imc0:license/", ORPHAN_SRC_LICENSE_DIR, 1}
};

void orphanScanRun() {
    g_orphanItemCount = 0;
    g_orphanTruncated = 0;
    g_orphanSkipped = 0;
    g_orphanProtected = 0;

    int totalSteps = (int)(sizeof(ORPHAN_ROOTS) / sizeof(ORPHAN_ROOTS[0])) + 1;
    initScanProgress(totalSteps);

    for (size_t i = 0; i < sizeof(ORPHAN_ROOTS) / sizeof(ORPHAN_ROOTS[0]); i++) {
        if (isEmergencyStopRequested()) break;
        orphanScanDirectoryRoot(ORPHAN_ROOTS[i].root, ORPHAN_ROOTS[i].source, ORPHAN_ROOTS[i].dirsOnly);
        updateScanProgress((int)(i + 1));
    }

    if (!isEmergencyStopRequested()) {
        orphanScanSaveData();
    }
    updateScanProgress(totalSteps);
}

static unsigned int maintenanceElapsedSeconds(const SceRtcTick *start, const SceRtcTick *end) {
    unsigned long long ticks = end->tick - start->tick;
    unsigned int resolution = sceRtcGetTickResolution();

    if (resolution == 0) resolution = 1000000;
    return (unsigned int)(ticks / resolution);
}

static int orphanDeleteFile(void *ctx, const char *path, unsigned long long size) {
    SafetyItem *root = (SafetyItem *)ctx;
    SafetyItem item;
    char reason[SAFETY_MAX_REASON];

    safetyItemInitWithTitle(&item, path, size,
                            (root->category == CAT_ORPHANED) ? SAFETY_ORPHAN_CERTAIN : SAFETY_ORPHAN_UNCERTAIN,
                            root->titleId);
    item.user_selected = 1;
    safetyRecompute(&item);

    if (!safetyVerifyDelete(&item, reason, sizeof(reason))) return 2;
    return 1;
}

void orphanDeleteSelected() {
    CleanReport report;
    SceRtcTick start;
    SceRtcTick end;

    memset(&report, 0, sizeof(report));
    sceRtcGetCurrentTick(&start);

    int selected = 0;
    for (int i = 0; i < g_orphanItemCount; i++) {
        if (g_orphanItems[i].user_selected) selected++;
    }

    initScanProgress(selected > 0 ? selected : 1);

    int processed = 0;
    for (int i = 0; i < g_orphanItemCount; i++) {
        if (isEmergencyStopRequested()) break;

        SafetyItem *item = &g_orphanItems[i];
        if (!item->user_selected) continue;

        char titleId[SAFETY_MAX_TITLE];
        safe_strncpy(titleId, item->titleId, sizeof(titleId));

        int certain = safetyIsValidTitleId(titleId) && !isTitleIdInstalled(titleId);

        SafetyItem fresh;
        safetyItemInitWithTitle(&fresh, item->path, item->size,
                                certain ? SAFETY_ORPHAN_CERTAIN : SAFETY_ORPHAN_UNCERTAIN,
                                titleId);
        fresh.user_selected = 1;
        safetyRecompute(&fresh);

        char reason[SAFETY_MAX_REASON];
        if (!safetyVerifyDelete(&fresh, reason, sizeof(reason))) {
            report.filesSkipped++;
            processed++;
            updateScanProgress(processed);
            continue;
        }

        ScanWalker walker;
        memset(&walker, 0, sizeof(walker));
        walker.allowDelete = 1;
        walker.ctx = &fresh;
        walker.fileFn = orphanDeleteFile;

        scanTree(&walker, item->path, 0);

        report.filesScanned += walker.filesSeen;
        report.filesDeleted += walker.filesDeleted;
        report.filesSkipped += walker.filesSkipped;
        report.filesFailed += walker.filesFailed + walker.dirsFailed;
        report.bytesRecovered += walker.bytesDeleted;

        if (walker.filesDeleted > 0 || walker.dirsDeleted > 0) {
            item->user_selected = 0;
            item->size = 0;
        }

        processed++;
        updateScanProgress(processed);
    }

    int writeIdx = 0;
    for (int i = 0; i < g_orphanItemCount; i++) {
        if (g_orphanItems[i].size > 0 || g_orphanItems[i].category == CAT_PROTECTED) {
            SceIoStat st;
            if (sceIoGetstat(g_orphanItems[i].path, &st) >= 0) {
                if (writeIdx != i) g_orphanItems[writeIdx] = g_orphanItems[i];
                writeIdx++;
            }
        }
    }
    g_orphanItemCount = writeIdx;

    updateScanProgress(selected > 0 ? selected : 1);
    sceRtcGetCurrentTick(&end);
    report.elapsedSeconds = maintenanceElapsedSeconds(&start, &end);

    g_cleanReport = report;
    invalidateSpaceCache();
}

static void maintenanceResetWalker(ScanWalker *walker, ScanFileFn fileFn, void *ctx, int allowDelete) {
    memset(walker, 0, sizeof(*walker));
    walker->fileFn = fileFn;
    walker->ctx = ctx;
    walker->allowDelete = allowDelete;
}

static int smartScanFile(void *ctx, const char *path, unsigned long long size) {
    (void)ctx;

    if (whitelistCheck(path, NULL)) {
        g_smartCleanStats.filesSkipped++;
        return 0;
    }

    SafetyItem item;
    safetyItemInit(&item, path, size, SAFETY_ORPHAN_NONE);
    safetyStatsAdd(&g_smartCleanStats, &item);
    safetyPlanAdd(&g_smartCleanPlan, &item);

    return 0;
}

static void smartScanPath(const char *path) {
    if (whitelistCheck(path, NULL)) return;

    if (strchr(path, '*') != NULL) {
        const char *slash = strrchr(path, '/');
        const char *colon = strrchr(path, ':');
        const char *split = slash ? slash : colon;
        if (!split) return;
        char dirPath[SAFETY_MAX_PATH];
        size_t dirLen = (size_t)(split - path) + 1;
        if (dirLen >= sizeof(dirPath)) return;
        memcpy(dirPath, path, dirLen);
        dirPath[dirLen] = '\0';
        if (split == colon) safe_strncat(dirPath, "/", sizeof(dirPath));
        const char *glob = split + 1;

        SceUID dfd = sceIoDopen(dirPath);
        if (dfd < 0) return;
        SceIoDirent dir;
        memset(&dir, 0, sizeof(dir));
        while (sceIoDread(dfd, &dir) > 0) {
            if (SCE_S_ISDIR(dir.d_stat.st_mode)) continue;
            if (!safetyMatchWildcard(glob, dir.d_name)) continue;
            char full[SAFETY_MAX_PATH];
            safe_snprintf(full, sizeof(full), "%s%s", dirPath, dir.d_name);
            smartScanFile(NULL, full, (unsigned long long)dir.d_stat.st_size);
        }
        sceIoDclose(dfd);
        return;
    }

    ScanWalker walker;
    SceIoStat stat;

    if (sceIoGetstat(path, &stat) < 0) return;

    maintenanceResetWalker(&walker, smartScanFile, NULL, 0);
    scanTree(&walker, path, 0);

    g_smartCleanStats.filesSkipped += walker.filesSkipped;
}

static void smartScanDumpRoots() {
    for (size_t i = 0; i < sizeof(DUMP_SEARCH_ROOTS) / sizeof(DUMP_SEARCH_ROOTS[0]); i++) {
        SceUID dfd = sceIoDopen(DUMP_SEARCH_ROOTS[i]);
        if (dfd < 0) continue;

        SceIoDirent dir;
        memset(&dir, 0, sizeof(dir));

        char rootPrefix[SAFETY_MAX_PATH];
        safe_snprintf(rootPrefix, sizeof(rootPrefix), "%s%s", DUMP_SEARCH_ROOTS[i],
                      (DUMP_SEARCH_ROOTS[i][strlen(DUMP_SEARCH_ROOTS[i]) - 1] == '/') ? "" : "/");

        while (sceIoDread(dfd, &dir) > 0) {
            if (isEmergencyStopRequested()) break;
            if (SCE_S_ISDIR(dir.d_stat.st_mode)) continue;
            if (!safetyIsDumpFileName(dir.d_name)) continue;

            char fullPath[SAFETY_MAX_PATH];
            safe_strncpy(fullPath, rootPrefix, sizeof(fullPath));
            safe_strncat(fullPath, dir.d_name, sizeof(fullPath));
            if (whitelistCheck(fullPath, NULL)) continue;

            SafetyItem item;
            safetyItemInit(&item, fullPath, (unsigned long long)dir.d_stat.st_size, SAFETY_ORPHAN_NONE);
            safetyStatsAdd(&g_smartCleanStats, &item);
            safetyPlanAdd(&g_smartCleanPlan, &item);
        }

        sceIoDclose(dfd);
    }
}

static void smartScanAppTempFiles() {
    char **apps = NULL;
    int appCount = 0;

    getInstalledAppsList(&apps, &appCount);
    if (!apps) return;

    const char *appDirs[] = {"ux0:data/%s", "ux0:patch/%s", "ux0:addcont/%s", "ux0:rePatch/%s", "uma0:data/%s", "imc0:data/%s"};

    for (int i = 0; i < appCount; i++) {
        if (!isEmergencyStopRequested() && !whitelistCheck(NULL, apps[i])) {
            for (size_t d = 0; d < sizeof(appDirs) / sizeof(appDirs[0]); d++) {
                char appPath[MAX_PATH_LENGTH];
                safe_snprintf(appPath, sizeof(appPath), appDirs[d], apps[i]);

                SceIoStat stat;
                if (sceIoGetstat(appPath, &stat) < 0) continue;
                if (!SCE_S_ISDIR(stat.st_mode)) continue;

                for (size_t t = 0; t < sizeof(APP_TEMP_DIRS) / sizeof(APP_TEMP_DIRS[0]); t++) {
                    if (isEmergencyStopRequested()) break;

                    char tempPath[MAX_PATH_LENGTH];
                    safe_snprintf(tempPath, sizeof(tempPath), "%s/%s", appPath, APP_TEMP_DIRS[t]);
                    if (sceIoGetstat(tempPath, &stat) < 0) continue;

                    smartScanPath(tempPath);
                }
            }
        }

        free(apps[i]);
    }

    free(apps);
}

static void smartScanOrphanRoot(const char *root, OrphanSource source, int dirsOnly) {
    SceUID dfd = sceIoDopen(root);
    if (dfd < 0) return;

    SceIoDirent dir;
    memset(&dir, 0, sizeof(dir));

    char rootPrefix[SAFETY_MAX_PATH];
    safe_snprintf(rootPrefix, sizeof(rootPrefix), "%s%s", root,
                  (root[strlen(root) - 1] == '/') ? "" : "/");

    while (sceIoDread(dfd, &dir) > 0) {
        if (isEmergencyStopRequested()) break;
        if (strcmp(dir.d_name, ".") == 0 || strcmp(dir.d_name, "..") == 0) continue;

        int isDir = SCE_S_ISDIR(dir.d_stat.st_mode);
        if (dirsOnly && !isDir) continue;
        if (!dirsOnly && isDir) continue;

        char titleId[SAFETY_MAX_TITLE] = {0};
        int verdict = orphanEvaluate(source, dir.d_name, &dir.d_stat.st_mtime, titleId, sizeof(titleId));

        if (verdict == 0) {
            g_smartCleanStats.filesSkipped++;
            continue;
        }

        char fullPath[SAFETY_MAX_PATH];
        safe_strncpy(fullPath, rootPrefix, sizeof(fullPath));
        safe_strncat(fullPath, dir.d_name, sizeof(fullPath));

        if (whitelistCheck(fullPath, titleId)) {
            g_smartCleanStats.filesSkipped++;
            continue;
        }

        unsigned long long size = isDir ? measurePath(fullPath) : (unsigned long long)dir.d_stat.st_size;

        SafetyItem item;
        safetyItemInitWithTitle(&item, fullPath, size,
                                (verdict == 1) ? SAFETY_ORPHAN_CERTAIN : SAFETY_ORPHAN_UNCERTAIN,
                                titleId);
        safetyStatsAdd(&g_smartCleanStats, &item);
        safetyPlanAdd(&g_smartCleanPlan, &item);
    }

    sceIoDclose(dfd);
}

static int smartCleanFile(void *ctx, const char *path, unsigned long long size) {
    (void)ctx;

    SafetyItem item;
    char reason[SAFETY_MAX_REASON];

    safetyItemInit(&item, path, size, SAFETY_ORPHAN_NONE);
    if (!safetyGroupDeletable(item.group, &g_smartCleanPlan)) return 2;
    if (!safetyVerifyDelete(&item, reason, sizeof(reason))) return 2;

    return 1;
}

static int smartCleanOrphanFile(void *ctx, const char *path, unsigned long long size) {
    if (!g_smartCleanPlan.enabled[GRP_ORPHANED]) return 2;
    return orphanDeleteFile(ctx, path, size);
}

static void smartCleanAddResults(CleanReport *report, const ScanWalker *walker) {
    report->filesScanned += walker->filesSeen;
    report->filesDeleted += walker->filesDeleted;
    report->filesSkipped += walker->filesSkipped;
    report->filesFailed += walker->filesFailed;
    report->bytesRecovered += walker->bytesDeleted;
}

static void smartCleanPath(const char *path, CleanReport *report) {
    if (whitelistCheck(path, NULL)) return;

    if (strchr(path, '*') != NULL) {
        const char *slash = strrchr(path, '/');
        const char *colon = strrchr(path, ':');
        const char *split = slash ? slash : colon;
        if (!split) return;
        char dirPath[SAFETY_MAX_PATH];
        size_t dirLen = (size_t)(split - path) + 1;
        if (dirLen >= sizeof(dirPath)) return;
        memcpy(dirPath, path, dirLen);
        dirPath[dirLen] = '\0';
        if (split == colon) safe_strncat(dirPath, "/", sizeof(dirPath));
        const char *glob = split + 1;

        SceUID dfd = sceIoDopen(dirPath);
        if (dfd < 0) return;
        char **matches = NULL;
        unsigned long long *sizes = NULL;
        int count = 0, cap = 0;
        SceIoDirent dir;
        memset(&dir, 0, sizeof(dir));
        while (sceIoDread(dfd, &dir) > 0) {
            if (SCE_S_ISDIR(dir.d_stat.st_mode)) continue;
            if (!safetyMatchWildcard(glob, dir.d_name)) continue;
            if (count >= cap) {
                int nc = cap ? cap * 2 : 16;
                char **nm = (char **)realloc(matches, sizeof(char *) * nc);
                unsigned long long *ns = (unsigned long long *)realloc(sizes, sizeof(unsigned long long) * nc);
                if (!nm || !ns) {
                    if (nm) matches = nm;
                    if (ns) sizes = ns;
                    break;
                }
                matches = nm;
                sizes = ns;
                cap = nc;
            }
            matches[count] = strdup(dir.d_name);
            sizes[count] = (unsigned long long)dir.d_stat.st_size;
            if (matches[count]) count++;
        }
        sceIoDclose(dfd);

        for (int i = 0; i < count; i++) {
            if (!isEmergencyStopRequested()) {
                char full[SAFETY_MAX_PATH];
                safe_snprintf(full, sizeof(full), "%s%s", dirPath, matches[i]);
                report->filesScanned++;
                SafetyItem item;
                char reason[SAFETY_MAX_REASON];
                safetyItemInit(&item, full, sizes[i], SAFETY_ORPHAN_NONE);
                if (!safetyGroupDeletable(item.group, &g_smartCleanPlan) || !safetyVerifyDelete(&item, reason, sizeof(reason))) {
                    report->filesSkipped++;
                } else if (sceIoRemove(full) >= 0) {
                    report->filesDeleted++;
                    report->bytesRecovered += sizes[i];
                    incrementDeletedFilesCount();
                } else {
                    report->filesFailed++;
                }
            }
            free(matches[i]);
        }
        if (matches) free(matches);
        if (sizes) free(sizes);
        return;
    }

    ScanWalker walker;
    SceIoStat stat;

    if (sceIoGetstat(path, &stat) < 0) return;

    maintenanceResetWalker(&walker, smartCleanFile, NULL, 1);
    scanTree(&walker, path, 0);
    smartCleanAddResults(report, &walker);
}

static void smartCleanDumpRoots(CleanReport *report) {
    for (size_t i = 0; i < sizeof(DUMP_SEARCH_ROOTS) / sizeof(DUMP_SEARCH_ROOTS[0]); i++) {
        if (isEmergencyStopRequested()) break;

        SceUID dfd = sceIoDopen(DUMP_SEARCH_ROOTS[i]);
        if (dfd < 0) continue;

        SceIoDirent dir;
        memset(&dir, 0, sizeof(dir));

        char rootPrefix[SAFETY_MAX_PATH];
        safe_snprintf(rootPrefix, sizeof(rootPrefix), "%s%s", DUMP_SEARCH_ROOTS[i],
                      (DUMP_SEARCH_ROOTS[i][strlen(DUMP_SEARCH_ROOTS[i]) - 1] == '/') ? "" : "/");

        while (sceIoDread(dfd, &dir) > 0) {
            if (isEmergencyStopRequested()) break;
            if (SCE_S_ISDIR(dir.d_stat.st_mode)) continue;
            if (!safetyIsDumpFileName(dir.d_name)) continue;

            char fullPath[SAFETY_MAX_PATH];
            safe_strncpy(fullPath, rootPrefix, sizeof(fullPath));
            safe_strncat(fullPath, dir.d_name, sizeof(fullPath));

            SafetyItem item;
            char reason[SAFETY_MAX_REASON];
            safetyItemInit(&item, fullPath, (unsigned long long)dir.d_stat.st_size, SAFETY_ORPHAN_NONE);

            report->filesScanned++;

            if (!safetyGroupDeletable(item.group, &g_smartCleanPlan)) {
                report->filesSkipped++;
                continue;
            }

            if (!safetyVerifyDelete(&item, reason, sizeof(reason))) {
                report->filesSkipped++;
                continue;
            }

            if (sceIoRemove(fullPath) >= 0) {
                report->filesDeleted++;
                report->bytesRecovered += (unsigned long long)dir.d_stat.st_size;
                incrementDeletedFilesCount();
            } else {
                report->filesFailed++;
            }
        }

        sceIoDclose(dfd);
    }
}

static void smartCleanAppTempFiles(CleanReport *report) {
    char **apps = NULL;
    int appCount = 0;

    getInstalledAppsList(&apps, &appCount);
    if (!apps) return;

    const char *appDirs[] = {"ux0:data/%s", "ux0:patch/%s", "ux0:addcont/%s", "ux0:rePatch/%s", "uma0:data/%s", "imc0:data/%s"};

    for (int i = 0; i < appCount; i++) {
        if (!isEmergencyStopRequested() && !whitelistCheck(NULL, apps[i])) {
            for (size_t d = 0; d < sizeof(appDirs) / sizeof(appDirs[0]); d++) {
                char appPath[MAX_PATH_LENGTH];
                safe_snprintf(appPath, sizeof(appPath), appDirs[d], apps[i]);

                SceIoStat stat;
                if (sceIoGetstat(appPath, &stat) < 0) continue;
                if (!SCE_S_ISDIR(stat.st_mode)) continue;

                for (size_t t = 0; t < sizeof(APP_TEMP_DIRS) / sizeof(APP_TEMP_DIRS[0]); t++) {
                    if (isEmergencyStopRequested()) break;

                    char tempPath[MAX_PATH_LENGTH];
                    safe_snprintf(tempPath, sizeof(tempPath), "%s/%s", appPath, APP_TEMP_DIRS[t]);
                    smartCleanPath(tempPath, report);
                }
            }
        }

        free(apps[i]);
    }

    free(apps);
}

static void smartCleanOrphanRoot(const char *root, OrphanSource source, int dirsOnly, CleanReport *report) {
    SceUID dfd = sceIoDopen(root);
    if (dfd < 0) return;

    typedef struct {
        char *name;
        int isDir;
        SceDateTime mtime;
        unsigned long long size;
    } OrphanCandidate;

    OrphanCandidate *candidates = NULL;
    int count = 0, capacity = 0;

    SceIoDirent dir;
    memset(&dir, 0, sizeof(dir));

    while (sceIoDread(dfd, &dir) > 0) {
        if (strcmp(dir.d_name, ".") == 0 || strcmp(dir.d_name, "..") == 0) continue;

        int isDir = SCE_S_ISDIR(dir.d_stat.st_mode);
        if (dirsOnly && !isDir) continue;
        if (!dirsOnly && isDir) continue;

        if (count >= capacity) {
            int newCap = capacity ? capacity * 2 : 32;
            OrphanCandidate *newCands = (OrphanCandidate *)realloc(candidates, newCap * sizeof(OrphanCandidate));
            if (!newCands) break;
            candidates = newCands;
            capacity = newCap;
        }

        candidates[count].name = strdup(dir.d_name);
        if (candidates[count].name) {
            candidates[count].isDir = isDir;
            candidates[count].mtime = dir.d_stat.st_mtime;
            candidates[count].size = (unsigned long long)dir.d_stat.st_size;
            count++;
        }
    }

    sceIoDclose(dfd);

    char rootPrefix[SAFETY_MAX_PATH];
    safe_snprintf(rootPrefix, sizeof(rootPrefix), "%s%s", root,
                  (root[strlen(root) - 1] == '/') ? "" : "/");

    for (int i = 0; i < count; i++) {
        if (isEmergencyStopRequested()) {
            free(candidates[i].name);
            continue;
        }

        char titleId[SAFETY_MAX_TITLE] = {0};
        int verdict = orphanEvaluate(source, candidates[i].name, &candidates[i].mtime, titleId, sizeof(titleId));

        if (verdict != 0) {
            char fullPath[SAFETY_MAX_PATH];
            safe_strncpy(fullPath, rootPrefix, sizeof(fullPath));
            safe_strncat(fullPath, candidates[i].name, sizeof(fullPath));

            report->filesScanned++;

            if (whitelistCheck(fullPath, titleId)) {
                report->filesSkipped++;
                free(candidates[i].name);
                continue;
            }

            unsigned long long itemSize = candidates[i].isDir ? measurePath(fullPath) : candidates[i].size;

            SafetyItem item;
            safetyItemInitWithTitle(&item, fullPath, itemSize,
                                    (verdict == 1) ? SAFETY_ORPHAN_CERTAIN : SAFETY_ORPHAN_UNCERTAIN,
                                    titleId);
            item.user_selected = 0;
            safetyRecompute(&item);

            if (safetyGroupDeletable(item.group, &g_smartCleanPlan)) {
                char reason[SAFETY_MAX_REASON];
                if (safetyVerifyDelete(&item, reason, sizeof(reason))) {
                    ScanWalker walker;
                    maintenanceResetWalker(&walker, smartCleanOrphanFile, &item, 1);
                    scanTree(&walker, fullPath, 0);
                    smartCleanAddResults(report, &walker);
                } else {
                    report->filesSkipped++;
                }
            } else {
                report->filesSkipped++;
            }
        }

        free(candidates[i].name);
    }

    if (candidates) free(candidates);
}

static void smartScanSaveData() {
    const char *saveRoots[] = {"ux0:user/00/savedata/", "uma0:user/00/savedata/", "imc0:user/00/savedata/"};
    for (size_t r = 0; r < sizeof(saveRoots) / sizeof(saveRoots[0]); r++) {
        SceUID dfd = sceIoDopen(saveRoots[r]);
        if (dfd < 0) continue;

        SceIoDirent dir;
        memset(&dir, 0, sizeof(dir));

        while (sceIoDread(dfd, &dir) > 0) {
            if (isEmergencyStopRequested()) break;
            if (strcmp(dir.d_name, ".") == 0 || strcmp(dir.d_name, "..") == 0) continue;
            if (!SCE_S_ISDIR(dir.d_stat.st_mode)) continue;
            if (!safetyIsValidTitleId(dir.d_name)) continue;
            if (isTitleIdInstalled(dir.d_name)) continue;

            char fullPath[SAFETY_MAX_PATH];
            safe_snprintf(fullPath, sizeof(fullPath), "%s%s", saveRoots[r], dir.d_name);

            SafetyItem item;
            safetyItemInitWithTitle(&item, fullPath, measurePath(fullPath), SAFETY_ORPHAN_NONE, dir.d_name);
            item.category = CAT_PROTECTED;
            item.group = GRP_PROTECTED;
            item.certainty = SAFETY_CERTAINTY_NONE;
            safetySetItemReason(&item, "user save data, protected");
            safetyRecompute(&item);

            safetyStatsAdd(&g_smartCleanStats, &item);
            safetyPlanAdd(&g_smartCleanPlan, &item);
        }

        sceIoDclose(dfd);
    }
}

static int smartCleanStepCount() {
    int steps = (int)TEMP_PATHS_COUNT;
    steps += 2;
    steps += (int)(sizeof(ORPHAN_ROOTS) / sizeof(ORPHAN_ROOTS[0]));
    steps += 1;
    return steps > 0 ? steps : 1;
}

void smartCleanReset() {
    safetyPlanResetDefaults(&g_smartCleanPlan);
    memset(&g_smartCleanStats, 0, sizeof(g_smartCleanStats));
    memset(&g_cleanReport, 0, sizeof(g_cleanReport));
}

void smartCleanScan() {
    int steps = smartCleanStepCount();
    int processed = 0;

    smartCleanReset();
    initScanProgress(steps);

    for (size_t i = 0; i < TEMP_PATHS_COUNT; i++) {
        if (isEmergencyStopRequested()) break;
        smartScanPath(TEMP_PATHS[i]);
        processed++;
        updateScanProgress(processed);
    }

    smartScanDumpRoots();
    updateScanProgress(++processed);

    smartScanAppTempFiles();
    updateScanProgress(++processed);

    for (size_t i = 0; i < sizeof(ORPHAN_ROOTS) / sizeof(ORPHAN_ROOTS[0]); i++) {
        if (isEmergencyStopRequested()) break;
        smartScanOrphanRoot(ORPHAN_ROOTS[i].root, ORPHAN_ROOTS[i].source, ORPHAN_ROOTS[i].dirsOnly);
        updateScanProgress(++processed);
    }

    smartScanSaveData();
    updateScanProgress(++processed);

    updateScanProgress(steps);
}

void smartCleanRun() {
    CleanReport report;
    SceRtcTick start;
    SceRtcTick end;

    int steps = smartCleanStepCount();
    int processed = 0;

    memset(&report, 0, sizeof(report));
    sceRtcGetCurrentTick(&start);
    initScanProgress(steps);

    for (size_t i = 0; i < TEMP_PATHS_COUNT; i++) {
        if (isEmergencyStopRequested()) break;
        smartCleanPath(TEMP_PATHS[i], &report);
        processed++;
        updateScanProgress(processed);
    }

    smartCleanDumpRoots(&report);
    updateScanProgress(++processed);

    smartCleanAppTempFiles(&report);
    updateScanProgress(++processed);

    for (size_t i = 0; i < sizeof(ORPHAN_ROOTS) / sizeof(ORPHAN_ROOTS[0]); i++) {
        if (isEmergencyStopRequested()) break;
        smartCleanOrphanRoot(ORPHAN_ROOTS[i].root, ORPHAN_ROOTS[i].source, ORPHAN_ROOTS[i].dirsOnly, &report);
        updateScanProgress(++processed);
    }

    updateScanProgress(steps);
    sceRtcGetCurrentTick(&end);
    report.elapsedSeconds = maintenanceElapsedSeconds(&start, &end);

    g_cleanReport = report;
    invalidateSpaceCache();
}

void maintenanceInit() {
    whitelistLoad();
    historyLoad();
    safetySetWhitelistCallback(whitelistCheck);
    safetyInit(TEMP_PATHS, TEMP_PATHS_COUNT);
    analyzerPrepare();
}
