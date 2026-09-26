#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include "psv_cleaner_safety.h"

static const char *const *g_curatedPaths = NULL;
static size_t g_curatedPathCount = 0;
static int g_userConfirmed = 0;

static const struct {
    const char *prefix;
    const char *reason;
} SAFETY_PROTECTED_PREFIXES[] = {
    {"ux0:app/", "system application data"},
    {"ux0:appmeta/", "system application metadata"},
    {"ux0:patch/", "game patch data"},
    {"ux0:license/", "license data"},
    {"ux0:system/", "system data"},
    {"ux0:shell/", "shell data"},
    {"ux0:user/", "user data"},
    {"ux0:kd/", "system data"},
    {"ux0:mms/", "system data"},
    {"ux0:resource/", "system data"},
    {"ux0:saverom/", "system data"},
    {"ux0:registry/", "system registry"},
    {"ux0:config/", "system configuration"},
    {"ux0:tai/", "kernel plugin configuration"},
    {"ux0:ac/", "system data"},
    {"ux0:bd/", "system data"},
    {"ur0:config/", "system configuration"},
    {"ur0:shell/", "shell data"},
    {"ur0:user/", "user data"},
    {"ur0:tai/", "kernel plugin configuration"},
    {"ur0:license/", "license data"},
    {"ur0:appmeta/", "system application metadata"},
    {"ur0:data/", "system data"},
    {"vs0:app/", "system application data"},
    {"vs0:data/", "system data"},
    {"vs0:sys/", "system data"}
};

static const char *SAFETY_PROTECTED_EXACT[] = {
    "ux0:app",
    "ux0:appmeta",
    "ux0:patch",
    "ux0:license",
    "ux0:data",
    "ux0:picture",
    "ux0:video",
    "ux0:music",
    "ux0:photo",
    "ux0:user",
    "ux0:system",
    "ux0:temp",
    "ux0:cache",
    "ux0:log",
    "ur0:data",
    "ur0:user",
    "ur0:temp",
    "uma0:data"
};

static int safetyCharUpper(int c) {
    if (c >= 'a' && c <= 'z') return c - 'a' + 'A';
    return c;
}

static void safetyCopy(char *dst, size_t size, const char *src) {
    if (!dst || size == 0) return;
    dst[0] = '\0';
    if (!src) return;
    snprintf(dst, size, "%s", src);
}

static const char *safetyBaseName(const char *path) {
    const char *slash = strrchr(path, '/');
    const char *colon = strrchr(path, ':');
    const char *base = path;
    if (slash) base = slash + 1;
    if (colon && colon + 1 > base) base = colon + 1;
    return base;
}

static int safetySamePath(const char *a, const char *b) {
    size_t la = strlen(a);
    size_t lb = strlen(b);
    if (la > 0 && a[la - 1] == '/') la--;
    if (lb > 0 && b[lb - 1] == '/') lb--;
    if (la != lb) return 0;
    return strncasecmp(a, b, la) == 0;
}

static int safetyHasComponent(const char *path, const char *name) {
    size_t nameLen = strlen(name);
    const char *p = path;

    while (*p) {
        while (*p == '/' || *p == ':') p++;
        const char *start = p;
        while (*p && *p != '/' && *p != ':') p++;

        size_t len = (size_t)(p - start);
        if (len == nameLen && len > 0 && strncasecmp(start, name, len) == 0) return 1;
    }

    return 0;
}

static int safetyHasExtension(const char *path, const char *ext) {
    const char *base = safetyBaseName(path);
    size_t baseLen = strlen(base);
    size_t extLen = strlen(ext);

    if (baseLen <= extLen) return 0;
    if (base[baseLen - extLen - 1] != '.') return 0;
    return strncasecmp(base + baseLen - extLen, ext, extLen) == 0;
}

static const char *safetyProtectedReason(const char *path) {
    for (size_t i = 0; i < sizeof(SAFETY_PROTECTED_PREFIXES) / sizeof(SAFETY_PROTECTED_PREFIXES[0]); i++) {
        size_t len = strlen(SAFETY_PROTECTED_PREFIXES[i].prefix);
        if (strncasecmp(path, SAFETY_PROTECTED_PREFIXES[i].prefix, len) == 0) {
            return SAFETY_PROTECTED_PREFIXES[i].reason;
        }
    }
    return NULL;
}
void safetyInit(const char *const *tempPaths, size_t count) {
    g_curatedPaths = tempPaths;
    g_curatedPathCount = count;
}

