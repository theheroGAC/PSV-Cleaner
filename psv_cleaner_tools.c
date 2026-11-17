#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/rtc.h>
#include <zlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "psv_cleaner_tools.h"

#define WHITELIST_FILE_PATH "ux0:data/PSV_Cleaner/whitelist.txt"
#define HISTORY_FILE_PATH "ux0:data/PSV_Cleaner/history.log"
#define STATS_FILE_PATH "ux0:data/PSV_Cleaner/stats.bin"
#define STATS_VERSION 1
#define TITLE_CACHE_MAX 256
#define LARGE_SCAN_SLOTS 512

typedef struct {
    char titleId[12];
    char appName[64];
} TitleCacheEntry;

static TitleCacheEntry s_titleCache[TITLE_CACHE_MAX];
static int s_titleCacheCount = 0;

static char s_whitelist[WHITELIST_MAX_ITEMS][MAX_PATH_LENGTH];
static int s_whitelistCount = 0;
static int s_whitelistLoaded = 0;

static HistoryEntry s_history[HISTORY_MAX_ENTRIES];
static int s_historyCount = 0;
static int s_historyLoaded = 0;
static LifetimeStats s_lifetimeStats = {0, 0, 0};

TaiAuditState g_taiAudit;
LargeFileItem g_largeItems[LARGE_MAX_ITEMS];
volatile int g_largeItemCount = 0;
volatile int g_largeTruncated = 0;
int g_largeMinSizeMB = 50;

static void trimString(char *s) {
    if (!s) return;
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\r' || s[len - 1] == '\n' || s[len - 1] == ' ' || s[len - 1] == '\t')) {
        s[--len] = '\0';
    }
    char *start = s;
    while (*start == ' ' || *start == '\t') start++;
    if (start != s) {
        memmove(s, start, strlen(start) + 1);
    }
}

static unsigned short readLE16(const unsigned char *p) {
    return (unsigned short)(p[0] | ((unsigned short)p[1] << 8));
}

static unsigned int readLE32(const unsigned char *p) {
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) | ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

static void writeLE16(unsigned char *p, unsigned short v) {
    p[0] = (unsigned char)(v & 0xFF);
    p[1] = (unsigned char)((v >> 8) & 0xFF);
}

static void writeLE32(unsigned char *p, unsigned int v) {
    p[0] = (unsigned char)(v & 0xFF);
    p[1] = (unsigned char)((v >> 8) & 0xFF);
    p[2] = (unsigned char)((v >> 16) & 0xFF);
    p[3] = (unsigned char)((v >> 24) & 0xFF);
}

int sfoReadTitle(const char *sfoPath, char *outTitle, size_t outSize) {
    if (!sfoPath || !outTitle || outSize == 0) return 0;
    outTitle[0] = '\0';

    SceIoStat stat;
    if (sceIoGetstat(sfoPath, &stat) < 0) return 0;
    if (stat.st_size < 20 || stat.st_size > 65536) return 0;

    SceUID fd = sceIoOpen(sfoPath, SCE_O_RDONLY, 0777);
    if (fd < 0) return 0;

    unsigned int fileSize = (unsigned int)stat.st_size;
    unsigned char *buf = (unsigned char *)malloc(fileSize + 1);
    if (!buf) {
        sceIoClose(fd);
        return 0;
    }

    int rd = sceIoRead(fd, buf, fileSize);
    sceIoClose(fd);
    if (rd != (int)fileSize) {
        free(buf);
        return 0;
    }
    buf[fileSize] = '\0';

    if (readLE32(buf) != 0x46535000U) {
        free(buf);
        return 0;
    }

    unsigned int keyTableStart = readLE32(buf + 8);
    unsigned int dataTableStart = readLE32(buf + 12);
    unsigned int numEntries = readLE32(buf + 16);

    if (keyTableStart >= fileSize || dataTableStart >= fileSize || numEntries > 256) {
        free(buf);
        return 0;
    }
    if (20 + numEntries * 16 > fileSize) {
        free(buf);
        return 0;
    }

    char fallback[64] = {0};
    int found = 0;

    for (unsigned int i = 0; i < numEntries; i++) {
        const unsigned char *entry = buf + 20 + i * 16;
        unsigned int keyOff = readLE16(entry);
        unsigned int dataFmt = readLE16(entry + 2);
        unsigned int dataLen = readLE32(entry + 4);
        unsigned int dataOff = readLE32(entry + 12);

        if (keyTableStart + keyOff >= fileSize) continue;
        if (dataTableStart + dataOff >= fileSize) continue;
        if (dataFmt != 0x0204 && dataFmt != 0x0004) continue;

        const char *key = (const char *)(buf + keyTableStart + keyOff);
        size_t maxKey = fileSize - (keyTableStart + keyOff);
        size_t keyLen = strnlen(key, maxKey);
        if (keyLen == maxKey) continue;

        int isTitle = (strcmp(key, "TITLE") == 0);
        int isStitle = (strcmp(key, "STITLE") == 0);
        if (!isTitle && !isStitle) continue;

        unsigned int avail = fileSize - (dataTableStart + dataOff);
        unsigned int copyLen = dataLen < avail ? dataLen : avail;
        if (copyLen >= outSize) copyLen = (unsigned int)(outSize - 1);

        char tmp[128];
        if (copyLen >= sizeof(tmp)) copyLen = sizeof(tmp) - 1;
        memcpy(tmp, buf + dataTableStart + dataOff, copyLen);
        tmp[copyLen] = '\0';

        for (size_t k = 0; tmp[k]; k++) {
            if (tmp[k] == '\n' || tmp[k] == '\r' || tmp[k] == '\t') tmp[k] = ' ';
        }
        trimString(tmp);

        if (isTitle && tmp[0] != '\0') {
            safe_strncpy(outTitle, tmp, outSize);
            found = 1;
            break;
        } else if (isStitle && tmp[0] != '\0' && fallback[0] == '\0') {
            safe_strncpy(fallback, tmp, sizeof(fallback));
        }
    }

    if (!found && fallback[0] != '\0') {
        safe_strncpy(outTitle, fallback, outSize);
        found = 1;
    }

    free(buf);
    return found;
}

void clearTitleNameCache() {
    s_titleCacheCount = 0;
}

