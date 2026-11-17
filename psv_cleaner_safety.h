#ifndef PSV_CLEANER_SAFETY_H
#define PSV_CLEANER_SAFETY_H

#include <stddef.h>

#define SAFETY_MAX_PATH 1024
#define SAFETY_MAX_REASON 96
#define SAFETY_MAX_TITLE 12
#define SAFETY_MAX_APPNAME 64
#define SAFETY_GROUP_COUNT 6

#define SAFETY_ORPHAN_NONE 0
#define SAFETY_ORPHAN_CERTAIN 1
#define SAFETY_ORPHAN_UNCERTAIN 2

#define SAFETY_CERTAINTY_NONE 0
#define SAFETY_CERTAINTY_PROBABLE 1
#define SAFETY_CERTAINTY_CERTAIN 2

typedef enum {
    CAT_SAFE = 0,
    CAT_REGENERABLE = 1,
    CAT_REVIEW = 2,
    CAT_PROTECTED = 3,
    CAT_ORPHANED = 4
} ItemCategory;

typedef enum {
    GRP_TEMPORARY = 0,
    GRP_CACHE = 1,
    GRP_CRASH = 2,
    GRP_ORPHANED = 3,
    GRP_REVIEW = 4,
    GRP_PROTECTED = 5
} ItemGroup;

typedef struct {
    char path[SAFETY_MAX_PATH];
    unsigned long long size;
    ItemCategory category;
    ItemGroup group;
    char reason[SAFETY_MAX_REASON];
    char titleId[SAFETY_MAX_TITLE];
    char appName[SAFETY_MAX_APPNAME];
    unsigned char deletion_allowed;
    unsigned char certainty;
    unsigned char user_selected;
    unsigned char curated;
} SafetyItem;

typedef struct {
    unsigned long long files[SAFETY_GROUP_COUNT];
    unsigned long long bytes[SAFETY_GROUP_COUNT];
    int enabled[SAFETY_GROUP_COUNT];
} SmartCleanPlan;

typedef struct {
    unsigned long long filesScanned;
    unsigned long long filesSafe;
    unsigned long long filesRegenerable;
    unsigned long long filesOrphaned;
    unsigned long long filesReview;
    unsigned long long filesProtected;
    unsigned long long filesSkipped;
    unsigned long long bytesSafe;
    unsigned long long bytesRegenerable;
    unsigned long long bytesOrphaned;
    unsigned long long bytesReview;
    unsigned long long bytesProtected;
} CleanScanStats;

typedef struct {
    unsigned long long filesScanned;
    unsigned long long filesDeleted;
    unsigned long long filesSkipped;
    unsigned long long filesFailed;
    unsigned long long bytesRecovered;
    unsigned int elapsedSeconds;
} CleanReport;

typedef int (*SafetyWhitelistFn)(const char *path, const char *titleId);

void safetyInit(const char *const *tempPaths, size_t count);
void safetySetWhitelistCallback(SafetyWhitelistFn fn);
int safetyMatchWildcard(const char *pattern, const char *text);
int safetyIsValidTitleId(const char *titleId);
int safetyIsDumpFileName(const char *name);
int safetyIsVolumeRoot(const char *path);
int safetyPathIsValid(const char *path);
int safetyPathIsCurated(const char *path);
int safetyPathIsProtected(const char *path);
void safetyItemInit(SafetyItem *item, const char *path, unsigned long long size, int orphanHint);
void safetyItemInitWithTitle(SafetyItem *item, const char *path, unsigned long long size, int orphanHint, const char *titleId);
void safetyRecompute(SafetyItem *item);
void safetySetItemReason(SafetyItem *item, const char *reason);
int safetyVerifyDelete(const SafetyItem *item, char *reasonOut, size_t reasonSize);
void safetySetUserConfirmed(int confirmed);
int safetyUserConfirmed(void);
const char *safetyCategoryName(ItemCategory category);
const char *safetyGroupLabel(ItemGroup group);
void safetyPlanReset(SmartCleanPlan *plan);
void safetyPlanResetDefaults(SmartCleanPlan *plan);
void safetyPlanAdd(SmartCleanPlan *plan, const SafetyItem *item);
int safetyGroupDeletable(ItemGroup group, const SmartCleanPlan *plan);
unsigned long long safetyPlanPotentialBytes(const SmartCleanPlan *plan);
unsigned long long safetyPlanPotentialFiles(const SmartCleanPlan *plan);
void safetyStatsAdd(CleanScanStats *stats, const SafetyItem *item);

#endif