void safetySetUserConfirmed(int confirmed) {
    g_userConfirmed = confirmed ? 1 : 0;
}

int safetyUserConfirmed(void) {
    return g_userConfirmed;
}

int safetyPathIsValid(const char *path) {
    if (!path) return 0;

    size_t len = strlen(path);
    if (len == 0 || len >= SAFETY_MAX_PATH) return 0;
    if (strstr(path, "..") != NULL) return 0;
    if (strstr(path, "//") != NULL) return 0;

    const char *colon = strchr(path, ':');
    if (!colon || colon == path) return 0;
    if ((size_t)(colon - path) > 8) return 0;
    if (strrchr(path, ':') != colon) return 0;

    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)path[i];
        if (c < 0x20 || c == 0x7f) return 0;
        if (c == '<' || c == '>' || c == '"' || c == '|' || c == '?' || c == '*') return 0;
    }

    return 1;
}

int safetyIsVolumeRoot(const char *path) {
    if (!path) return 0;
    size_t len = strlen(path);
    if (len < 4) return 0;

    const char *colon = strchr(path, ':');
    if (!colon) return 0;
    size_t prefixLen = (size_t)(colon - path);
    if (prefixLen < 3 || prefixLen > 4) return 0;

    for (size_t i = 0; i < prefixLen; i++) {
        int c = safetyCharUpper((unsigned char)path[i]);
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) return 0;
    }

    if (len == prefixLen + 1) return 1;
    if (len == prefixLen + 2 && path[prefixLen + 1] == '/') return 1;
    return 0;
}

int safetyPathIsProtected(const char *path) {
    if (!safetyPathIsValid(path)) return 1;
    if (safetyIsVolumeRoot(path)) return 1;

    for (size_t i = 0; i < sizeof(SAFETY_PROTECTED_EXACT) / sizeof(SAFETY_PROTECTED_EXACT[0]); i++) {
        if (safetySamePath(path, SAFETY_PROTECTED_EXACT[i])) return 1;
    }

    if (safetyProtectedReason(path) != NULL) return 1;
    return 0;
}

int safetyPathIsCurated(const char *path) {
    if (!path) return 0;

    for (size_t i = 0; i < g_curatedPathCount; i++) {
        const char *entry = g_curatedPaths ? g_curatedPaths[i] : NULL;
        if (!entry || entry[0] == '\0') continue;
        if (strchr(entry, '*') != NULL) continue;

        size_t entryLen = strlen(entry);
        if (strncmp(path, entry, entryLen) == 0) return 1;

        if (entry[entryLen - 1] == '/') {
            if (strlen(path) == entryLen - 1 && strncmp(path, entry, entryLen - 1) == 0) return 1;
        }
    }

    return 0;
}

int safetyIsValidTitleId(const char *titleId) {
    if (!titleId) return 0;

    size_t len = strlen(titleId);
    if (len != 9) return 0;

    int hasLetter = 0;
    for (size_t i = 0; i < len; i++) {
        int c = (unsigned char)titleId[i];
        if (c >= 'a' && c <= 'z') return 0;
        if (c >= 'A' && c <= 'Z') {
            hasLetter = 1;
            continue;
        }
        if (c >= '0' && c <= '9') continue;
        return 0;
    }

    return hasLetter;
}

int safetyIsDumpFileName(const char *name) {
    if (!name || name[0] == '\0') return 0;

    if (strncasecmp(name, "psp2core", 8) == 0 || strncasecmp(name, "psp2dmp", 7) == 0) return 1;
    if (safetyHasExtension(name, "psp2core")) return 1;
    if (safetyHasExtension(name, "psp2dmp")) return 1;

    size_t len = strlen(name);
    if (len > 12 && strncasecmp(name + len - 12, ".psp2dmp.tmp", 12) == 0) return 1;

    return 0;
}

static int safetyIsCacheLike(const char *path) {
    if (safetyHasComponent(path, "cache")) return 1;
    if (safetyHasComponent(path, "caches")) return 1;
    if (safetyHasComponent(path, "thumbnails")) return 1;
    if (safetyHasExtension(path, "cache")) return 1;
    return 0;
}