void resolveTitleName(const char *titleId, char *outName, size_t outSize) {
    if (!outName || outSize == 0) return;
    outName[0] = '\0';
    if (!titleId || !safetyIsValidTitleId(titleId)) return;

    for (int i = 0; i < s_titleCacheCount; i++) {
        if (strcasecmp(s_titleCache[i].titleId, titleId) == 0) {
            safe_strncpy(outName, s_titleCache[i].appName, outSize);
            return;
        }
    }

    const char *sfoTemplates[] = {
        "ux0:app/%s/sce_sys/param.sfo",
        "ux0:appmeta/%s/param.sfo",
        "ur0:app/%s/sce_sys/param.sfo",
        "ur0:appmeta/%s/param.sfo",
        "uma0:app/%s/sce_sys/param.sfo",
        "uma0:appmeta/%s/param.sfo",
        "imc0:app/%s/sce_sys/param.sfo",
        "imc0:appmeta/%s/param.sfo",
        "vs0:app/%s/sce_sys/param.sfo"
    };

    char path[MAX_PATH_LENGTH];
    for (size_t i = 0; i < sizeof(sfoTemplates) / sizeof(sfoTemplates[0]); i++) {
        safe_snprintf(path, sizeof(path), sfoTemplates[i], titleId);
        if (sfoReadTitle(path, outName, outSize)) break;
    }

    if (s_titleCacheCount < TITLE_CACHE_MAX) {
        safe_strncpy(s_titleCache[s_titleCacheCount].titleId, titleId, sizeof(s_titleCache[s_titleCacheCount].titleId));
        safe_strncpy(s_titleCache[s_titleCacheCount].appName, outName, sizeof(s_titleCache[s_titleCacheCount].appName));
        s_titleCacheCount++;
    }
}

void whitelistLoad() {
    s_whitelistCount = 0;
    s_whitelistLoaded = 1;

    SceUID fd = sceIoOpen(WHITELIST_FILE_PATH, SCE_O_RDONLY, 0777);
    if (fd < 0) return;

    char buf[8192];
    int rd = sceIoRead(fd, buf, sizeof(buf) - 1);
    sceIoClose(fd);
    if (rd <= 0) return;
    buf[rd] = '\0';

    char *line = buf;
    while (line && *line && s_whitelistCount < WHITELIST_MAX_ITEMS) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        trimString(line);
        if (line[0] != '\0') {
            safe_strncpy(s_whitelist[s_whitelistCount], line, MAX_PATH_LENGTH);
            s_whitelistCount++;
        }
        line = nl ? nl + 1 : NULL;
    }
}

void whitelistSave() {
    sceIoMkdir("ux0:data/PSV_Cleaner", 0777);
    SceUID fd = sceIoOpen(WHITELIST_FILE_PATH, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (fd < 0) return;

    for (int i = 0; i < s_whitelistCount; i++) {
        sceIoWrite(fd, s_whitelist[i], strlen(s_whitelist[i]));
        sceIoWrite(fd, "\n", 1);
    }
    sceIoClose(fd);
}

int whitelistAdd(const char *entry) {
    if (!s_whitelistLoaded) whitelistLoad();
    if (!entry || entry[0] == '\0') return 0;

    char clean[MAX_PATH_LENGTH];
    safe_strncpy(clean, entry, sizeof(clean));
    trimString(clean);
    if (clean[0] == '\0') return 0;

    size_t len = strlen(clean);
    if (len > 4 && clean[len - 1] == '/' && clean[len - 2] != ':') {
        clean[len - 1] = '\0';
    }

    for (int i = 0; i < s_whitelistCount; i++) {
        if (strcasecmp(s_whitelist[i], clean) == 0) return 0;
    }

    if (s_whitelistCount >= WHITELIST_MAX_ITEMS) return 0;
    safe_strncpy(s_whitelist[s_whitelistCount], clean, MAX_PATH_LENGTH);
    s_whitelistCount++;
    whitelistSave();
    invalidateSpaceCache();
    return 1;
}

int whitelistRemove(int index) {
    if (!s_whitelistLoaded) whitelistLoad();
    if (index < 0 || index >= s_whitelistCount) return 0;

    for (int i = index; i < s_whitelistCount - 1; i++) {
        safe_strncpy(s_whitelist[i], s_whitelist[i + 1], MAX_PATH_LENGTH);
    }
    s_whitelistCount--;
    whitelistSave();
    invalidateSpaceCache();
    return 1;
}

void whitelistClear() {
    s_whitelistCount = 0;
    s_whitelistLoaded = 1;
    whitelistSave();
    invalidateSpaceCache();
}

int whitelistCheck(const char *path, const char *titleId) {
    if (!s_whitelistLoaded) whitelistLoad();
    if (s_whitelistCount == 0) return 0;

    for (int i = 0; i < s_whitelistCount; i++) {
        const char *w = s_whitelist[i];
        size_t wLen = strlen(w);
        if (wLen == 0) continue;

        if (titleId && titleId[0] && strcasecmp(w, titleId) == 0) return 1;

        if (path && path[0]) {
            if (strcasecmp(path, w) == 0) return 1;

            if (strncasecmp(path, w, wLen) == 0) {
                if (path[wLen] == '\0' || path[wLen] == '/' || (wLen > 0 && w[wLen - 1] == '/')) {
                    return 1;
                }
            }

            if (safetyIsValidTitleId(w)) {
                char seg[24];
                safe_snprintf(seg, sizeof(seg), "/%s", w);
                const char *found = strstr(path, seg);
                if (!found) {
                    safe_snprintf(seg, sizeof(seg), ":%s", w);
                    found = strstr(path, seg);
                }
                if (found) {
                    char next = found[strlen(seg)];
                    if (next == '\0' || next == '/') return 1;
                }
            }
        }
    }

    return 0;
}

int whitelistGetCount() {
    if (!s_whitelistLoaded) whitelistLoad();
    return s_whitelistCount;
}

const char *whitelistGetItem(int index) {
    if (!s_whitelistLoaded) whitelistLoad();
    if (index < 0 || index >= s_whitelistCount) return "";
    return s_whitelist[index];
}

typedef struct {
    int version;
    unsigned int totalCleanups;
    unsigned long long totalFilesDeleted;
    unsigned long long totalBytesFreed;
} PersistentStats;

static void saveLifetimeStats() {
    PersistentStats ps;
    ps.version = STATS_VERSION;
    ps.totalCleanups = s_lifetimeStats.totalCleanups;
    ps.totalFilesDeleted = s_lifetimeStats.totalFilesDeleted;
    ps.totalBytesFreed = s_lifetimeStats.totalBytesFreed;

    sceIoMkdir("ux0:data/PSV_Cleaner", 0777);
    SceUID fd = sceIoOpen(STATS_FILE_PATH, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (fd >= 0) {
        sceIoWrite(fd, &ps, sizeof(ps));
        sceIoClose(fd);
    }
}

static void loadLifetimeStats() {
    memset(&s_lifetimeStats, 0, sizeof(s_lifetimeStats));
    SceUID fd = sceIoOpen(STATS_FILE_PATH, SCE_O_RDONLY, 0777);
    if (fd < 0) return;

    PersistentStats ps;
    int rd = sceIoRead(fd, &ps, sizeof(ps));
    sceIoClose(fd);
    if (rd == sizeof(ps) && ps.version == STATS_VERSION) {
        s_lifetimeStats.totalCleanups = ps.totalCleanups;
        s_lifetimeStats.totalFilesDeleted = ps.totalFilesDeleted;
        s_lifetimeStats.totalBytesFreed = ps.totalBytesFreed;
    }
}

void historyLoad() {
    s_historyCount = 0;
    s_historyLoaded = 1;
    loadLifetimeStats();

    SceUID fd = sceIoOpen(HISTORY_FILE_PATH, SCE_O_RDONLY, 0777);
    if (fd < 0) return;

    char *buf = (char *)malloc(16384);
    if (!buf) {
        sceIoClose(fd);
        return;
    }

    int rd = sceIoRead(fd, buf, 16383);
    sceIoClose(fd);
    if (rd <= 0) {
        free(buf);
        return;
    }
    buf[rd] = '\0';

    char *line = buf;
    while (line && *line && s_historyCount < HISTORY_MAX_ENTRIES) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        trimString(line);

        if (line[0] != '\0') {
            HistoryEntry entry;
            memset(&entry, 0, sizeof(entry));
            unsigned long long bytes = 0;
            int del = 0, fail = 0, inter = 0;
            char *p1 = strchr(line, '|');
            if (p1) {
                *p1 = '\0';
                char *p2 = strchr(p1 + 1, '|');
                if (p2) {
                    *p2 = '\0';
                    safe_strncpy(entry.timestamp, line, sizeof(entry.timestamp));
                    safe_strncpy(entry.mode, p1 + 1, sizeof(entry.mode));
                    if (sscanf(p2 + 1, "%d|%d|%d|%llu", &del, &fail, &inter, &bytes) == 4) {
                        entry.deleted = del;
                        entry.failed = fail;
                        entry.interrupted = inter;
                        entry.bytesFreed = bytes;
                        s_history[s_historyCount++] = entry;
                    }
                }
            }
        }
        line = nl ? nl + 1 : NULL;
    }

    free(buf);
}

static void historySaveAll() {
    sceIoMkdir("ux0:data/PSV_Cleaner", 0777);
    SceUID fd = sceIoOpen(HISTORY_FILE_PATH, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (fd < 0) return;

    char line[160];
    for (int i = 0; i < s_historyCount; i++) {
        int n = snprintf(line, sizeof(line), "%s|%s|%d|%d|%d|%llu\n",
                         s_history[i].timestamp,
                         s_history[i].mode,
                         s_history[i].deleted,
                         s_history[i].failed,
                         s_history[i].interrupted,
                         s_history[i].bytesFreed);
        if (n > 0) sceIoWrite(fd, line, (SceSize)n);
    }
    sceIoClose(fd);
}

void historyAppend(const char *mode, int deleted, int failed, int interrupted, unsigned long long bytesFreed) {
    if (!s_historyLoaded) historyLoad();

    SceDateTime dt;
    memset(&dt, 0, sizeof(dt));
    sceRtcGetCurrentClockLocalTime(&dt);

    HistoryEntry entry;
    memset(&entry, 0, sizeof(entry));
    snprintf(entry.timestamp, sizeof(entry.timestamp), "%04u-%02u-%02u %02u:%02u:%02u",
             (unsigned int)(dt.year % 10000U), (unsigned int)(dt.month % 100U), (unsigned int)(dt.day % 100U),
             (unsigned int)(dt.hour % 100U), (unsigned int)(dt.minute % 100U), (unsigned int)(dt.second % 100U));
    safe_strncpy(entry.mode, mode ? mode : "Clean", sizeof(entry.mode));
    entry.deleted = deleted;
    entry.failed = failed;
    entry.interrupted = interrupted;
    entry.bytesFreed = bytesFreed;

    int moveCount = s_historyCount < HISTORY_MAX_ENTRIES ? s_historyCount : HISTORY_MAX_ENTRIES - 1;
    for (int i = moveCount; i > 0; i--) {
        s_history[i] = s_history[i - 1];
    }
    s_history[0] = entry;
    if (s_historyCount < HISTORY_MAX_ENTRIES) s_historyCount++;

    s_lifetimeStats.totalCleanups++;
    if (deleted > 0) s_lifetimeStats.totalFilesDeleted += (unsigned long long)deleted;
    s_lifetimeStats.totalBytesFreed += bytesFreed;

    historySaveAll();
    saveLifetimeStats();
}

int historyGetCount() {
    if (!s_historyLoaded) historyLoad();
    return s_historyCount;
}

const HistoryEntry *historyGetEntry(int index) {
    if (!s_historyLoaded) historyLoad();
    if (index < 0 || index >= s_historyCount) return NULL;
    return &s_history[index];
}

void historyGetLifetimeStats(LifetimeStats *outStats) {
    if (!s_historyLoaded) historyLoad();
    if (outStats) *outStats = s_lifetimeStats;
}

void historyClear() {
    s_historyCount = 0;
    s_historyLoaded = 1;
    sceIoRemove(HISTORY_FILE_PATH);
}

typedef struct {
    char *relName;
    unsigned int crc;
    unsigned int compSize;
    unsigned int uncompSize;
    unsigned int localOffset;
    unsigned int dosDateTime;
    unsigned short method;
} ZipCdRecord;

typedef struct {
    SceUID fd;
    unsigned int currentOffset;
    ZipCdRecord *records;
    int count;
    int capacity;
    unsigned long long totalUncomp;
    unsigned long long totalComp;
    int error;
} ZipWriter;

static int zipAddRecord(ZipWriter *zw, const char *relName, unsigned int crc, unsigned int compSize,
                        unsigned int uncompSize, unsigned int localOffset, unsigned int dosDT, unsigned short method) {
    if (zw->count >= zw->capacity) {
        int newCap = zw->capacity ? zw->capacity * 2 : 32;
        ZipCdRecord *nr = (ZipCdRecord *)realloc(zw->records, sizeof(ZipCdRecord) * newCap);
        if (!nr) return 0;
        zw->records = nr;
        zw->capacity = newCap;
    }

    ZipCdRecord *r = &zw->records[zw->count];
    r->relName = strdup(relName);
    if (!r->relName) return 0;
    r->crc = crc;
    r->compSize = compSize;
    r->uncompSize = uncompSize;
    r->localOffset = localOffset;
    r->dosDateTime = dosDT;
    r->method = method;
    zw->count++;
    return 1;
}

static void zipWriteFileEntry(ZipWriter *zw, const char *srcPath, const char *relPath, const SceDateTime *mtime) {
    if (zw->error || isEmergencyStopRequested()) return;

    SceUID inFd = sceIoOpen(srcPath, SCE_O_RDONLY, 0777);
    if (inFd < 0) {
        zw->error = 1;
        return;
    }

    unsigned int dosDT = 0;
    if (mtime) sceRtcGetDosTime(mtime, &dosDT);
    unsigned short dosTime = (unsigned short)(dosDT & 0xFFFF);
    unsigned short dosDate = (unsigned short)((dosDT >> 16) & 0xFFFF);

    size_t nameLen = strlen(relPath);
    if (nameLen > 0xFFFF) nameLen = 0xFFFF;

    unsigned int localOff = zw->currentOffset;
    unsigned char lfh[30];
    memset(lfh, 0, sizeof(lfh));
    writeLE32(lfh, 0x04034b50U);
    writeLE16(lfh + 4, 20);
    writeLE16(lfh + 6, 0x0800);
    writeLE16(lfh + 8, Z_DEFLATED);
    writeLE16(lfh + 10, dosTime);
    writeLE16(lfh + 12, dosDate);
    writeLE16(lfh + 26, (unsigned short)nameLen);

    if (sceIoWrite(zw->fd, lfh, sizeof(lfh)) != sizeof(lfh) ||
        sceIoWrite(zw->fd, relPath, (SceSize)nameLen) != (int)nameLen) {
        sceIoClose(inFd);
        zw->error = 1;
        return;
    }
    zw->currentOffset += (unsigned int)(sizeof(lfh) + nameLen);

    z_stream strm;
    memset(&strm, 0, sizeof(strm));
    if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        sceIoClose(inFd);
        zw->error = 1;
        return;
    }

    unsigned char *inBuf = (unsigned char *)malloc(16384);
    unsigned char *outBuf = (unsigned char *)malloc(16384);
    if (!inBuf || !outBuf) {
        if (inBuf) free(inBuf);
        if (outBuf) free(outBuf);
        deflateEnd(&strm);
        sceIoClose(inFd);
        zw->error = 1;
        return;
    }

    uLong crc = crc32(0L, Z_NULL, 0);
    unsigned int uncompTotal = 0;
    unsigned int compTotal = 0;
    int flush = Z_NO_FLUSH;

    do {
        int rd = sceIoRead(inFd, inBuf, 16384);
        if (rd < 0) {
            zw->error = 1;
            break;
        }
        if (rd > 0) {
            crc = crc32(crc, inBuf, (uInt)rd);
            uncompTotal += (unsigned int)rd;
            flush = Z_NO_FLUSH;
        } else {
            flush = Z_FINISH;
        }

        strm.next_in = inBuf;
        strm.avail_in = (uInt)rd;

        do {
            strm.next_out = outBuf;
            strm.avail_out = 16384;
            int zret = deflate(&strm, flush);
            if (zret == Z_STREAM_ERROR) {
                zw->error = 1;
                break;
            }
            unsigned int have = 16384 - strm.avail_out;
            if (have > 0) {
                if (sceIoWrite(zw->fd, outBuf, have) != (int)have) {
                    zw->error = 1;
                    break;
                }
                compTotal += have;
                zw->currentOffset += have;
            }
        } while (strm.avail_out == 0 && !zw->error);
    } while (flush != Z_FINISH && !zw->error);

    free(inBuf);
    free(outBuf);
    deflateEnd(&strm);
    sceIoClose(inFd);

    if (zw->error) return;

    unsigned char sizes[12];
    writeLE32(sizes, (unsigned int)crc);
    writeLE32(sizes + 4, compTotal);
    writeLE32(sizes + 8, uncompTotal);

    if (sceIoLseek(zw->fd, (SceOff)(localOff + 14), SCE_SEEK_SET) < 0 ||
        sceIoWrite(zw->fd, sizes, sizeof(sizes)) != sizeof(sizes) ||
        sceIoLseek(zw->fd, (SceOff)zw->currentOffset, SCE_SEEK_SET) < 0) {
        zw->error = 1;
        return;
    }

    if (!zipAddRecord(zw, relPath, (unsigned int)crc, compTotal, uncompTotal, localOff, dosDT, Z_DEFLATED)) {
        zw->error = 1;
        return;
    }

    zw->totalUncomp += uncompTotal;
    zw->totalComp += compTotal;
}

static void zipWalkDir(ZipWriter *zw, const char *rootDir, const char *relPrefix) {
    if (zw->error || isEmergencyStopRequested()) return;

    SceUID dfd = sceIoDopen(rootDir);
    if (dfd < 0) return;

    SceIoDirent dir;
    memset(&dir, 0, sizeof(dir));

    while (sceIoDread(dfd, &dir) > 0) {
        if (zw->error || isEmergencyStopRequested()) break;
        if (strcmp(dir.d_name, ".") == 0 || strcmp(dir.d_name, "..") == 0) continue;

        char fullPath[MAX_PATH_LENGTH];
        safe_snprintf(fullPath, sizeof(fullPath), "%s%s%s", rootDir,
                      (rootDir[strlen(rootDir) - 1] == '/') ? "" : "/", dir.d_name);

        char relPath[MAX_PATH_LENGTH];
        if (relPrefix && relPrefix[0]) {
            safe_snprintf(relPath, sizeof(relPath), "%s/%s", relPrefix, dir.d_name);
        } else {
            safe_strncpy(relPath, dir.d_name, sizeof(relPath));
        }

        if (SCE_S_ISDIR(dir.d_stat.st_mode)) {
            zipWalkDir(zw, fullPath, relPath);
        } else {
            zipWriteFileEntry(zw, fullPath, relPath, &dir.d_stat.st_mtime);
        }
    }

    sceIoDclose(dfd);
}

int backupDirToZip(const char *srcDir, const char *zipPath, unsigned long long *outUncompressed, unsigned long long *outCompressed) {
    if (!srcDir || !zipPath) return 0;

    ZipWriter zw;
    memset(&zw, 0, sizeof(zw));

    zw.fd = sceIoOpen(zipPath, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (zw.fd < 0) return 0;

    zipWalkDir(&zw, srcDir, "");

    unsigned int cdStart = zw.currentOffset;
    unsigned int cdSize = 0;

    if (!zw.error && zw.count > 0) {
        for (int i = 0; i < zw.count; i++) {
            ZipCdRecord *r = &zw.records[i];
            size_t nameLen = strlen(r->relName);
            unsigned char cdh[46];
            memset(cdh, 0, sizeof(cdh));
            writeLE32(cdh, 0x02014b50U);
            writeLE16(cdh + 4, 20);
            writeLE16(cdh + 6, 20);
            writeLE16(cdh + 8, 0x0800);
            writeLE16(cdh + 10, r->method);
            writeLE16(cdh + 12, (unsigned short)(r->dosDateTime & 0xFFFF));
            writeLE16(cdh + 14, (unsigned short)((r->dosDateTime >> 16) & 0xFFFF));
            writeLE32(cdh + 16, r->crc);
            writeLE32(cdh + 20, r->compSize);
            writeLE32(cdh + 24, r->uncompSize);
            writeLE16(cdh + 28, (unsigned short)nameLen);
            writeLE32(cdh + 42, r->localOffset);

            if (sceIoWrite(zw.fd, cdh, sizeof(cdh)) != sizeof(cdh) ||
                sceIoWrite(zw.fd, r->relName, (SceSize)nameLen) != (int)nameLen) {
                zw.error = 1;
                break;
            }
            cdSize += (unsigned int)(sizeof(cdh) + nameLen);
        }

        if (!zw.error) {
            unsigned char eocd[22];
            memset(eocd, 0, sizeof(eocd));
            writeLE32(eocd, 0x06054b50U);
            writeLE16(eocd + 8, (unsigned short)zw.count);
            writeLE16(eocd + 10, (unsigned short)zw.count);
            writeLE32(eocd + 12, cdSize);
            writeLE32(eocd + 16, cdStart);
            if (sceIoWrite(zw.fd, eocd, sizeof(eocd)) != sizeof(eocd)) {
                zw.error = 1;
            }
        }
    } else if (zw.count == 0) {
        zw.error = 1;
    }

    sceIoClose(zw.fd);

    for (int i = 0; i < zw.count; i++) {
        free(zw.records[i].relName);
    }
    if (zw.records) free(zw.records);

    if (zw.error) {
        sceIoRemove(zipPath);
        return 0;
    }

    if (outUncompressed) *outUncompressed = zw.totalUncomp;
    if (outCompressed) *outCompressed = zw.totalComp;
    return 1;
}

static void deleteDirectoryUnchecked(const char *path) {
    SceUID dfd = sceIoDopen(path);
    if (dfd < 0) {
        sceIoRemove(path);
        return;
    }

    char **names = NULL;
    int *isDirs = NULL;
    int count = 0, cap = 0;
    SceIoDirent dir;
    memset(&dir, 0, sizeof(dir));

    while (sceIoDread(dfd, &dir) > 0) {
        if (strcmp(dir.d_name, ".") == 0 || strcmp(dir.d_name, "..") == 0) continue;
        if (count >= cap) {
            int nc = cap ? cap * 2 : 32;
            char **nn = (char **)realloc(names, sizeof(char *) * nc);
            int *nd = (int *)realloc(isDirs, sizeof(int) * nc);
            if (!nn || !nd) {
                if (nn) names = nn;
                if (nd) isDirs = nd;
                break;
            }
            names = nn;
            isDirs = nd;
            cap = nc;
        }
        names[count] = strdup(dir.d_name);
        isDirs[count] = SCE_S_ISDIR(dir.d_stat.st_mode);
        if (names[count]) count++;
    }
    sceIoDclose(dfd);

    for (int i = 0; i < count; i++) {
        char child[MAX_PATH_LENGTH];
        safe_snprintf(child, sizeof(child), "%s%s%s", path, (path[strlen(path) - 1] == '/') ? "" : "/", names[i]);
        if (isDirs[i]) {
            deleteDirectoryUnchecked(child);
        } else {
            if (sceIoRemove(child) >= 0) incrementDeletedFilesCount();
        }
        free(names[i]);
    }
    if (names) free(names);
    if (isDirs) free(isDirs);
    sceIoRmdir(path);
}

int backupOrphanSaveZip(const char *titleId, const char *saveDir, int deleteAfterBackup, char *outZipPath, size_t outZipSize) {
    if (!titleId || !saveDir || !safetyIsValidTitleId(titleId)) return 0;

    char baseDir[128];
    SceIoStat st;
    if (sceIoGetstat("uma0:", &st) >= 0) {
        sceIoMkdir("uma0:PSV_Cleaner_Backups", 0777);
        if (sceIoGetstat("uma0:PSV_Cleaner_Backups", &st) >= 0) {
            safe_strncpy(baseDir, "uma0:PSV_Cleaner_Backups", sizeof(baseDir));
        } else {
            sceIoMkdir("ux0:data/PSV_Cleaner", 0777);
            sceIoMkdir("ux0:data/PSV_Cleaner/saves_backup", 0777);
            safe_strncpy(baseDir, "ux0:data/PSV_Cleaner/saves_backup", sizeof(baseDir));
        }
    } else {
        sceIoMkdir("ux0:data/PSV_Cleaner", 0777);
        sceIoMkdir("ux0:data/PSV_Cleaner/saves_backup", 0777);
        safe_strncpy(baseDir, "ux0:data/PSV_Cleaner/saves_backup", sizeof(baseDir));
    }

    SceDateTime dt;
    memset(&dt, 0, sizeof(dt));
    sceRtcGetCurrentClockLocalTime(&dt);

    char zipPath[MAX_PATH_LENGTH];
    safe_snprintf(zipPath, sizeof(zipPath), "%s/%s_%04d%02d%02d_%02d%02d%02d.zip",
                  baseDir, titleId, dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);

    unsigned long long uncomp = 0, comp = 0;
    if (!backupDirToZip(saveDir, zipPath, &uncomp, &comp)) {
        return 0;
    }

    if (sceIoGetstat(zipPath, &st) < 0 || st.st_size <= 22) {
        return 0;
    }

    if (outZipPath && outZipSize > 0) {
        safe_strncpy(outZipPath, zipPath, outZipSize);
    }

    if (deleteAfterBackup) {
        if (strncasecmp(saveDir, "ux0:user/00/savedata/", 21) == 0 && strlen(saveDir) == 30) {
            deleteDirectoryUnchecked(saveDir);
            historyAppend("Save Backup+Del", getDeletedFilesCount(), 0, 0, uncomp);
        }
    } else {
        historyAppend("Save ZIP Backup", 0, 0, 0, 0);
    }

    return 1;
}

static int taiPathEndsWith(const char *s, const char *ext) {
    size_t ls = strlen(s);
    size_t le = strlen(ext);
    if (ls <= le) return 0;
    return strcasecmp(s + ls - le, ext) == 0;
}

static void taiAddEntry(const char *path, const char *section, TaiStatus status, unsigned long long size) {
    if (g_taiAudit.entryCount >= TAI_MAX_ITEMS) return;
    TaiAuditEntry *e = &g_taiAudit.entries[g_taiAudit.entryCount++];
    safe_strncpy(e->path, path, sizeof(e->path));
    safe_strncpy(e->section, section, sizeof(e->section));
    e->status = status;
    e->size = size;
    e->userSelected = 0;
}

static int taiIsReferenced(const char *fullPath, const char refList[][MAX_PATH_LENGTH], int refCount) {
    for (int i = 0; i < refCount; i++) {
        if (strcasecmp(fullPath, refList[i]) == 0) return 1;
    }
    return 0;
}

static void taiParseConfig(const char *cfgPath, char refList[][MAX_PATH_LENGTH], int *refCount, int maxRefs) {
    SceUID fd = sceIoOpen(cfgPath, SCE_O_RDONLY, 0777);
    if (fd < 0) return;

    char *buf = (char *)malloc(16384);
    if (!buf) {
        sceIoClose(fd);
        return;
    }

    int rd = sceIoRead(fd, buf, 16383);
    sceIoClose(fd);
    if (rd <= 0) {
        free(buf);
        return;
    }
    buf[rd] = '\0';

    char currentSection[32] = "*KERNEL";
    char *line = buf;

    while (line && *line) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        trimString(line);

        if (line[0] != '\0' && line[0] != '#') {
            if (line[0] == '*') {
                safe_strncpy(currentSection, line, sizeof(currentSection));
            } else if (strchr(line, ':') != NULL) {
                char *sp = strpbrk(line, " \t");
                if (sp) *sp = '\0';

                int isDup = taiIsReferenced(line, refList, *refCount);
                SceIoStat st;
                int exists = (sceIoGetstat(line, &st) >= 0);

                if (isDup) {
                    taiAddEntry(line, currentSection, TAI_DUPLICATE, exists ? (unsigned long long)st.st_size : 0);
                    g_taiAudit.duplicateCount++;
                } else if (!exists) {
                    taiAddEntry(line, currentSection, TAI_BROKEN, 0);
                    g_taiAudit.brokenCount++;
                } else {
                    taiAddEntry(line, currentSection, TAI_OK, (unsigned long long)st.st_size);
                }

                if (!isDup && *refCount < maxRefs) {
                    safe_strncpy(refList[*refCount], line, MAX_PATH_LENGTH);
                    (*refCount)++;
                }
            }
        }

        line = nl ? nl + 1 : NULL;
    }

    free(buf);
}

static void taiScanDirForUnused(const char *dirPath, const char refList[][MAX_PATH_LENGTH], int refCount) {
    SceUID dfd = sceIoDopen(dirPath);
    if (dfd < 0) return;

    SceIoDirent dir;
    memset(&dir, 0, sizeof(dir));

    while (sceIoDread(dfd, &dir) > 0) {
        if (strcmp(dir.d_name, ".") == 0 || strcmp(dir.d_name, "..") == 0) continue;
        if (SCE_S_ISDIR(dir.d_stat.st_mode)) continue;
        if (strcasecmp(dir.d_name, "config.txt") == 0) continue;

        char fullPath[MAX_PATH_LENGTH];
        safe_snprintf(fullPath, sizeof(fullPath), "%s%s%s", dirPath,
                      (dirPath[strlen(dirPath) - 1] == '/') ? "" : "/", dir.d_name);

        if (taiPathEndsWith(dir.d_name, ".bak") || taiPathEndsWith(dir.d_name, ".old") || taiPathEndsWith(dir.d_name, ".tmp")) {
            taiAddEntry(fullPath, "BACKUP", TAI_BACKUP, (unsigned long long)dir.d_stat.st_size);
            g_taiAudit.unusedCount++;
        } else if (taiPathEndsWith(dir.d_name, ".skprx") || taiPathEndsWith(dir.d_name, ".suprx")) {
            if (!taiIsReferenced(fullPath, refList, refCount)) {
                taiAddEntry(fullPath, "DISK", TAI_UNUSED, (unsigned long long)dir.d_stat.st_size);
                g_taiAudit.unusedCount++;
            }
        }
    }

    sceIoDclose(dfd);
}

void taiAuditRun() {
    memset(&g_taiAudit, 0, sizeof(g_taiAudit));
    SceIoStat st;

    g_taiAudit.hasUx0Tai = (sceIoGetstat("ux0:tai", &st) >= 0) ? 1 : 0;
    g_taiAudit.hasUr0Tai = (sceIoGetstat("ur0:tai", &st) >= 0) ? 1 : 0;
    g_taiAudit.hasUx0Config = (sceIoGetstat("ux0:tai/config.txt", &st) >= 0) ? 1 : 0;
    g_taiAudit.hasUr0Config = (sceIoGetstat("ur0:tai/config.txt", &st) >= 0) ? 1 : 0;
    g_taiAudit.conflictDetected = (g_taiAudit.hasUx0Tai && g_taiAudit.hasUr0Tai) ? 1 : 0;

    char (*refList)[MAX_PATH_LENGTH] = (char (*)[MAX_PATH_LENGTH])malloc(sizeof(char[MAX_PATH_LENGTH]) * 96);
    if (!refList) return;
    int refCount = 0;

    if (g_taiAudit.hasUx0Config) {
        safe_strncpy(g_taiAudit.activeConfig, "ux0:tai/config.txt", sizeof(g_taiAudit.activeConfig));
        taiParseConfig("ux0:tai/config.txt", refList, &refCount, 96);
    } else if (g_taiAudit.hasUr0Config) {
        safe_strncpy(g_taiAudit.activeConfig, "ur0:tai/config.txt", sizeof(g_taiAudit.activeConfig));
        taiParseConfig("ur0:tai/config.txt", refList, &refCount, 96);
    } else {
        safe_strncpy(g_taiAudit.activeConfig, "None", sizeof(g_taiAudit.activeConfig));
    }

    if (g_taiAudit.hasUr0Tai) taiScanDirForUnused("ur0:tai/", refList, refCount);
    if (g_taiAudit.hasUx0Tai) taiScanDirForUnused("ux0:tai/", refList, refCount);

    free(refList);
}

int taiBackupConfig(char *outBackupPath, size_t outSize) {
    const char *src = NULL;
    const char *dst = NULL;
    SceIoStat st;

    if (sceIoGetstat("ux0:tai/config.txt", &st) >= 0) {
        src = "ux0:tai/config.txt";
        dst = "ux0:tai/config.txt.bak";
    } else if (sceIoGetstat("ur0:tai/config.txt", &st) >= 0) {
        src = "ur0:tai/config.txt";
        dst = "ur0:tai/config.txt.bak";
    } else {
        return 0;
    }

    SceUID inFd = sceIoOpen(src, SCE_O_RDONLY, 0777);
    if (inFd < 0) return 0;
    SceUID outFd = sceIoOpen(dst, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (outFd < 0) {
        sceIoClose(inFd);
        return 0;
    }

    char buf[4096];
    int rd;
    int ok = 1;
    while ((rd = sceIoRead(inFd, buf, sizeof(buf))) > 0) {
        if (sceIoWrite(outFd, buf, (SceSize)rd) != rd) {
            ok = 0;
            break;
        }
    }
    sceIoClose(inFd);
    sceIoClose(outFd);

    if (ok && outBackupPath && outSize > 0) {
        safe_strncpy(outBackupPath, dst, outSize);
    }
    return ok;
}

int taiDeleteUnusedSelected() {
    int deleted = 0;
    unsigned long long freed = 0;

    for (int i = 0; i < g_taiAudit.entryCount; i++) {
        TaiAuditEntry *e = &g_taiAudit.entries[i];
        if (!e->userSelected) continue;
        if (e->status != TAI_UNUSED && e->status != TAI_BACKUP) continue;
        if (whitelistCheck(e->path, NULL)) continue;
        if (strstr(e->path, "config.txt") != NULL) continue;

        if (sceIoRemove(e->path) >= 0) {
            deleted++;
            freed += e->size;
            incrementDeletedFilesCount();
        }
    }

    if (deleted > 0) {
        historyAppend("TaiHEN Cleanup", deleted, 0, 0, freed);
        taiAuditRun();
    }
    return deleted;
}

static int largeExtKind(const char *name, LargeFileKind *outKind) {
    const char *dot = strrchr(name, '.');
    if (!dot) return 0;
    if (strcasecmp(dot, ".vpk") == 0) { *outKind = LARGE_KIND_VPK; return 1; }
    if (strcasecmp(dot, ".pkg") == 0) { *outKind = LARGE_KIND_PKG; return 1; }
    if (strcasecmp(dot, ".zip") == 0 || strcasecmp(dot, ".7z") == 0 || strcasecmp(dot, ".rar") == 0 || strcasecmp(dot, ".cso") == 0) {
        *outKind = LARGE_KIND_ARCHIVE;
        return 1;
    }
    return 0;
}

static int largeShouldSkipDir(const char *path) {
    if (safetyPathIsProtected(path)) {
        if (strncasecmp(path, "ux0:data", 8) == 0 ||
            strncasecmp(path, "uma0:data", 9) == 0 ||
            strncasecmp(path, "imc0:data", 9) == 0 ||
            strncasecmp(path, "ur0:data", 8) == 0) {
            if (strlen(path) <= 9) return 0;
        }
        return 1;
    }
    if (whitelistCheck(path, NULL)) return 1;
    return 0;
}

static int largeCompareDesc(const void *a, const void *b) {
    const LargeFileItem *la = (const LargeFileItem *)a;
    const LargeFileItem *lb = (const LargeFileItem *)b;
    if (la->size > lb->size) return -1;
    if (la->size < lb->size) return 1;
    return 0;
}

static void largeInsertItem(const char *path, unsigned long long size, LargeFileKind kind) {
    if (g_largeItemCount < LARGE_MAX_ITEMS) {
        LargeFileItem *it = &g_largeItems[g_largeItemCount++];
        safe_strncpy(it->path, path, sizeof(it->path));
        it->size = size;
        it->kind = kind;
        it->selected = 0;
        return;
    }

    g_largeTruncated = 1;
    int smallestIdx = 0;
    for (int i = 1; i < g_largeItemCount; i++) {
        if (g_largeItems[i].size < g_largeItems[smallestIdx].size) smallestIdx = i;
    }
    if (size > g_largeItems[smallestIdx].size) {
        LargeFileItem *it = &g_largeItems[smallestIdx];
        safe_strncpy(it->path, path, sizeof(it->path));
        it->size = size;
        it->kind = kind;
        it->selected = 0;
    }
}

void largeFilesScanRun(int minSizeMB) {
    g_largeItemCount = 0;
    g_largeTruncated = 0;
    unsigned long long minBytes = (unsigned long long)(minSizeMB > 0 ? minSizeMB : 50) * 1024ULL * 1024ULL;

    const char *roots[] = {"ux0:", "uma0:", "imc0:", "ur0:data/"};
    char (*stack)[MAX_PATH_LENGTH] = (char (*)[MAX_PATH_LENGTH])malloc(sizeof(char[MAX_PATH_LENGTH]) * LARGE_SCAN_SLOTS);
    if (!stack) return;

    initScanProgress(4);

    for (size_t r = 0; r < sizeof(roots) / sizeof(roots[0]); r++) {
        if (isEmergencyStopRequested()) break;
        SceIoStat rootStat;
        if (sceIoGetstat(roots[r], &rootStat) < 0) {
            updateScanProgress((int)(r + 1));
            continue;
        }

        int top = 0;
        safe_strncpy(stack[top++], roots[r], MAX_PATH_LENGTH);
        unsigned int yields = 0;

        while (top > 0) {
            if (isEmergencyStopRequested()) break;

            char cur[MAX_PATH_LENGTH];
            safe_strncpy(cur, stack[--top], MAX_PATH_LENGTH);

            SceUID dfd = sceIoDopen(cur);
            if (dfd < 0) continue;

            SceIoDirent dir;
            memset(&dir, 0, sizeof(dir));

            while (sceIoDread(dfd, &dir) > 0) {
                if (isEmergencyStopRequested()) break;
                if (strcmp(dir.d_name, ".") == 0 || strcmp(dir.d_name, "..") == 0) continue;

                char child[MAX_PATH_LENGTH];
                size_t curLen = strlen(cur);
                const char *sep = (curLen > 0 && (cur[curLen - 1] == '/' || cur[curLen - 1] == ':')) ? "" : "/";
                safe_snprintf(child, sizeof(child), "%s%s%s", cur, sep, dir.d_name);

                if (SCE_S_ISDIR(dir.d_stat.st_mode)) {
                    if (!largeShouldSkipDir(child) && top < LARGE_SCAN_SLOTS) {
                        safe_strncpy(stack[top++], child, MAX_PATH_LENGTH);
                    }
                } else {
                    if (whitelistCheck(child, NULL)) continue;
                    unsigned long long sz = (unsigned long long)dir.d_stat.st_size;
                    LargeFileKind kind = LARGE_KIND_SIZE;
                    int isArch = largeExtKind(dir.d_name, &kind);
                    if (sz >= minBytes || isArch) {
                        largeInsertItem(child, sz, isArch ? kind : LARGE_KIND_SIZE);
                    }
                }
            }

            sceIoDclose(dfd);
            yields++;
            if ((yields % 128) == 0) sceKernelDelayThread(1000);
        }

        updateScanProgress((int)(r + 1));
    }

    free(stack);

    if (g_largeItemCount > 1) {
        qsort(g_largeItems, (size_t)g_largeItemCount, sizeof(LargeFileItem), largeCompareDesc);
    }
}

void largeFilesDeleteSelected() {
    CleanReport report;
    memset(&report, 0, sizeof(report));
    SceRtcTick start, end;
    sceRtcGetCurrentTick(&start);

    int selCount = 0;
    for (int i = 0; i < g_largeItemCount; i++) {
        if (g_largeItems[i].selected) selCount++;
    }
    initScanProgress(selCount > 0 ? selCount : 1);

    int done = 0;
    for (int i = 0; i < g_largeItemCount; i++) {
        if (isEmergencyStopRequested()) break;
        if (!g_largeItems[i].selected) continue;

        report.filesScanned++;
        SafetyItem item;
        safetyItemInit(&item, g_largeItems[i].path, g_largeItems[i].size, SAFETY_ORPHAN_NONE);
        item.user_selected = 1;
        safetyRecompute(&item);

        char reason[SAFETY_MAX_REASON];
        if (!safetyVerifyDelete(&item, reason, sizeof(reason))) {
            report.filesSkipped++;
        } else if (sceIoRemove(g_largeItems[i].path) >= 0) {
            report.filesDeleted++;
            report.bytesRecovered += g_largeItems[i].size;
            incrementDeletedFilesCount();
            g_largeItems[i].selected = 0;
        } else {
            report.filesFailed++;
        }

        done++;
        updateScanProgress(done);
    }

    sceRtcGetCurrentTick(&end);
    unsigned int res = sceRtcGetTickResolution();
    if (res == 0) res = 1000000;
    report.elapsedSeconds = (unsigned int)((end.tick - start.tick) / res);
    g_cleanReport = report;

    historyAppend("Large File Clean", (int)report.filesDeleted, (int)report.filesFailed, isEmergencyStopRequested(), report.bytesRecovered);
    invalidateSpaceCache();
}

static int analyzerDirCompareDesc(const void *a, const void *b) {
    const AnalyzerDirEntry *ea = (const AnalyzerDirEntry *)a;
    const AnalyzerDirEntry *eb = (const AnalyzerDirEntry *)b;
    if (ea->size > eb->size) return -1;
    if (ea->size < eb->size) return 1;
    return strcasecmp(ea->name, eb->name);
}

void analyzerBrowseDir(const char *dirPath) {
    g_analyzerDirEntryCount = 0;
    g_analyzerDirTruncated = 0;
    if (!dirPath || dirPath[0] == '\0') return;

    safe_strncpy(g_analyzerBrowsePath, dirPath, sizeof(g_analyzerBrowsePath));

    SceUID dfd = sceIoDopen(dirPath);
    if (dfd < 0) return;

    SceIoDirent dir;
    memset(&dir, 0, sizeof(dir));
    size_t baseLen = strlen(dirPath);
    const char *sep = (baseLen > 0 && (dirPath[baseLen - 1] == '/' || dirPath[baseLen - 1] == ':')) ? "" : "/";

    initScanProgress(1);

    while (sceIoDread(dfd, &dir) > 0) {
        if (isEmergencyStopRequested()) break;
        if (strcmp(dir.d_name, ".") == 0 || strcmp(dir.d_name, "..") == 0) continue;

        if (g_analyzerDirEntryCount >= ANALYZER_DIR_ENTRIES) {
            g_analyzerDirTruncated = 1;
            break;
        }

        AnalyzerDirEntry *e = &g_analyzerDirEntries[g_analyzerDirEntryCount];
        memset(e, 0, sizeof(*e));
        safe_strncpy(e->name, dir.d_name, sizeof(e->name));
        safe_snprintf(e->fullPath, sizeof(e->fullPath), "%s%s%s", dirPath, sep, dir.d_name);
        e->isDir = SCE_S_ISDIR(dir.d_stat.st_mode);

        if (e->isDir) {
            e->size = calculateTempSizeRecursive(e->fullPath);
        } else {
            e->size = (unsigned long long)dir.d_stat.st_size;
        }

        e->deletable = (!safetyPathIsProtected(e->fullPath) && !whitelistCheck(e->fullPath, NULL)) ? 1 : 0;
        g_analyzerDirEntryCount++;
    }

    sceIoDclose(dfd);

    if (g_analyzerDirEntryCount > 1) {
        qsort(g_analyzerDirEntries, (size_t)g_analyzerDirEntryCount, sizeof(AnalyzerDirEntry), analyzerDirCompareDesc);
    }
    updateScanProgress(1);
}

int analyzerDeleteEntry(const char *path) {
    if (!path || safetyPathIsProtected(path) || whitelistCheck(path, NULL)) return 0;

    SafetyItem item;
    safetyItemInit(&item, path, 0, SAFETY_ORPHAN_NONE);
    item.user_selected = 1;
    safetyRecompute(&item);

    char reason[SAFETY_MAX_REASON];
    safetySetUserConfirmed(1);
    int ok = safetyVerifyDelete(&item, reason, sizeof(reason));
    if (!ok) {
        safetySetUserConfirmed(0);
        return 0;
    }

    unsigned long long sizeBefore = calculateTempSizeRecursive(path);
    resetDeletedFilesCount();
    deleteRecursive(path);
    int delCount = getDeletedFilesCount();
    safetySetUserConfirmed(0);

    if (delCount > 0) {
        historyAppend("Analyzer Delete", delCount, 0, 0, sizeBefore);
        invalidateSpaceCache();
        return 1;
    }
    return 0;
}