static int safetyIsCrashLike(const char *path) {
    if (safetyHasComponent(path, "crash")) return 1;
    if (safetyHasComponent(path, "dumps")) return 1;
    if (safetyHasComponent(path, "crash_dumps")) return 1;
    return 0;
}

static int safetyIsTempLike(const char *path) {
    if (safetyHasComponent(path, "temp")) return 1;
    if (safetyHasComponent(path, "tmp")) return 1;
    if (safetyHasComponent(path, "logs")) return 1;
    if (safetyHasComponent(path, "log")) return 1;
    if (safetyHasComponent(path, "trash")) return 1;
    if (safetyHasComponent(path, "recent")) return 1;
    if (safetyHasComponent(path, "shaderlog")) return 1;
    if (safetyHasComponent(path, "patch_temp")) return 1;
    if (safetyHasComponent(path, "update_temp")) return 1;

    if (safetyHasExtension(path, "tmp")) return 1;
    if (safetyHasExtension(path, "temp")) return 1;
    if (safetyHasExtension(path, "log")) return 1;
    if (safetyHasExtension(path, "bak")) return 1;
    if (safetyHasExtension(path, "old")) return 1;
    if (safetyHasExtension(path, "swp")) return 1;

    return 0;
}

static int safetyIsInstallerPackage(const char *path) {
    if (safetyHasExtension(path, "vpk")) return 1;
    if (safetyHasExtension(path, "pkg")) return 1;
    return 0;
}

static void safetySet(SafetyItem *item, ItemCategory category, ItemGroup group, unsigned char certainty, const char *reason) {
    item->category = category;
    item->group = group;
    item->certainty = certainty;
    safetyCopy(item->reason, sizeof(item->reason), reason);
}

const char *safetyCategoryName(ItemCategory category) {
    switch (category) {
        case CAT_SAFE: return "SAFE";
        case CAT_REGENERABLE: return "REGENERABLE";
        case CAT_REVIEW: return "REVIEW";
        case CAT_PROTECTED: return "PROTECTED";
        case CAT_ORPHANED: return "ORPHANED";
    }
    return "UNKNOWN";
}

const char *safetyGroupLabel(ItemGroup group) {
    switch (group) {
        case GRP_TEMPORARY: return "Temporary";
        case GRP_CACHE: return "Cache";
        case GRP_CRASH: return "Crash dumps";
        case GRP_ORPHANED: return "Orphaned";
        case GRP_REVIEW: return "Review";
        case GRP_PROTECTED: return "Protected";
    }
    return "Unknown";
}

static void safetyClassify(SafetyItem *item, int orphanHint) {
    const char *path = item->path;

    if (!safetyPathIsValid(path)) {
        safetySet(item, CAT_PROTECTED, GRP_PROTECTED, SAFETY_CERTAINTY_NONE, "invalid path");
        return;
    }

    if (safetyIsVolumeRoot(path)) {
        safetySet(item, CAT_PROTECTED, GRP_PROTECTED, SAFETY_CERTAINTY_NONE, "volume root");
        return;
    }

    if (safetyIsDumpFileName(safetyBaseName(path))) {
        safetySet(item, CAT_SAFE, GRP_CRASH, SAFETY_CERTAINTY_PROBABLE, "crash dump file");
        return;
    }

    item->curated = (unsigned char)(safetyPathIsCurated(path) ? 1 : 0);

    if (orphanHint != SAFETY_ORPHAN_NONE) {
        if (!safetyIsValidTitleId(item->titleId)) {
            if (orphanHint == SAFETY_ORPHAN_CERTAIN) {
                safetySet(item, CAT_ORPHANED, GRP_ORPHANED, SAFETY_CERTAINTY_CERTAIN, "orphaned application data");
            } else {
                safetySet(item, CAT_REVIEW, GRP_REVIEW, SAFETY_CERTAINTY_PROBABLE, "possible orphan data");
            }
            return;
        }

        if (orphanHint == SAFETY_ORPHAN_CERTAIN) {
            safetySet(item, CAT_ORPHANED, GRP_ORPHANED, SAFETY_CERTAINTY_CERTAIN, "data of an uninstalled application");
        } else {
            safetySet(item, CAT_REVIEW, GRP_REVIEW, SAFETY_CERTAINTY_NONE, "possible orphan, link uncertain");
        }
        return;
    }

    if (safetyHasComponent(path, "SCREENSHOT")) {
        safetySet(item, CAT_REVIEW, GRP_REVIEW, SAFETY_CERTAINTY_PROBABLE, "user screenshots");
        return;
    }

    if (item->curated) {
        if (safetyIsCacheLike(path)) {
            safetySet(item, CAT_REGENERABLE, GRP_CACHE, SAFETY_CERTAINTY_PROBABLE, "regenerable cache");
        } else if (safetyIsCrashLike(path)) {
            safetySet(item, CAT_SAFE, GRP_CRASH, SAFETY_CERTAINTY_PROBABLE, "crash dump storage");
        } else {
            safetySet(item, CAT_SAFE, GRP_TEMPORARY, SAFETY_CERTAINTY_PROBABLE, "known temporary location");
        }
        return;
    }

    if (safetyPathIsProtected(path)) {
        const char *protection = safetyProtectedReason(path);
        if (protection != NULL) {
            char reason[SAFETY_MAX_REASON];
            snprintf(reason, sizeof(reason), "protected: %s", protection);
            safetySet(item, CAT_PROTECTED, GRP_PROTECTED, SAFETY_CERTAINTY_NONE, reason);
        } else {
            safetySet(item, CAT_PROTECTED, GRP_PROTECTED, SAFETY_CERTAINTY_NONE, "protected system path");
        }
        return;
    }

    if (safetyIsCacheLike(path)) {
        safetySet(item, CAT_REGENERABLE, GRP_CACHE, SAFETY_CERTAINTY_PROBABLE, "cache data");
        return;
    }

    if (safetyIsCrashLike(path)) {
        safetySet(item, CAT_SAFE, GRP_CRASH, SAFETY_CERTAINTY_PROBABLE, "crash dump storage");
        return;
    }

    if (safetyIsTempLike(path)) {
        safetySet(item, CAT_SAFE, GRP_TEMPORARY, SAFETY_CERTAINTY_PROBABLE, "temporary file");
        return;
    }

    if (safetyIsInstallerPackage(path)) {
        safetySet(item, CAT_REVIEW, GRP_REVIEW, SAFETY_CERTAINTY_PROBABLE, "installer package");
        return;
    }

    safetySet(item, CAT_REVIEW, GRP_REVIEW, SAFETY_CERTAINTY_PROBABLE, "not a known cleanup target");
}

void safetyItemInitWithTitle(SafetyItem *item, const char *path, unsigned long long size, int orphanHint, const char *titleId) {
    if (!item) return;

    memset(item, 0, sizeof(*item));
    safetyCopy(item->path, sizeof(item->path), path);
    if (titleId) safetyCopy(item->titleId, sizeof(item->titleId), titleId);
    item->size = size;
    item->certainty = SAFETY_CERTAINTY_PROBABLE;
    safetyClassify(item, orphanHint);
    safetyRecompute(item);
}

void safetyItemInit(SafetyItem *item, const char *path, unsigned long long size, int orphanHint) {
    safetyItemInitWithTitle(item, path, size, orphanHint, NULL);
}

void safetySetItemReason(SafetyItem *item, const char *reason) {
    if (!item) return;
    safetyCopy(item->reason, sizeof(item->reason), reason);
}

void safetyRecompute(SafetyItem *item) {
    int allowed = 0;

    if (!item) return;

    switch (item->category) {
        case CAT_SAFE:
        case CAT_REGENERABLE:
            allowed = 1;
            break;
        case CAT_ORPHANED:
            allowed = (item->certainty >= SAFETY_CERTAINTY_CERTAIN || item->user_selected);
            break;
        case CAT_REVIEW:
            allowed = ((item->user_selected || item->curated) && item->certainty >= SAFETY_CERTAINTY_PROBABLE);
            break;
        default:
            allowed = 0;
            break;
    }

    item->deletion_allowed = (unsigned char)allowed;
}

int safetyVerifyDelete(const SafetyItem *item, char *reasonOut, size_t reasonSize) {
    if (reasonOut && reasonSize > 0) reasonOut[0] = '\0';

    if (!item) {
        if (reasonOut) safetyCopy(reasonOut, reasonSize, "missing item");
        return 0;
    }

    if (!safetyPathIsValid(item->path)) {
        if (reasonOut) safetyCopy(reasonOut, reasonSize, "invalid path");
        return 0;
    }

    if (item->category == CAT_PROTECTED) {
        if (reasonOut) safetyCopy(reasonOut, reasonSize, "protected item");
        return 0;
    }

    if (!item->deletion_allowed) {
        if (reasonOut) safetyCopy(reasonOut, reasonSize, "classification does not allow deletion");
        return 0;
    }

    if (safetyPathIsProtected(item->path)) {
        int exempt = 0;
        if (safetyPathIsCurated(item->path)) exempt = 1;
        else if (safetyIsDumpFileName(safetyBaseName(item->path))) exempt = 1;
        else if (item->category == CAT_ORPHANED && item->certainty >= SAFETY_CERTAINTY_CERTAIN) exempt = 1;

        if (!exempt) {
            if (reasonOut) safetyCopy(reasonOut, reasonSize, "protected system path");
            return 0;
        }
    }

    if (!safetyUserConfirmed() && !item->user_selected) {
        if (reasonOut) safetyCopy(reasonOut, reasonSize, "user confirmation required");
        return 0;
    }

    return 1;
}

void safetyPlanReset(SmartCleanPlan *plan) {
    if (!plan) return;
    memset(plan, 0, sizeof(*plan));
}

void safetyPlanResetDefaults(SmartCleanPlan *plan) {
    if (!plan) return;

    memset(plan, 0, sizeof(*plan));
    plan->enabled[GRP_TEMPORARY] = 1;
    plan->enabled[GRP_CACHE] = 1;
    plan->enabled[GRP_CRASH] = 1;
    plan->enabled[GRP_ORPHANED] = 1;
    plan->enabled[GRP_REVIEW] = 0;
    plan->enabled[GRP_PROTECTED] = 0;
}

void safetyPlanAdd(SmartCleanPlan *plan, const SafetyItem *item) {
    if (!plan || !item) return;
    if (item->group < GRP_TEMPORARY || item->group > GRP_PROTECTED) return;

    plan->files[item->group]++;
    plan->bytes[item->group] += item->size;
}

int safetyGroupDeletable(ItemGroup group, const SmartCleanPlan *plan) {
    if (!plan) return 0;
    if (group < GRP_TEMPORARY || group > GRP_PROTECTED) return 0;
    if (group == GRP_REVIEW || group == GRP_PROTECTED) return 0;
    return plan->enabled[group] ? 1 : 0;
}

unsigned long long safetyPlanPotentialBytes(const SmartCleanPlan *plan) {
    unsigned long long total = 0;

    if (!plan) return 0;

    for (ItemGroup group = GRP_TEMPORARY; group <= GRP_PROTECTED; group++) {
        if (safetyGroupDeletable(group, plan)) total += plan->bytes[group];
    }

    return total;
}

unsigned long long safetyPlanPotentialFiles(const SmartCleanPlan *plan) {
    unsigned long long total = 0;

    if (!plan) return 0;

    for (ItemGroup group = GRP_TEMPORARY; group <= GRP_PROTECTED; group++) {
        if (safetyGroupDeletable(group, plan)) total += plan->files[group];
    }

    return total;
}

void safetyStatsAdd(CleanScanStats *stats, const SafetyItem *item) {
    if (!stats || !item) return;

    stats->filesScanned++;

    switch (item->category) {
        case CAT_SAFE:
            stats->filesSafe++;
            stats->bytesSafe += item->size;
            break;
        case CAT_REGENERABLE:
            stats->filesRegenerable++;
            stats->bytesRegenerable += item->size;
            break;
        case CAT_ORPHANED:
            stats->filesOrphaned++;
            stats->bytesOrphaned += item->size;
            break;
        case CAT_REVIEW:
            stats->filesReview++;
            stats->bytesReview += item->size;
            break;
        default:
            stats->filesProtected++;
            stats->bytesProtected += item->size;
            break;
    }
}

