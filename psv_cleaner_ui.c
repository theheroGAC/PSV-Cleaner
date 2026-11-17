#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <psp2/sysmodule.h>
#include <psp2/display.h>
#include <psp2/gxm.h>
#include <psp2/common_dialog.h>
#include <psp2/ime_dialog.h>
#include <vita2d.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include "psv_cleaner_core.h"
#include "psv_cleaner_lang.h"
#include "psv_cleaner_tools.h"

#define APP_VERSION "v1.18"
#define TOTAL_CATEGORIES 49
#define TOOLS_MENU_COUNT 8

static vita2d_pgf *g_draw_font = NULL;
static int g_animFrame = 0;
static ThemeColors g_theme;
static SceTouchPanelInfo g_touchPanel;

static char g_toastTitle[64] = "";
static char g_toastMsg[128] = "";
static int g_toastTimer = 0;
static int g_toastKind = 0;

static int isLatinChar(unsigned int c) {
    return c < 0x3000;
}

void drawProgressBar(vita2d_pgf *font, int percent);

void gpuMemoryCleanup() {
    vita2d_wait_rendering_done();
}

void showNotificationKind(const char *title, const char *message, int kind) {
    safe_strncpy(g_toastTitle, title ? title : "PSV Cleaner", sizeof(g_toastTitle));
    safe_strncpy(g_toastMsg, message ? message : "", sizeof(g_toastMsg));
    g_toastKind = kind;
    g_toastTimer = 210;
}

void showNotification(const char *title, const char *message) {
    showNotificationKind(title, message, 1);
}

void formatSize(unsigned long long size, char *out, size_t outSize) {
    if (size < 1024ULL)
        snprintf(out, outSize, "%llu B", size);
    else if (size < (1024ULL * 1024))
        snprintf(out, outSize, "%.1f KB", size / 1024.0);
    else if (size < (1024ULL * 1024 * 1024))
        snprintf(out, outSize, "%.2f MB", size / (1024.0 * 1024));
    else
        snprintf(out, outSize, "%.2f GB", size / (1024.0 * 1024 * 1024));
}

static int lerpChannel(int a, int b, int num, int den) {
    return a + ((b - a) * num) / den;
}

void drawBackground() {
    int r1 = g_theme.bg_primary & 0xFF;
    int g1 = (g_theme.bg_primary >> 8) & 0xFF;
    int b1 = (g_theme.bg_primary >> 16) & 0xFF;
    int r2 = g_theme.bg_secondary & 0xFF;
    int g2 = (g_theme.bg_secondary >> 8) & 0xFF;
    int b2 = (g_theme.bg_secondary >> 16) & 0xFF;

    for (int row = 0; row < 544; row += 8) {
        int r = lerpChannel(r1, r2, row, 544);
        int g = lerpChannel(g1, g2, row, 544);
        int b = lerpChannel(b1, b2, row, 544);
        vita2d_draw_rectangle(0, row, 960, 8, RGBA(r, g, b, 255));
    }
    vita2d_draw_fill_circle(110, 70, 170, g_theme.bg_glow1);
    vita2d_draw_fill_circle(880, 500, 200, g_theme.bg_glow2);
}

void drawCardBorder(int x, int y, int w, int h, int color) {
    vita2d_draw_rectangle(x, y, w, 1, color);
    vita2d_draw_rectangle(x, y + h - 1, w, 1, color);
    vita2d_draw_rectangle(x, y, 1, h, color);
    vita2d_draw_rectangle(x + w - 1, y, 1, h, color);
}

void drawCard(int x, int y, int w, int h) {
    vita2d_draw_rectangle(x, y, w, h, g_theme.card);
    drawCardBorder(x, y, w, h, g_theme.border);
}

void drawCheckBox(int x, int y, int size, int checked) {
    if (checked) {
        vita2d_draw_rectangle(x, y, size, size, g_theme.success);
        int ink = RGBA(6, 30, 20, 255);
        vita2d_draw_line(x + 3, y + size / 2, x + size / 2 - 1, y + size - 4, ink);
        vita2d_draw_line(x + 3, y + size / 2 + 1, x + size / 2 - 1, y + size - 3, ink);
        vita2d_draw_line(x + size / 2 - 1, y + size - 4, x + size - 3, y + 4, ink);
        vita2d_draw_line(x + size / 2 - 1, y + size - 3, x + size - 3, y + 5, ink);
    } else {
        vita2d_draw_rectangle(x, y, size, size, g_theme.card_dark);
        drawCardBorder(x, y, size, size, g_theme.border);
    }
}

void drawCenteredText(vita2d_pgf *font, int cx, int y, int color, float scale, const char *text) {
    int w = vita2d_pgf_text_width(font, scale, text);
    if (w <= 0) w = (int)(strlen(text) * 9 * scale);
    vita2d_pgf_draw_text(font, cx - w / 2, y, color, scale, text);
}

void drawHeader(vita2d_pgf *font, const char *title, const char *subtitle) {
    vita2d_draw_rectangle(40, 34, 6, 38, g_theme.accent);
    vita2d_pgf_draw_text(font, 58, 60, g_theme.text_primary, 1.55f, title);
    if (subtitle) {
        vita2d_pgf_draw_text(font, 58, 82, g_theme.text_secondary, 0.85f, subtitle);
    }
    vita2d_pgf_draw_text(font, 880, 56, g_theme.text_faint, 0.8f, APP_VERSION);
    vita2d_draw_rectangle(40, 94, 880, 1, g_theme.border);
}

void drawFooterBar() {
    vita2d_draw_rectangle(0, 496, 960, 48, g_theme.card_dark);
    vita2d_draw_rectangle(0, 496, 960, 1, g_theme.accent_dim);
}

void drawButtonHint(vita2d_pgf *font, int x, int y, const char *buttonText, const char *actionText) {
    int glyphColor = g_theme.text_secondary;
    int kind = 0;

    if (strcmp(buttonText, "X") == 0) {
        glyphColor = RGBA(96, 165, 250, 255);
        kind = 1;
    } else if (strcmp(buttonText, "O") == 0) {
        glyphColor = RGBA(248, 113, 113, 255);
        kind = 2;
    } else if (strcmp(buttonText, "T") == 0 || strcmp(buttonText, "△") == 0) {
        glyphColor = RGBA(74, 222, 128, 255);
        kind = 3;
    } else if (strcmp(buttonText, "S") == 0 || strcmp(buttonText, "□") == 0 || strcmp(buttonText, "■") == 0) {
        glyphColor = RGBA(244, 114, 182, 255);
        kind = 4;
    } else if (strcmp(buttonText, "↕") == 0 || strcmp(buttonText, "↔") == 0) {
        glyphColor = g_theme.text_secondary;
        kind = 5;
    } else if (strcmp(buttonText, "SEL") == 0 || strcmp(buttonText, "SELECT") == 0) {
        glyphColor = g_theme.text_secondary;
        kind = 6;
    }

    int textX = x + 30;

    if (kind >= 1 && kind <= 5) {
        int cx = x + 11;
        int cy = y - 7;

        vita2d_draw_fill_circle(cx, cy, 11, g_theme.border);
        vita2d_draw_fill_circle(cx, cy, 9, g_theme.card_dark);

        if (kind == 1) {
            for (int i = -1; i <= 1; i++) {
                vita2d_draw_line(cx - 5, cy - 5 + i, cx + 5, cy + 5 + i, glyphColor);
                vita2d_draw_line(cx - 5, cy + 5 + i, cx + 5, cy - 5 + i, glyphColor);
            }
        } else if (kind == 2) {
            vita2d_draw_fill_circle(cx, cy, 6, glyphColor);
            vita2d_draw_fill_circle(cx, cy, 3, g_theme.card_dark);
        } else if (kind == 3) {
            for (int i = 0; i <= 1; i++) {
                vita2d_draw_line(cx, cy - 5 + i, cx - 5, cy + 4 + i, glyphColor);
                vita2d_draw_line(cx, cy - 5 + i, cx + 5, cy + 4 + i, glyphColor);
                vita2d_draw_line(cx - 5, cy + 4 + i, cx + 5, cy + 4 + i, glyphColor);
            }
        } else if (kind == 4) {
            vita2d_draw_rectangle(cx - 5, cy - 5, 10, 2, glyphColor);
            vita2d_draw_rectangle(cx - 5, cy + 3, 10, 2, glyphColor);
            vita2d_draw_rectangle(cx - 5, cy - 5, 2, 10, glyphColor);
            vita2d_draw_rectangle(cx + 3, cy - 5, 2, 10, glyphColor);
        } else if (kind == 5) {
            vita2d_draw_line(cx, cy - 7, cx, cy + 7, glyphColor);
            vita2d_draw_line(cx - 3, cy - 4, cx, cy - 7, glyphColor);
            vita2d_draw_line(cx + 3, cy - 4, cx, cy - 7, glyphColor);
            vita2d_draw_line(cx - 3, cy + 4, cx, cy + 7, glyphColor);
            vita2d_draw_line(cx + 3, cy + 4, cx, cy + 7, glyphColor);
        }
    } else if (kind == 6) {
        vita2d_draw_rectangle(x, y - 16, 28, 18, g_theme.card_dark);
        drawCardBorder(x, y - 16, 28, 18, g_theme.border);
        vita2d_pgf_draw_text(font, x + 4, y - 2, glyphColor, 0.62f, "SEL");
        textX = x + 34;
    } else {
        char chip[12];
        safe_strncpy(chip, buttonText, sizeof(chip));
        int chipW = vita2d_pgf_text_width(font, 0.72f, chip) + 12;
        if (chipW < 26) chipW = 26;
        vita2d_draw_rectangle(x, y - 16, chipW, 18, g_theme.card_dark);
        drawCardBorder(x, y - 16, chipW, 18, g_theme.border);
        vita2d_pgf_draw_text(font, x + 6, y - 2, glyphColor, 0.72f, chip);
        textX = x + chipW + 8;
    }

    vita2d_pgf_draw_text(font, textX, y, g_theme.text_secondary, 0.82f, actionText);
}

void drawToastOverlay(vita2d_pgf *font) {
    if (g_toastTimer <= 0) return;
    g_toastTimer--;

    int accentCol = g_theme.accent;
    if (g_toastKind == 1) accentCol = g_theme.success;
    else if (g_toastKind == 2) accentCol = g_theme.warning;
    else if (g_toastKind == 3) accentCol = g_theme.danger;

    int x = 520, y = 14, w = 420, h = 58;
    vita2d_draw_rectangle(x, y, w, h, g_theme.card_solid);
    drawCardBorder(x, y, w, h, accentCol);
    vita2d_draw_rectangle(x, y, 5, h, accentCol);

    vita2d_pgf_draw_text(font, x + 16, y + 23, g_theme.text_primary, 0.85f, g_toastTitle);
    vita2d_pgf_draw_text(font, x + 16, y + 45, g_theme.text_secondary, 0.72f, g_toastMsg);
}

void drawSpinnerDots(int cx, int cy, int radius) {
    int step = (g_animFrame / 4) % 8;
    int r = g_theme.accent & 0xFF;
    int g = (g_theme.accent >> 8) & 0xFF;
    int b = (g_theme.accent >> 16) & 0xFF;
    for (int i = 0; i < 8; i++) {
        float angle = i * 0.785398f;
        int px = cx + (int)(radius * cosf(angle));
        int py = cy + (int)(radius * sinf(angle));
        int alpha = 55 + (((i - step + 8) % 8) * 25);
        if (alpha > 255) alpha = 255;
        vita2d_draw_fill_circle(px, py, 4, RGBA(r, g, b, alpha));
    }
}

void drawSpinner(vita2d_pgf *font, int cx, int cy, const char *label) {
    drawSpinnerDots(cx, cy, 22);
    drawCenteredText(font, cx, cy + 48, g_theme.text_secondary, 1.0f, label);
}

void drawProgressBar(vita2d_pgf *font, int percent) {
    drawBackground();

    drawCard(230, 150, 500, 250);
    vita2d_draw_rectangle(231, 151, 498, 3, g_theme.accent);

    drawCenteredText(font, 480, 195, g_theme.text_primary, 1.4f, TR(TXT_APP_NAME));

    if (isEmergencyStopRequested()) {
        drawCenteredText(font, 480, 230, g_theme.danger, 1.0f, TR(TXT_STOPPING_OP));
    } else {
        drawCenteredText(font, 480, 230, g_theme.accent, 1.0f, TR(TXT_CLEANING_PROGRESS));
    }

    int barX = 270, barY = 256, barW = 420, barH = 18;
    vita2d_draw_rectangle(barX, barY, barW, barH, g_theme.card_dark);
    drawCardBorder(barX, barY, barW, barH, g_theme.border);

    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    int filled = (barW * percent) / 100;
    if (filled > 0) {
        vita2d_draw_rectangle(barX, barY, filled, barH, g_theme.accent);
        vita2d_draw_rectangle(barX, barY, filled, 6, RGBA(255, 255, 255, 45));
        vita2d_draw_rectangle(barX + filled - 3, barY, 3, barH, RGBA(224, 242, 254, 255));
    }

    char text[32];
    snprintf(text, sizeof(text), "%d%%", percent);
    drawCenteredText(font, 480, 306, g_theme.text_primary, 1.3f, text);

    char filesText[64];
    snprintf(filesText, sizeof(filesText), "%s: %d", TR(TXT_FILES_DELETED), getDeletedFilesCount());
    drawCenteredText(font, 480, 336, g_theme.text_secondary, 0.85f, filesText);

    const char *stage;
    if (percent < 30) stage = TR(TXT_STAGE_SCANNING);
    else if (percent < 70) stage = TR(TXT_STAGE_DELETING);
    else stage = TR(TXT_STAGE_FINALIZING);

    drawSpinnerDots(280, 366, 9);
    vita2d_pgf_draw_text(font, 300, 372, g_theme.text_secondary, 0.9f, stage);

    drawFooterBar();
    if (isEmergencyStopRequested()) {
        drawCenteredText(font, 480, 526, g_theme.danger, 0.9f, TR(TXT_ESTOP_WAIT));
    } else {
        drawButtonHint(font, 350, 526, "O", TR(TXT_ESTOP_BTN));
    }
}

typedef struct {
    int selected;
    int total_options;
    int scrollOffset;
    int enabled[TOTAL_CATEGORIES];
} MenuOptions;

typedef struct {
    FileList *fileList;
    int *visibleIndices;
    int visibleCount;
    int visibleCapacity;
    int scrollOffset;
    int selectedRow;
    SortMode sortMode;
    char fileFilter[16];
    char searchQuery[64];
    unsigned long long totalVisibleSize;
} PreviewState;

typedef struct {
    AppList *appList;
    int *visibleIndices;
    int visibleCount;
    int visibleCapacity;
    int scrollOffset;
    int selectedRow;
    int showAppList;
    SortMode sortMode;
    char searchQuery[64];
} AppListState;

typedef enum {
    PROFILE_QUICK = 0,
    PROFILE_COMPLETE,
    PROFILE_SELECTIVE,
    PROFILE_SMART,
    PROFILE_ORPHAN,
    PROFILE_ANALYZER
} CleaningProfile;

typedef enum {
    RESULT_NONE = 0,
    RESULT_CLEAN_DONE = 1,
    RESULT_INTERRUPTED = 2,
    RESULT_APP_CLEANED = 3
} ResultKind;

static void truncateMiddleOrTail(const char *src, char *dst, size_t maxChars) {
    size_t len = strlen(src);
    if (len <= maxChars) {
        strcpy(dst, src);
        return;
    }
    if (maxChars <= 3) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, maxChars - 3);
    dst[maxChars - 3] = '.';
    dst[maxChars - 2] = '.';
    dst[maxChars - 1] = '.';
    dst[maxChars] = '\0';
}

static int isSpecialOptionIndex(int index) {
    return index == 7 || index == 47 || index == 48;
}

void syncMenuToGlobals(MenuOptions *menu) {
    cleanSystem = menu->enabled[0];
    cleanVitaShell = menu->enabled[1];
    cleanPkgi = menu->enabled[2];
    cleanRetroArch = menu->enabled[3];
    cleanAutoplugin = menu->enabled[4];
    cleanAdrenaline = menu->enabled[5];
    cleanCrashDumps = menu->enabled[6];

    excludePictureFolder = menu->enabled[8];
    excludeVpkFiles = menu->enabled[9];
    excludeVitaDBCache = menu->enabled[10];

    cleanOrphanedData = menu->enabled[11];
    cleanAllAppsTempFiles = menu->enabled[12];

    cleanEasyVpK = menu->enabled[13];
    cleanDaemon = menu->enabled[14];
    cleanVitaGrafix = menu->enabled[15];
    cleanOnemenu = menu->enabled[16];
    cleanPCSX = menu->enabled[17];
    cleanMGBA = menu->enabled[18];
    cleanFlycast = menu->enabled[19];
    cleanShellbat = menu->enabled[20];
    cleanSwitchUser = menu->enabled[21];

    cleanThemeCache = menu->enabled[22];
    cleanNotificationCache = menu->enabled[23];
    cleanActivityLog = menu->enabled[24];
    cleanOrphanedLicenseFiles = menu->enabled[25];
    cleanOrphanedDLC = menu->enabled[26];
    cleanBrowser = menu->enabled[27];
    cleanVHBB = menu->enabled[28];
    cleanITLS = menu->enabled[29];
    cleanDownloadEnabler = menu->enabled[30];
    cleanMoonlight = menu->enabled[31];
    cleanRetroFlow = menu->enabled[32];
    cleanOrphanedAddcont = menu->enabled[33];
    cleanEmptyLiveareaBubbles = menu->enabled[34];
    excludeVideoFolder = menu->enabled[35];

    cleanFontCache = menu->enabled[36];
    cleanPhotoMusicCache = menu->enabled[37];
    cleanSceShellCache = menu->enabled[38];
    cleanRegistryTemp = menu->enabled[39];
    cleanNetworkCache = menu->enabled[40];
    cleanLicenseCache = menu->enabled[41];
    cleanPSVitaDB = menu->enabled[42];
    cleanHenkaku = menu->enabled[43];
    cleanPSVshell = menu->enabled[44];
    cleanCheatTools = menu->enabled[45];
    cleanRePatchOrphans = menu->enabled[46];

    invalidateSpaceCache();
}

void syncGlobalsToMenu(MenuOptions *menu) {
    menu->enabled[0] = cleanSystem;
    menu->enabled[1] = cleanVitaShell;
    menu->enabled[2] = cleanPkgi;
    menu->enabled[3] = cleanRetroArch;
    menu->enabled[4] = cleanAutoplugin;
    menu->enabled[5] = cleanAdrenaline;
    menu->enabled[6] = cleanCrashDumps;
    menu->enabled[7] = 1;
    menu->enabled[8] = excludePictureFolder;
    menu->enabled[9] = excludeVpkFiles;
    menu->enabled[10] = excludeVitaDBCache;
    menu->enabled[11] = cleanOrphanedData;
    menu->enabled[12] = cleanAllAppsTempFiles;
    menu->enabled[13] = cleanEasyVpK;
    menu->enabled[14] = cleanDaemon;
    menu->enabled[15] = cleanVitaGrafix;
    menu->enabled[16] = cleanOnemenu;
    menu->enabled[17] = cleanPCSX;
    menu->enabled[18] = cleanMGBA;
    menu->enabled[19] = cleanFlycast;
    menu->enabled[20] = cleanShellbat;
    menu->enabled[21] = cleanSwitchUser;
    menu->enabled[22] = cleanThemeCache;
    menu->enabled[23] = cleanNotificationCache;
    menu->enabled[24] = cleanActivityLog;
    menu->enabled[25] = cleanOrphanedLicenseFiles;
    menu->enabled[26] = cleanOrphanedDLC;
    menu->enabled[27] = cleanBrowser;
    menu->enabled[28] = cleanVHBB;
    menu->enabled[29] = cleanITLS;
    menu->enabled[30] = cleanDownloadEnabler;
    menu->enabled[31] = cleanMoonlight;
    menu->enabled[32] = cleanRetroFlow;
    menu->enabled[33] = cleanOrphanedAddcont;
    menu->enabled[34] = cleanEmptyLiveareaBubbles;
    menu->enabled[35] = excludeVideoFolder;
    menu->enabled[36] = cleanFontCache;
    menu->enabled[37] = cleanPhotoMusicCache;
    menu->enabled[38] = cleanSceShellCache;
    menu->enabled[39] = cleanRegistryTemp;
    menu->enabled[40] = cleanNetworkCache;
    menu->enabled[41] = cleanLicenseCache;
    menu->enabled[42] = cleanPSVitaDB;
    menu->enabled[43] = cleanHenkaku;
    menu->enabled[44] = cleanPSVshell;
    menu->enabled[45] = cleanCheatTools;
    menu->enabled[46] = cleanRePatchOrphans;
    menu->enabled[47] = 1;
    menu->enabled[48] = 1;
}

void initMenuOptions(MenuOptions *menu, CleaningProfile profile) {
    menu->selected = 0;
    menu->scrollOffset = 0;
    menu->total_options = TOTAL_CATEGORIES;

    switch (profile) {
        case PROFILE_QUICK:
            for (int i = 0; i < TOTAL_CATEGORIES; i++) {
                menu->enabled[i] = 0;
            }
            menu->enabled[1] = 1;
            menu->enabled[2] = 1;
            menu->enabled[3] = 1;
            menu->enabled[4] = 1;
            menu->enabled[5] = 1;
            menu->enabled[8] = 1;
            menu->enabled[9] = 1;
            menu->enabled[10] = 1;
            menu->enabled[35] = 1;
            for (int i = 13; i <= 21; i++) menu->enabled[i] = 1;
            menu->enabled[42] = 1;
            menu->enabled[43] = 1;
            menu->enabled[44] = 1;
            menu->enabled[45] = 1;
            break;

        case PROFILE_COMPLETE:
            for (int i = 0; i < TOTAL_CATEGORIES; i++) {
                menu->enabled[i] = 1;
            }
            menu->enabled[9] = 0;
            menu->enabled[10] = 0;
            break;

        case PROFILE_SELECTIVE:
        default:
            syncGlobalsToMenu(menu);
            return;
    }

    syncMenuToGlobals(menu);
}

void drawScrollbar(int x, int y, int height, int visible, int total, int offset) {
    if (total <= visible) return;
    vita2d_draw_rectangle(x, y, 8, height, g_theme.card_dark);
    int thumbH = (visible * height) / total;
    if (thumbH < 12) thumbH = 12;
    int thumbY = y + (offset * (height - thumbH)) / (total - visible);
    vita2d_draw_rectangle(x, thumbY, 8, thumbH, g_theme.accent_dim);
}

static void rebuildPreviewView(PreviewState *preview) {
    preview->visibleCount = 0;
    preview->totalVisibleSize = 0;
    if (!preview->fileList || preview->fileList->count <= 0) return;

    if (preview->fileList->count > preview->visibleCapacity) {
        int *nv = (int *)realloc(preview->visibleIndices, sizeof(int) * preview->fileList->count);
        if (!nv) return;
        preview->visibleIndices = nv;
        preview->visibleCapacity = preview->fileList->count;
    }

    for (int i = 0; i < preview->fileList->count; i++) {
        const char *path = preview->fileList->files[i].path;
        const char *fn = strrchr(path, '/');
        fn = fn ? fn + 1 : path;
        if (matchesFileFilter(fn, preview->fileFilter) && matchesFileSearch(path, preview->searchQuery)) {
            preview->visibleIndices[preview->visibleCount++] = i;
            preview->totalVisibleSize += preview->fileList->files[i].size;
        }
    }

    if (preview->selectedRow >= preview->visibleCount) {
        preview->selectedRow = preview->visibleCount > 0 ? preview->visibleCount - 1 : 0;
    }
    if (preview->scrollOffset > preview->selectedRow) {
        preview->scrollOffset = preview->selectedRow;
    }
}

static void rebuildAppView(AppListState *appState) {
    appState->visibleCount = 0;
    if (!appState->appList || appState->appList->count <= 0) return;

    if (appState->appList->count > appState->visibleCapacity) {
        int *nv = (int *)realloc(appState->visibleIndices, sizeof(int) * appState->appList->count);
        if (!nv) return;
        appState->visibleIndices = nv;
        appState->visibleCapacity = appState->appList->count;
    }

    for (int i = 0; i < appState->appList->count; i++) {
        if (appState->searchQuery[0] == '\0' ||
            matchesFileSearch(appState->appList->apps[i].titleId, appState->searchQuery) ||
            matchesFileSearch(appState->appList->apps[i].appName, appState->searchQuery)) {
            appState->visibleIndices[appState->visibleCount++] = i;
        }
    }

    if (appState->selectedRow >= appState->visibleCount) {
        appState->selectedRow = appState->visibleCount > 0 ? appState->visibleCount - 1 : 0;
    }
    if (appState->scrollOffset > appState->selectedRow) {
        appState->scrollOffset = appState->selectedRow;
    }
}

static int countPreviewSelected(const PreviewState *preview) {
    if (!preview->fileList) return 0;
    int c = 0;
    for (int i = 0; i < preview->fileList->count; i++) {
        if (preview->fileList->files[i].selected) c++;
    }
    return c;
}

static unsigned long long sumPreviewSelectedSize(const PreviewState *preview) {
    if (!preview->fileList) return 0;
    unsigned long long s = 0;
    for (int i = 0; i < preview->fileList->count; i++) {
        if (preview->fileList->files[i].selected) s += preview->fileList->files[i].size;
    }
    return s;
}

static void utf8ToUtf16(const char *src, SceWChar16 *dst, size_t maxChars) {
    size_t i = 0;
    if (!src || maxChars == 0) return;
    while (*src && i + 1 < maxChars) {
        unsigned char c = (unsigned char)*src;
        if (c < 0x80) {
            dst[i++] = (SceWChar16)c;
            src++;
        } else if ((c & 0xE0) == 0xC0 && src[1]) {
            dst[i++] = (SceWChar16)(((c & 0x1F) << 6) | ((unsigned char)src[1] & 0x3F));
            src += 2;
        } else if ((c & 0xF0) == 0xE0 && src[1] && src[2]) {
            dst[i++] = (SceWChar16)(((c & 0x0F) << 12) | (((unsigned char)src[1] & 0x3F) << 6) | ((unsigned char)src[2] & 0x3F));
            src += 3;
        } else {
            src++;
        }
    }
    dst[i] = 0;
}

static void utf16ToUtf8(const SceWChar16 *src, char *dst, size_t maxBytes) {
    size_t o = 0;
    if (!src || maxBytes == 0) return;
    while (*src && o + 4 < maxBytes) {
        unsigned int c = (unsigned int)*src++;
        if (c < 0x80) {
            dst[o++] = (char)c;
        } else if (c < 0x800) {
            dst[o++] = (char)(0xC0 | (c >> 6));
            dst[o++] = (char)(0x80 | (c & 0x3F));
        } else {
            dst[o++] = (char)(0xE0 | (c >> 12));
            dst[o++] = (char)(0x80 | ((c >> 6) & 0x3F));
            dst[o++] = (char)(0x80 | (c & 0x3F));
        }
    }
    dst[o] = '\0';
}

static int openImeKeyboard(vita2d_pgf *font, const char *title, const char *initialText, char *outUtf8, size_t outSize) {
    SceWChar16 titleW[128];
    SceWChar16 initW[128];
    SceWChar16 inputW[128];
    memset(titleW, 0, sizeof(titleW));
    memset(initW, 0, sizeof(initW));
    memset(inputW, 0, sizeof(inputW));

    utf8ToUtf16(title ? title : "Search", titleW, 127);
    utf8ToUtf16(initialText ? initialText : "", initW, 127);

    SceImeDialogParam param;
    sceImeDialogParamInit(&param);
    param.supportedLanguages = 0;
    param.languagesForced = SCE_FALSE;
    param.type = SCE_IME_TYPE_BASIC_LATIN;
    param.option = SCE_IME_OPTION_NO_ASSISTANCE;
    param.dialogMode = SCE_IME_DIALOG_DIALOG_MODE_WITH_CANCEL;
    param.textBoxMode = SCE_IME_DIALOG_TEXTBOX_MODE_WITH_CLEAR;
    param.title = titleW;
    param.maxTextLength = (SceUInt32)(outSize < 127 ? outSize - 1 : 126);
    param.initialText = initW;
    param.inputTextBuffer = inputW;

    if (sceImeDialogInit(&param) < 0) {
        return 0;
    }

    int accepted = 0;
    while (1) {
        vita2d_start_drawing();
        vita2d_clear_screen();
        drawBackground();
        drawHeader(font, TR(TXT_APP_NAME), title);
        vita2d_end_drawing();
        vita2d_common_dialog_update();
        vita2d_swap_buffers();

        SceCommonDialogStatus status = sceImeDialogGetStatus();
        if (status == SCE_COMMON_DIALOG_STATUS_FINISHED) {
            SceImeDialogResult result;
            memset(&result, 0, sizeof(result));
            sceImeDialogGetResult(&result);
            if (result.button == SCE_IME_DIALOG_BUTTON_ENTER) {
                utf16ToUtf8(inputW, outUtf8, outSize);
                accepted = 1;
            }
            break;
        }
        sceKernelDelayThread(16 * 1000);
    }

    sceImeDialogTerm();
    return accepted;
}

void drawMainScreen(vita2d_pgf *font, const char *spaceText, int spaceKnown) {
    drawBackground();
    drawHeader(font, TR(TXT_APP_NAME), TR(TXT_APP_SUBTITLE));

    drawCard(200, 134, 560, 274);
    vita2d_draw_rectangle(201, 135, 558, 3, g_theme.accent);

    vita2d_pgf_draw_text(font, 240, 174, g_theme.text_secondary, 0.88f, TR(TXT_SYSTEM_STATUS));
    vita2d_pgf_draw_text(font, 240, 208, g_theme.success, 1.25f, TR(TXT_READY_CLEANUP));

    vita2d_draw_rectangle(240, 232, 480, 1, g_theme.border);

    vita2d_pgf_draw_text(font, 240, 264, g_theme.text_secondary, 0.88f, TR(TXT_SPACE_TO_FREE));
    if (spaceKnown) {
        vita2d_pgf_draw_text(font, 240, 310, g_theme.accent, 1.8f, spaceText);
    } else {
        drawSpinnerDots(260, 298, 13);
        vita2d_pgf_draw_text(font, 290, 306, g_theme.text_secondary, 1.0f, TR(TXT_SCANNING));
    }

    LifetimeStats stats;
    historyGetLifetimeStats(&stats);
    char totalFreedBuf[32];
    formatSize(stats.totalBytesFreed, totalFreedBuf, sizeof(totalFreedBuf));
    char statsLine[128];
    snprintf(statsLine, sizeof(statsLine), "Cleanups: %u  |  Total Freed: %s  |  Theme: %s",
             stats.totalCleanups, totalFreedBuf, getThemeName(currentTheme));
    vita2d_draw_rectangle(240, 340, 480, 1, g_theme.border);
    vita2d_pgf_draw_text(font, 240, 376, g_theme.text_faint, 0.78f, statsLine);

    drawFooterBar();
    drawButtonHint(font, 24, 526, "□", TR(TXT_BTN_MODES));
    drawButtonHint(font, 150, 526, "X", TR(TXT_BTN_PREVIEW_CLEAN));
    drawButtonHint(font, 345, 526, "△", TR(TXT_BTN_OPTIONS));
    drawButtonHint(font, 480, 526, "SEL", TR(TXT_BTN_APPS));
    drawButtonHint(font, 600, 526, "L", TR(TXT_BTN_TOOLS));
    drawButtonHint(font, 715, 526, "START", "Info");
    drawButtonHint(font, 855, 526, "O", TR(TXT_BTN_EXIT));
}

void drawProfileSelect(vita2d_pgf *font, CleaningProfile selected) {
    drawBackground();
    drawHeader(font, TR(TXT_SELECT_MODE), TR(TXT_SELECT_MODE_SUB));

    const char* names[] = {
        TR(TXT_MODE_QUICK),
        TR(TXT_MODE_COMPLETE),
        TR(TXT_MODE_SELECTIVE),
        TR(TXT_MODE_SMART),
        TR(TXT_MODE_ORPHAN),
        TR(TXT_MODE_ANALYZER)
    };
    const char* descs[] = {
        TR(TXT_DESC_QUICK),
        TR(TXT_DESC_COMPLETE),
        TR(TXT_DESC_SELECTIVE),
        TR(TXT_DESC_SMART),
        TR(TXT_DESC_ORPHAN),
        TR(TXT_DESC_ANALYZER)
    };
    const char* tags[] = {
        "FAST",
        "DEEP",
        "CUSTOM",
        "SMART",
        "ORPHAN",
        "STORAGE"
    };

    for (int i = 0; i < 6; i++) {
        int y = 110 + i * 62;
        int isSelected = (i == (int)selected);

        vita2d_draw_rectangle(110, y, 740, 56, isSelected ? g_theme.card_solid : g_theme.card);
        drawCardBorder(110, y, 740, 56, isSelected ? g_theme.accent : g_theme.border);

        if (isSelected) {
            vita2d_draw_rectangle(111, y + 1, 4, 54, g_theme.accent);
            vita2d_draw_rectangle(111, y + 1, 738, 54, g_theme.accent_soft);
        }

        int titleColor = isSelected ? g_theme.text_primary : g_theme.text_secondary;
        vita2d_pgf_draw_text(font, 135, y + 26, titleColor, 1.02f, names[i]);
        vita2d_pgf_draw_text(font, 135, y + 46, g_theme.text_faint, 0.76f, descs[i]);

        int tagColor = isSelected ? g_theme.accent : g_theme.text_faint;
        vita2d_pgf_draw_text(font, 760, y + 33, tagColor, 0.78f, tags[i]);
    }

    drawFooterBar();
    drawButtonHint(font, 180, 526, "↕", TR(TXT_BTN_NAVIGATE));
    drawButtonHint(font, 400, 526, "X", TR(TXT_BTN_CONFIRM));
    drawButtonHint(font, 620, 526, "O", TR(TXT_BTN_DASHBOARD));
}

void drawOptionsMenu(vita2d_pgf *font, MenuOptions *menu) {
    drawBackground();
    drawHeader(font, TR(TXT_ADV_OPTIONS), TR(TXT_ADV_OPTIONS_SUB));

    drawCard(80, 110, 800, 372);

    int maxVisible = 7;
    int startIdx = menu->scrollOffset;
    int endIdx = startIdx + maxVisible;
    if (endIdx > menu->total_options) endIdx = menu->total_options;

    for (int i = startIdx; i < endIdx; i++) {
        int displayIndex = i - startIdx;
        int y = 120 + displayIndex * 38;
        int isSelected = (i == menu->selected);

        if (isSelected) {
            vita2d_draw_rectangle(82, y, 780, 36, g_theme.selected);
            vita2d_draw_rectangle(82, y, 4, 36, g_theme.accent);
        }

        if (!isSpecialOptionIndex(i)) {
            drawCheckBox(112, y + 9, 18, menu->enabled[i]);
        }

        int textColor = isSelected ? g_theme.text_primary : (menu->enabled[i] ? g_theme.text_secondary : g_theme.text_faint);
        vita2d_pgf_draw_text(font, 152, y + 24, textColor, 0.96f, TR_OptionName(i));

        if (i == 47) {
            vita2d_pgf_draw_text(font, 720, y + 24, g_theme.accent, 0.85f, getThemeName(currentTheme));
        } else if (i == 48) {
            vita2d_pgf_draw_text(font, 720, y + 24, g_theme.accent, 0.85f, getLanguageName(languageMode));
        } else if (i != 7) {
            const char *stateText = menu->enabled[i] ? "ON" : "OFF";
            int stateColor = menu->enabled[i] ? g_theme.success : g_theme.text_faint;
            vita2d_pgf_draw_text(font, 815, y + 24, stateColor, 0.82f, stateText);
        }
    }

    drawScrollbar(868, 120, 260, maxVisible, menu->total_options, menu->scrollOffset);

    vita2d_draw_rectangle(100, 396, 760, 1, g_theme.border);
    vita2d_pgf_draw_text(font, 100, 422, g_theme.accent, 0.82f, TR(TXT_DETAILS));
    vita2d_pgf_draw_text(font, 100, 450, g_theme.text_secondary, 0.86f, TR_OptionDesc(menu->selected));

    drawFooterBar();
    drawButtonHint(font, 40, 526, "↕", TR(TXT_BTN_NAVIGATE));
    drawButtonHint(font, 210, 526, "L/R", "Page");
    drawButtonHint(font, 360, 526, "X", TR(TXT_BTN_TOGGLE));
    drawButtonHint(font, 530, 526, "O", TR(TXT_BTN_PREVIEW));
    drawButtonHint(font, 720, 526, "△", TR(TXT_BTN_BACK));
}

void drawPreviewScreen(vita2d_pgf *font, PreviewState *preview, int scanning) {
    drawBackground();
    drawHeader(font, TR(TXT_PREVIEW), TR(TXT_PREVIEW_SUB));

    if (scanning) {
        drawSpinner(font, 480, 260, TR(TXT_SCANNING_FILES));
        drawFooterBar();
        return;
    }

    if (!preview->fileList || preview->fileList->count == 0) {
        drawCard(260, 210, 440, 100);
        drawCenteredText(font, 480, 268, g_theme.warning, 1.05f, TR(TXT_NO_TEMP_FILES));
        drawFooterBar();
        drawButtonHint(font, 420, 526, "O", TR(TXT_BTN_BACK));
        return;
    }

    const char* sortLabel = (preview->sortMode == SORT_BY_SIZE) ? TR(TXT_SORT_SIZE) : TR(TXT_SORT_NAME);
    char infoText[160];
    snprintf(infoText, sizeof(infoText), "%s: %s  |  %s: %s%s%s",
             TR(TXT_BTN_SORT), sortLabel,
             TR(TXT_BTN_FILTER),
             strlen(preview->fileFilter) > 0 ? preview->fileFilter : TR(TXT_FILTER_ALL),
             preview->searchQuery[0] ? "  |  Query: " : "",
             preview->searchQuery[0] ? preview->searchQuery : "");
    vita2d_pgf_draw_text(font, 40, 120, g_theme.text_secondary, 0.80f, infoText);

    int selCount = countPreviewSelected(preview);
    char totalText[128];
    char sizeBuf[32];
    if (selCount > 0) {
        formatSize(sumPreviewSelectedSize(preview), sizeBuf, sizeof(sizeBuf));
        snprintf(totalText, sizeof(totalText), "Sel: %d (%s)  |  Vis: %d/%d",
                 selCount, sizeBuf, preview->visibleCount, preview->fileList->count);
    } else {
        formatSize(preview->totalVisibleSize, sizeBuf, sizeof(sizeBuf));
        snprintf(totalText, sizeof(totalText), "Files: %d/%d  |  %s",
                 preview->visibleCount, preview->fileList->count, sizeBuf);
    }
    vita2d_pgf_draw_text(font, 595, 120, g_theme.success, 0.80f, totalText);

    if (preview->visibleCount == 0) {
        drawCard(240, 210, 480, 100);
        drawCenteredText(font, 480, 268, g_theme.warning, 1.0f, "No matching files for current filter/search.");
        drawFooterBar();
        drawButtonHint(font, 160, 526, "□", TR(TXT_BTN_FILTER));
        drawButtonHint(font, 360, 526, "SEL", TR(TXT_BTN_ACTIONS));
        drawButtonHint(font, 620, 526, "O", TR(TXT_BTN_BACK));
        return;
    }

    int maxVisible = 16;
    int startIdx = preview->scrollOffset;
    int endIdx = startIdx + maxVisible;
    if (endIdx > preview->visibleCount) endIdx = preview->visibleCount;

    for (int i = startIdx; i < endIdx; i++) {
        int fileIdx = preview->visibleIndices[i];
        int rowIdx = i - startIdx;
        int y = 136 + rowIdx * 21;

        if (i == preview->selectedRow) {
            vita2d_draw_rectangle(40, y, 880, 21, g_theme.selected);
            vita2d_draw_rectangle(40, y, 3, 21, g_theme.accent);
        } else if (rowIdx % 2 == 1) {
            vita2d_draw_rectangle(40, y, 880, 21, g_theme.row_alt);
        }

        drawCheckBox(48, y + 3, 14, preview->fileList->files[fileIdx].selected);

        char displayPath[80];
        truncateMiddleOrTail(preview->fileList->files[fileIdx].path, displayPath, 76);

        int color = (i == preview->selectedRow) ? g_theme.text_primary : g_theme.text_secondary;
        vita2d_pgf_draw_text(font, 70, y + 15, color, 0.68f, displayPath);

        char sizeText[32];
        formatSize(preview->fileList->files[fileIdx].size, sizeText, sizeof(sizeText));
        vita2d_pgf_draw_text(font, 830, y + 15, g_theme.accent, 0.70f, sizeText);
    }

    drawScrollbar(928, 136, 336, maxVisible, preview->visibleCount, preview->scrollOffset);

    drawFooterBar();
    drawButtonHint(font, 18, 526, "X", TR(TXT_BTN_TOGGLE));
    drawButtonHint(font, 140, 526, "△", TR(TXT_BTN_SORT));
    drawButtonHint(font, 255, 526, "□", TR(TXT_BTN_FILTER));
    drawButtonHint(font, 380, 526, "SEL", TR(TXT_BTN_ACTIONS));
    drawButtonHint(font, 535, 526, "START", TR(TXT_BTN_CLEAN));
    drawButtonHint(font, 710, 526, "L/R", "Page");
    drawButtonHint(font, 845, 526, "O", TR(TXT_BTN_BACK));
}

void drawPreviewActionsModal(vita2d_pgf *font, int actionIdx) {
    vita2d_draw_rectangle(0, 0, 960, 544, RGBA(0, 0, 0, 170));
    drawCard(250, 120, 460, 300);
    vita2d_draw_rectangle(251, 121, 458, 3, g_theme.accent);

    drawCenteredText(font, 480, 154, g_theme.text_primary, 1.15f, TR(TXT_BTN_ACTIONS));
    vita2d_draw_rectangle(270, 170, 420, 1, g_theme.border);

    const char *items[6] = {
        "Delete Highlighted File",
        "Search Text (Keyboard)",
        "Clear Search Filter",
        "Select All Visible",
        "Unselect All Visible",
        "Add Path to Whitelist"
    };

    for (int i = 0; i < 6; i++) {
        int y = 182 + i * 36;
        if (i == actionIdx) {
            vita2d_draw_rectangle(270, y, 420, 32, g_theme.selected);
            vita2d_draw_rectangle(270, y, 4, 32, g_theme.accent);
        }
        int col = (i == actionIdx) ? g_theme.text_primary : g_theme.text_secondary;
        vita2d_pgf_draw_text(font, 286, y + 22, col, 0.88f, items[i]);
    }
}

void drawAppListScreen(vita2d_pgf *font, AppListState *appState, int scanning) {
    drawBackground();
    drawHeader(font, TR(TXT_SELECT_APP), TR(TXT_SELECT_APP_SUB));

    if (scanning) {
        drawSpinner(font, 480, 260, TR(TXT_SCANNING_APPS));
        drawFooterBar();
        return;
    }

    if (!appState->appList || appState->appList->count == 0) {
        drawCard(260, 210, 440, 100);
        drawCenteredText(font, 480, 268, g_theme.warning, 1.05f, TR(TXT_NO_APPS_FOUND));
        drawFooterBar();
        drawButtonHint(font, 420, 526, "O", TR(TXT_BTN_BACK));
        return;
    }

    unsigned long long totalSize = 0;
    for (int i = 0; i < appState->visibleCount; i++) {
        int idx = appState->visibleIndices[i];
        totalSize += appState->appList->apps[idx].tempSize;
    }

    char totalText[140];
    char sizeBuf[32];
    formatSize(totalSize, sizeBuf, sizeof(sizeBuf));
    snprintf(totalText, sizeof(totalText), "Apps: %d/%d  |  Temp: %s%s%s",
             appState->visibleCount, appState->appList->count, sizeBuf,
             appState->searchQuery[0] ? "  |  Query: " : "",
             appState->searchQuery[0] ? appState->searchQuery : "");
    vita2d_pgf_draw_text(font, 40, 120, g_theme.success, 0.84f, totalText);

    int maxVisible = 15;
    int startIdx = appState->scrollOffset;
    int endIdx = startIdx + maxVisible;
    if (endIdx > appState->visibleCount) endIdx = appState->visibleCount;

    for (int i = startIdx; i < endIdx; i++) {
        int appIdx = appState->visibleIndices[i];
        int rowIdx = i - startIdx;
        int y = 138 + rowIdx * 22;

        if (i == appState->selectedRow) {
            vita2d_draw_rectangle(40, y, 880, 22, g_theme.selected);
            vita2d_draw_rectangle(40, y, 3, 22, g_theme.accent);
        } else if (rowIdx % 2 == 1) {
            vita2d_draw_rectangle(40, y, 880, 22, g_theme.row_alt);
        }

        AppInfo *app = &appState->appList->apps[appIdx];
        int color = (i == appState->selectedRow) ? g_theme.text_primary : g_theme.text_secondary;

        vita2d_pgf_draw_text(font, 52, y + 16, g_theme.accent, 0.80f, app->titleId);
        char nameBuf[48];
        if (app->appName[0]) {
            truncateMiddleOrTail(app->appName, nameBuf, 44);
        } else {
            strcpy(nameBuf, "-");
        }
        vita2d_pgf_draw_text(font, 175, y + 16, color, 0.80f, nameBuf);

        char sizeText[32];
        formatSize(app->tempSize, sizeText, sizeof(sizeText));
        vita2d_pgf_draw_text(font, 815, y + 16, color, 0.80f, sizeText);
    }

    drawScrollbar(928, 138, 330, maxVisible, appState->visibleCount, appState->scrollOffset);

    drawFooterBar();
    drawButtonHint(font, 36, 526, "X", TR(TXT_BTN_CLEAN));
    drawButtonHint(font, 180, 526, "△", TR(TXT_BTN_SORT));
    drawButtonHint(font, 330, 526, "□", TR(TXT_BTN_SEARCH));
    drawButtonHint(font, 490, 526, "SEL", "Whitelist");
    drawButtonHint(font, 675, 526, "L/R", "Page");
    drawButtonHint(font, 815, 526, "O", TR(TXT_BTN_BACK));
}

void drawAppCleaningScreen(vita2d_pgf *font) {
    drawBackground();
    drawHeader(font, TR(TXT_CLEANING_APP), TR(TXT_REMOVING_TEMP));
    drawSpinner(font, 480, 260, TR(TXT_CLEANING_ELIPSIS));
    drawFooterBar();
    drawButtonHint(font, 380, 526, "O", TR(TXT_ESTOP_BTN));
}

void drawDeleteConfirmation(vita2d_pgf *font, PreviewState *preview) {
    vita2d_draw_rectangle(0, 0, 960, 544, RGBA(0, 0, 0, 170));

    drawCard(220, 170, 520, 210);
    vita2d_draw_rectangle(221, 171, 518, 3, g_theme.danger);

    drawCenteredText(font, 480, 210, g_theme.text_primary, 1.25f, TR(TXT_DELETE_FILE));
    drawCenteredText(font, 480, 245, g_theme.warning, 0.92f, TR(TXT_DELETE_FILE_Q));

    vita2d_draw_rectangle(250, 262, 460, 1, g_theme.border);

    char filename[80] = {0};
    char sizeText[32] = {0};
    if (preview->fileList && preview->selectedRow >= 0 && preview->selectedRow < preview->visibleCount) {
        int idx = preview->visibleIndices[preview->selectedRow];
        char *path = preview->fileList->files[idx].path;
        char *basename = strrchr(path, '/');
        basename = basename ? basename + 1 : path;
        truncateMiddleOrTail(basename, filename, 68);
        formatSize(preview->fileList->files[idx].size, sizeText, sizeof(sizeText));
    }

    drawCenteredText(font, 480, 295, g_theme.text_secondary, 0.88f, filename);
    drawCenteredText(font, 480, 322, g_theme.accent, 0.88f, sizeText);

    drawButtonHint(font, 300, 360, "X", TR(TXT_DELETE_FILE));
    drawButtonHint(font, 540, 360, "O", TR(TXT_BTN_CANCEL));
}

void drawCleanAllConfirmation(vita2d_pgf *font, int targetCount, unsigned long long targetBytes) {
    vita2d_draw_rectangle(0, 0, 960, 544, RGBA(0, 0, 0, 170));

    drawCard(220, 175, 520, 200);
    vita2d_draw_rectangle(221, 176, 518, 3, g_theme.warning);

    drawCenteredText(font, 480, 215, g_theme.text_primary, 1.25f, TR(TXT_CLEAN_ALL));
    drawCenteredText(font, 480, 248, g_theme.warning, 0.92f, TR(TXT_CLEAN_ALL_Q));

    vita2d_draw_rectangle(250, 266, 460, 1, g_theme.border);

    char sizeBuf[32];
    formatSize(targetBytes, sizeBuf, sizeof(sizeBuf));
    char filesCountText[96];
    snprintf(filesCountText, sizeof(filesCountText), "%s: %d  (%s)", TR(TXT_TOTAL_TO_DEL), targetCount, sizeBuf);
    drawCenteredText(font, 480, 302, g_theme.text_secondary, 0.92f, filesCountText);

    drawButtonHint(font, 300, 348, "X", TR(TXT_BTN_CLEAN));
    drawButtonHint(font, 540, 348, "O", TR(TXT_BTN_CANCEL));
}

void drawCheckIcon(int cx, int cy) {
    vita2d_draw_fill_circle(cx, cy, 48, RGBA(52, 211, 153, 50));
    vita2d_draw_fill_circle(cx, cy, 36, g_theme.success);
    for (int i = -1; i <= 1; i++) {
        vita2d_draw_line(cx - 16, cy + i, cx - 4, cy + 12 + i, RGBA(6, 30, 20, 255));
        vita2d_draw_line(cx - 4, cy + 12 + i, cx + 18, cy - 12 + i, RGBA(6, 30, 20, 255));
    }
}

void drawWarningIcon(int cx, int cy) {
    vita2d_draw_fill_circle(cx, cy, 48, RGBA(250, 204, 21, 45));
    vita2d_draw_fill_circle(cx, cy, 36, g_theme.warning);
    vita2d_pgf_draw_text(g_draw_font, cx - 6, cy + 15, RGBA(40, 30, 0, 255), 1.8f, "!");
}

void drawStatBox(vita2d_pgf *font, int x, int y, const char *label, const char *value, int valueColor) {
    drawCard(x, y, 230, 96);
    vita2d_pgf_draw_text(font, x + 22, y + 34, g_theme.text_secondary, 0.88f, label);
    vita2d_pgf_draw_text(font, x + 22, y + 70, valueColor, 1.12f, value);
}

void drawResultScreen(vita2d_pgf *font, ResultKind kind, int cleanupCount, const char *titleId, const char *spaceText, int filesDeleted) {
    drawBackground();

    if (kind == RESULT_INTERRUPTED) {
        drawWarningIcon(480, 125);
        drawCenteredText(font, 480, 208, g_theme.warning, 1.75f, TR(TXT_OP_INTERRUPTED));
        drawCenteredText(font, 480, 245, g_theme.text_secondary, 0.95f, TR(TXT_OP_INTERRUPTED_DESC));
    } else if (kind == RESULT_APP_CLEANED) {
        drawCheckIcon(480, 120);
        drawCenteredText(font, 480, 200, g_theme.success, 1.75f, TR(TXT_APP_CLEANED));
        char titleText[96];
        snprintf(titleText, sizeof(titleText), "App: %s", titleId);
        drawCenteredText(font, 480, 238, g_theme.text_secondary, 0.95f, titleText);
    } else {
        drawCheckIcon(480, 125);
        drawCenteredText(font, 480, 208, g_theme.success, 1.75f, TR(TXT_CLEANING_DONE));
        drawCenteredText(font, 480, 245, g_theme.text_secondary, 0.98f, TR(TXT_GREAT_JOB));
    }

    drawStatBox(font, 235, 280, TR(TXT_SPACE_FREED), spaceText, g_theme.accent);

    char filesText[64];
    snprintf(filesText, sizeof(filesText), "%d", filesDeleted);
    drawStatBox(font, 495, 280, TR(TXT_FILES_DELETED), filesText, g_theme.success);

    if (kind == RESULT_CLEAN_DONE) {
        char countText[64];
        snprintf(countText, sizeof(countText), "Cleanup #%d", cleanupCount);
        drawCenteredText(font, 480, 412, g_theme.text_faint, 0.88f, countText);
    }

    drawFooterBar();
    drawButtonHint(font, 380, 526, "X", TR(TXT_BTN_CONTINUE));
    drawButtonHint(font, 540, 526, "O", TR(TXT_BTN_BACK));
}

void startPreviewScan(PreviewState *preview) {
    waitBgIdle();
    preview->scrollOffset = 0;
    preview->selectedRow = 0;
    if (preview->fileList) {
        freeFileList(preview->fileList);
        preview->fileList = NULL;
    }
    preview->fileList = createFileList();
    if (!preview->fileList) return;
    g_bgPreviewList = preview->fileList;
    g_bgSortMode = preview->sortMode;
    safe_strncpy(g_bgFileFilter, preview->fileFilter, MAX_FILE_FILTER_LENGTH);
    safe_strncpy(g_bgSearchQuery, preview->searchQuery, sizeof(g_bgSearchQuery));
    g_bgVisibleSize = 0;
    requestBgTask(BG_TASK_SCAN_PREVIEW);
}

void startAppScan(AppListState *appState) {
    waitBgIdle();
    if (!appState->appList) {
        appState->appList = createAppList();
    }
    if (!appState->appList) return;
    g_bgAppList = appState->appList;
    requestBgTask(BG_TASK_SCAN_APPS);
}

void drawToolsMenu(vita2d_pgf *font, int selected, int scrollOffset) {
    drawBackground();
    drawHeader(font, TR(TXT_TOOLS_TITLE), TR(TXT_TOOLS_SUB));

    const char* names[TOOLS_MENU_COUNT] = {
        TR(TXT_TOOL_ANALYZER),
        TR(TXT_TOOL_SMART),
        TR(TXT_TOOL_ORPHAN),
        TR(TXT_TOOL_LARGE),
        TR(TXT_TOOL_TAI),
        TR(TXT_TOOL_HISTORY),
        TR(TXT_TOOL_WHITELIST),
        TR(TXT_TOOL_ABOUT)
    };
    const char* descs[TOOLS_MENU_COUNT] = {
        TR(TXT_TOOL_ANALYZER_DESC),
        TR(TXT_TOOL_SMART_DESC),
        TR(TXT_TOOL_ORPHAN_DESC),
        TR(TXT_TOOL_LARGE_DESC),
        TR(TXT_TOOL_TAI_DESC),
        TR(TXT_TOOL_HISTORY_DESC),
        TR(TXT_TOOL_WHITELIST_DESC),
        TR(TXT_TOOL_ABOUT_DESC)
    };

    int maxVisible = 5;
    int cardX = 110, cardW = 740, cardH = 68;
    int startIdx = scrollOffset;
    int endIdx = startIdx + maxVisible;
    if (endIdx > TOOLS_MENU_COUNT) endIdx = TOOLS_MENU_COUNT;

    for (int i = startIdx; i < endIdx; i++) {
        int row = i - startIdx;
        int y = 108 + row * 76;
        int isSelected = (i == selected);

        vita2d_draw_rectangle(cardX, y, cardW, cardH, isSelected ? g_theme.card_solid : g_theme.card);
        drawCardBorder(cardX, y, cardW, cardH, isSelected ? g_theme.accent : g_theme.border);

        if (isSelected) {
            vita2d_draw_rectangle(cardX + 1, y + 1, 5, cardH - 2, g_theme.accent);
            vita2d_draw_rectangle(cardX + 1, y + 1, cardW - 2, cardH - 2, g_theme.accent_soft);
        }

        vita2d_pgf_draw_text(font, cardX + 22, y + 28, isSelected ? g_theme.text_primary : g_theme.text_secondary, 1.05f, names[i]);
        vita2d_pgf_draw_text(font, cardX + 22, y + 52, g_theme.text_faint, 0.76f, descs[i]);
    }

    drawScrollbar(866, 108, 372, maxVisible, TOOLS_MENU_COUNT, scrollOffset);

    drawFooterBar();
    drawButtonHint(font, 100, 526, "↕", TR(TXT_BTN_NAVIGATE));
    drawButtonHint(font, 360, 526, "X", TR(TXT_BTN_CONFIRM));
    drawButtonHint(font, 760, 526, "O", TR(TXT_BTN_BACK));
}

void drawAnalyzerScreen(vita2d_pgf *font, int scanning, int selectedIdx, int scrollOffset) {
    drawBackground();
    drawHeader(font, TR(TXT_ANALYZER_TITLE), g_analyzerPathDepth > 0 ? g_analyzerBrowsePath : TR(TXT_ANALYZER_SUB));

    if (scanning) {
        drawSpinner(font, 480, 260, TR(TXT_ANALYZER_WAIT));
        drawFooterBar();
        return;
    }

    if (g_analyzerVolumeCount == 0 || g_analyzerActiveVolume < 0) {
        drawCenteredText(font, 480, 278, g_theme.warning, 1.1f, TR(TXT_NO_VOLUMES));
        drawFooterBar();
        drawButtonHint(font, 420, 526, "O", TR(TXT_BTN_BACK));
        return;
    }

    AnalyzerVolumeState *vol = (AnalyzerVolumeState*)&g_analyzerVolumes[g_analyzerActiveVolume];

    drawCard(40, 102, 880, 82);

    char volText[128];
    if (g_analyzerPathDepth > 0) {
        truncateMiddleOrTail(g_analyzerBrowsePath, volText, 58);
    } else {
        snprintf(volText, sizeof(volText), "<  %s  >", vol->volume);
    }
    drawCenteredText(font, 480, 126, g_theme.accent, 1.05f, volText);

    char tStr[32], uStr[32], fStr[32];
    formatSize(vol->totalSize, tStr, sizeof(tStr));
    formatSize(vol->usedSize, uStr, sizeof(uStr));
    formatSize(vol->freeSize, fStr, sizeof(fStr));

    char spaceText[128];
    snprintf(spaceText, sizeof(spaceText), "Total: %s   Used: %s (%d%%)   Free: %s",
             tStr, uStr, vol->usedPercent, fStr);
    drawCenteredText(font, 480, 148, g_theme.text_secondary, 0.80f, spaceText);

    int barW = 800, barH = 11, barX = 80, barY = 160;
    vita2d_draw_rectangle(barX, barY, barW, barH, g_theme.card_dark);
    drawCardBorder(barX, barY, barW, barH, g_theme.border);
    if (vol->totalSize > 0) {
        int fillW = (int)((vol->usedSize * barW) / vol->totalSize);
        if (fillW > barW) fillW = barW;
        if (fillW > 0) {
            vita2d_draw_rectangle(barX + 1, barY + 1, fillW - 2, barH - 2, g_theme.accent);
        }
    }

    drawCard(40, 192, 330, 290);
    vita2d_pgf_draw_text(font, 60, 218, g_theme.text_secondary, 0.88f, TR(TXT_CATEGORIES));
    vita2d_draw_rectangle(56, 228, 298, 1, g_theme.border);

    for (int i = 0; i < vol->categoryCount && i < 8; i++) {
        char catStr[32];
        formatSize(vol->categories[i].size, catStr, sizeof(catStr));
        int y = 254 + i * 27;
        vita2d_pgf_draw_text(font, 60, y, g_theme.text_primary, 0.78f, vol->categories[i].name);
        vita2d_pgf_draw_text(font, 240, y, g_theme.accent, 0.78f, catStr);
    }

    drawCard(386, 192, 534, 290);
    vita2d_pgf_draw_text(font, 404, 218, g_theme.text_secondary, 0.88f, TR(TXT_TOP_DIRS));
    vita2d_draw_rectangle(402, 228, 502, 1, g_theme.border);

    int maxVis = 8;
    int start = scrollOffset;
    int end = start + maxVis;
    if (end > g_analyzerDirEntryCount) end = g_analyzerDirEntryCount;

    for (int i = start; i < end; i++) {
        int row = i - start;
        int y = 236 + row * 29;
        AnalyzerDirEntry *e = &g_analyzerDirEntries[i];

        if (i == selectedIdx) {
            vita2d_draw_rectangle(392, y, 512, 27, g_theme.selected);
            vita2d_draw_rectangle(392, y, 3, 27, g_theme.accent);
        }

        const char *tag = e->isDir ? "[D]" : "[F]";
        vita2d_pgf_draw_text(font, 402, y + 19, e->isDir ? g_theme.accent : g_theme.text_faint, 0.72f, tag);

        char shortName[34];
        truncateMiddleOrTail(e->name, shortName, 29);
        int nameCol = (i == selectedIdx) ? g_theme.text_primary : g_theme.text_secondary;
        vita2d_pgf_draw_text(font, 434, y + 19, nameCol, 0.76f, shortName);

        char dirStr[32];
        formatSize(e->size, dirStr, sizeof(dirStr));
        vita2d_pgf_draw_text(font, 750, y + 19, g_theme.accent, 0.76f, dirStr);

        if (!e->deletable) {
            vita2d_pgf_draw_text(font, 858, y + 19, g_theme.danger, 0.65f, "LOCK");
        }
    }

    drawScrollbar(908, 236, 232, maxVis, g_analyzerDirEntryCount, scrollOffset);

    drawFooterBar();
    if (g_analyzerPathDepth == 0) {
        drawButtonHint(font, 24, 526, "L/R", TR(TXT_BTN_SWITCH_VOL));
    } else {
        drawButtonHint(font, 24, 526, "O", TR(TXT_BTN_UP));
    }
    drawButtonHint(font, 200, 526, "X", TR(TXT_BTN_OPEN));
    drawButtonHint(font, 355, 526, "□", TR(TXT_BTN_RESCAN));
    drawButtonHint(font, 540, 526, "△", TR(TXT_DELETE_FILE));
    if (g_analyzerPathDepth == 0) {
        drawButtonHint(font, 770, 526, "O", TR(TXT_BTN_BACK));
    }
}

void drawSmartPreview(vita2d_pgf *font) {
    drawBackground();
    drawHeader(font, TR(TXT_SMART_TITLE), TR(TXT_SMART_SUB));

    drawCard(60, 104, 840, 376);

    char s1[32], s2[32], s3[32], s4[32];
    formatSize(g_smartCleanPlan.bytes[GRP_TEMPORARY], s1, sizeof(s1));
    formatSize(g_smartCleanPlan.bytes[GRP_CACHE], s2, sizeof(s2));
    formatSize(g_smartCleanPlan.bytes[GRP_CRASH], s3, sizeof(s3));
    formatSize(g_smartCleanPlan.bytes[GRP_ORPHANED], s4, sizeof(s4));

    vita2d_pgf_draw_text(font, 100, 144, g_theme.text_primary, 0.92f, TR(TXT_SMART_TEMP));
    char t1[64]; snprintf(t1, sizeof(t1), "%llu files  (%s)", g_smartCleanPlan.files[GRP_TEMPORARY], s1);
    vita2d_pgf_draw_text(font, 370, 144, g_theme.accent, 0.92f, t1);

    vita2d_pgf_draw_text(font, 100, 179, g_theme.text_primary, 0.92f, TR(TXT_SMART_CACHE));
    char t2[64]; snprintf(t2, sizeof(t2), "%llu files  (%s)", g_smartCleanPlan.files[GRP_CACHE], s2);
    vita2d_pgf_draw_text(font, 370, 179, g_theme.accent, 0.92f, t2);

    vita2d_pgf_draw_text(font, 100, 214, g_theme.text_primary, 0.92f, TR(TXT_SMART_CRASH));
    char t3[64]; snprintf(t3, sizeof(t3), "%llu files  (%s)", g_smartCleanPlan.files[GRP_CRASH], s3);
    vita2d_pgf_draw_text(font, 370, 214, g_theme.accent, 0.92f, t3);

    vita2d_pgf_draw_text(font, 100, 249, g_theme.text_primary, 0.92f, TR(TXT_SMART_ORPHAN));
    char t4[64]; snprintf(t4, sizeof(t4), "%llu items  (%s)", g_smartCleanPlan.files[GRP_ORPHANED], s4);
    vita2d_pgf_draw_text(font, 370, 249, g_theme.accent, 0.92f, t4);

    vita2d_draw_rectangle(90, 272, 780, 1, g_theme.border);

    char totS[32];
    formatSize(safetyPlanPotentialBytes(&g_smartCleanPlan), totS, sizeof(totS));
    char totText[128];
    snprintf(totText, sizeof(totText), "%llu files  |  %s", safetyPlanPotentialFiles(&g_smartCleanPlan), totS);
    vita2d_pgf_draw_text(font, 100, 312, g_theme.success, 1.1f, TR(TXT_SMART_POTENTIAL));
    vita2d_pgf_draw_text(font, 370, 312, g_theme.success, 1.1f, totText);

    char reviewText[128];
    snprintf(reviewText, sizeof(reviewText), TR(TXT_SMART_REVIEW_INFO), g_smartCleanPlan.files[GRP_REVIEW]);
    vita2d_pgf_draw_text(font, 100, 355, g_theme.warning, 0.80f, reviewText);

    char protText[128];
    snprintf(protText, sizeof(protText), TR(TXT_SMART_PROT_INFO), g_smartCleanPlan.files[GRP_PROTECTED]);
    vita2d_pgf_draw_text(font, 100, 385, g_theme.text_faint, 0.80f, protText);

    vita2d_pgf_draw_text(font, 100, 415, g_theme.text_secondary, 0.78f, TR(TXT_SMART_SAFE_NOTE));

    drawFooterBar();
    drawButtonHint(font, 300, 526, "X", TR(TXT_BTN_CLEAN));
    drawButtonHint(font, 760, 526, "O", TR(TXT_BTN_CANCEL));
}

void drawSmartReport(vita2d_pgf *font) {
    drawBackground();
    drawCheckIcon(480, 125);
    drawCenteredText(font, 480, 205, g_theme.success, 1.55f, TR(TXT_SMART_REPORT_DONE));

    drawCard(120, 235, 720, 230);

    char str[128];
    snprintf(str, sizeof(str), "Files Scanned: %llu", g_cleanReport.filesScanned);
    vita2d_pgf_draw_text(font, 160, 280, g_theme.text_primary, 0.90f, str);

    snprintf(str, sizeof(str), "%s: %llu", TR(TXT_FILES_DELETED), g_cleanReport.filesDeleted);
    vita2d_pgf_draw_text(font, 160, 320, g_theme.success, 0.90f, str);

    snprintf(str, sizeof(str), "Skipped / Protected: %llu", g_cleanReport.filesSkipped);
    vita2d_pgf_draw_text(font, 490, 280, g_theme.text_secondary, 0.90f, str);

    snprintf(str, sizeof(str), "Failed: %llu", g_cleanReport.filesFailed);
    vita2d_pgf_draw_text(font, 490, 320, g_cleanReport.filesFailed > 0 ? g_theme.warning : g_theme.text_secondary, 0.90f, str);

    vita2d_draw_rectangle(150, 355, 660, 1, g_theme.border);

    char s[32];
    formatSize(g_cleanReport.bytesRecovered, s, sizeof(s));
    snprintf(str, sizeof(str), "%s: %s", TR(TXT_SPACE_FREED), s);
    vita2d_pgf_draw_text(font, 160, 400, g_theme.accent, 1.1f, str);

    snprintf(str, sizeof(str), "Elapsed Time: %u s", g_cleanReport.elapsedSeconds);
    vita2d_pgf_draw_text(font, 490, 400, g_theme.text_secondary, 0.92f, str);

    drawFooterBar();
    drawButtonHint(font, 380, 526, "X", TR(TXT_BTN_CONTINUE));
    drawButtonHint(font, 760, 526, "O", TR(TXT_BTN_BACK));
}

void drawOrphanScanScreen(vita2d_pgf *font, int phase, int scrollOffset, int selectedItem) {
    drawBackground();
    drawHeader(font, TR(TXT_ORPHAN_TITLE), TR(TXT_ORPHAN_SUB));

    if (phase == 0) {
        drawSpinner(font, 480, 260, TR(TXT_ORPHAN_WAIT));
        drawFooterBar();
        return;
    }

    if (g_orphanItemCount == 0) {
        drawCenteredText(font, 480, 278, g_theme.success, 1.05f, TR(TXT_ORPHAN_CLEAN));
        drawFooterBar();
        drawButtonHint(font, 420, 526, "O", TR(TXT_BTN_BACK));
        return;
    }

    if (g_orphanTruncated) {
        vita2d_pgf_draw_text(font, 40, 114, g_theme.warning, 0.82f, TR(TXT_ORPHAN_TRUNCATED));
    }

    int maxVisible = 14;
    int startIdx = scrollOffset;
    int endIdx = startIdx + maxVisible;
    if (endIdx > g_orphanItemCount) endIdx = g_orphanItemCount;

    for (int i = startIdx; i < endIdx; i++) {
        int rowIdx = i - startIdx;
        int y = 132 + rowIdx * 25;

        SafetyItem *item = (SafetyItem*)&g_orphanItems[i];

        if (i == selectedItem) {
            vita2d_draw_rectangle(40, y, 880, 24, g_theme.selected);
            vita2d_draw_rectangle(40, y, 3, 24, g_theme.accent);
        } else if (rowIdx % 2 == 1) {
            vita2d_draw_rectangle(40, y, 880, 24, g_theme.row_alt);
        }

        if (item->category != CAT_PROTECTED) {
            drawCheckBox(52, y + 4, 16, item->user_selected);
        }

        int color = (item->category == CAT_PROTECTED) ? g_theme.danger : g_theme.text_primary;

        char labelBuf[96];
        if (item->appName[0]) {
            char shortPath[24];
            truncateMiddleOrTail(item->path, shortPath, 21);
            snprintf(labelBuf, sizeof(labelBuf), "%s (%s)", item->appName, shortPath);
        } else {
            truncateMiddleOrTail(item->path, labelBuf, 44);
        }
        char clippedLabel[48];
        truncateMiddleOrTail(labelBuf, clippedLabel, 42);

        vita2d_pgf_draw_text(font, 78, y + 18, color, 0.78f, item->titleId[0] ? item->titleId : "-");
        vita2d_pgf_draw_text(font, 178, y + 18, color, 0.76f, clippedLabel);

        char s[32];
        formatSize(item->size, s, sizeof(s));
        vita2d_pgf_draw_text(font, 650, y + 18, g_theme.accent, 0.78f, s);

        const char *catName = (item->category == CAT_PROTECTED) ? "SAVE/PROT" : safetyCategoryName(item->category);
        vita2d_pgf_draw_text(font, 770, y + 18, g_theme.text_secondary, 0.75f, catName);
    }

    drawScrollbar(930, 132, 350, maxVisible, g_orphanItemCount, scrollOffset);

    drawFooterBar();
    drawButtonHint(font, 24, 526, "X", TR(TXT_BTN_TOGGLE));
    drawButtonHint(font, 160, 526, "□", TR(TXT_BTN_SEL_ALL));
    drawButtonHint(font, 315, 526, "△", TR(TXT_BTN_DELETE_SEL));
    drawButtonHint(font, 500, 526, "SEL", "Save ZIP / WL");
    drawButtonHint(font, 710, 526, "L/R", "Page");
    drawButtonHint(font, 845, 526, "O", TR(TXT_BTN_BACK));
}

void drawOrphanActionsModal(vita2d_pgf *font, const SafetyItem *item, int actionIdx, int isSavedata) {
    vita2d_draw_rectangle(0, 0, 960, 544, RGBA(0, 0, 0, 170));
    drawCard(230, 140, 500, 240);
    vita2d_draw_rectangle(231, 141, 498, 3, g_theme.accent);

    drawCenteredText(font, 480, 176, g_theme.text_primary, 1.12f, item->titleId[0] ? item->titleId : TR(TXT_BTN_ACTIONS));
    vita2d_draw_rectangle(250, 194, 460, 1, g_theme.border);

    int count = isSavedata ? 3 : 1;
    const char *saveActions[3] = {
        "Backup Save to ZIP",
        "Backup ZIP + Delete Original",
        "Add to Whitelist"
    };
    const char *otherActions[1] = {
        "Add to Whitelist"
    };

    for (int i = 0; i < count; i++) {
        int y = 212 + i * 42;
        if (i == actionIdx) {
            vita2d_draw_rectangle(250, y, 460, 36, g_theme.selected);
            vita2d_draw_rectangle(250, y, 4, 36, g_theme.accent);
        }
        const char *txt = isSavedata ? saveActions[i] : otherActions[i];
        int col = (i == actionIdx) ? g_theme.text_primary : g_theme.text_secondary;
        vita2d_pgf_draw_text(font, 268, y + 24, col, 0.90f, txt);
    }
}

void drawOrphanDeleteConfirmation(vita2d_pgf *font) {
    vita2d_draw_rectangle(0, 0, 960, 544, RGBA(0, 0, 0, 170));
    drawCard(220, 180, 520, 190);
    vita2d_draw_rectangle(221, 181, 518, 3, g_theme.warning);

    drawCenteredText(font, 480, 220, g_theme.text_primary, 1.25f, TR(TXT_ORPHAN_DEL_TITLE));
    drawCenteredText(font, 480, 255, g_theme.warning, 0.92f, TR(TXT_ORPHAN_DEL_Q));

    vita2d_draw_rectangle(250, 272, 460, 1, g_theme.border);

    int selCount = 0;
    for (int i = 0; i < g_orphanItemCount; i++) {
        if (g_orphanItems[i].user_selected) selCount++;
    }

    char txt[64];
    snprintf(txt, sizeof(txt), "%s: %d", TR(TXT_TOTAL_TO_DEL), selCount);
    drawCenteredText(font, 480, 305, g_theme.text_secondary, 0.92f, txt);

    drawButtonHint(font, 300, 345, "X", TR(TXT_BTN_DELETE_SEL));
    drawButtonHint(font, 540, 345, "O", TR(TXT_BTN_CANCEL));
}

void drawLargeFilesScreen(vita2d_pgf *font, int scanning, int scrollOffset, int selectedItem) {
    drawBackground();
    drawHeader(font, TR(TXT_LARGE_TITLE), TR(TXT_LARGE_SUB));

    if (scanning) {
        drawSpinner(font, 480, 260, TR(TXT_LARGE_WAIT));
        drawFooterBar();
        return;
    }

    char hdr[96];
    snprintf(hdr, sizeof(hdr), "Threshold: >= %d MB (+ VPK/PKG/ZIP/7Z)  |  Found: %d%s",
             g_largeMinSizeMB, g_largeItemCount, g_largeTruncated ? "+" : "");
    vita2d_pgf_draw_text(font, 40, 118, g_theme.accent, 0.82f, hdr);

    if (g_largeItemCount == 0) {
        drawCard(220, 210, 520, 100);
        drawCenteredText(font, 480, 268, g_theme.success, 1.0f, TR(TXT_LARGE_NONE));
        drawFooterBar();
        drawButtonHint(font, 260, 526, "□", "Threshold");
        drawButtonHint(font, 580, 526, "O", TR(TXT_BTN_BACK));
        return;
    }

    int maxVisible = 14;
    int start = scrollOffset;
    int end = start + maxVisible;
    if (end > g_largeItemCount) end = g_largeItemCount;

    for (int i = start; i < end; i++) {
        int row = i - start;
        int y = 136 + row * 25;
        LargeFileItem *it = &g_largeItems[i];

        if (i == selectedItem) {
            vita2d_draw_rectangle(40, y, 880, 24, g_theme.selected);
            vita2d_draw_rectangle(40, y, 3, 24, g_theme.accent);
        } else if (row % 2 == 1) {
            vita2d_draw_rectangle(40, y, 880, 24, g_theme.row_alt);
        }

        drawCheckBox(50, y + 4, 16, it->selected);

        const char *tag = "LARGE";
        int tagCol = g_theme.warning;
        if (it->kind == LARGE_KIND_VPK) { tag = "VPK"; tagCol = g_theme.accent; }
        else if (it->kind == LARGE_KIND_PKG) { tag = "PKG"; tagCol = g_theme.accent; }
        else if (it->kind == LARGE_KIND_ARCHIVE) { tag = "ARCH"; tagCol = g_theme.text_secondary; }

        vita2d_pgf_draw_text(font, 76, y + 18, tagCol, 0.72f, tag);

        char shortPath[64];
        truncateMiddleOrTail(it->path, shortPath, 58);
        int textCol = (i == selectedItem) ? g_theme.text_primary : g_theme.text_secondary;
        vita2d_pgf_draw_text(font, 145, y + 18, textCol, 0.74f, shortPath);

        char s[32];
        formatSize(it->size, s, sizeof(s));
        vita2d_pgf_draw_text(font, 815, y + 18, g_theme.accent, 0.78f, s);
    }

    drawScrollbar(930, 136, 350, maxVisible, g_largeItemCount, scrollOffset);

    drawFooterBar();
    drawButtonHint(font, 24, 526, "X", TR(TXT_BTN_TOGGLE));
    drawButtonHint(font, 155, 526, "□", "Min Size");
    drawButtonHint(font, 320, 526, "△", TR(TXT_BTN_DELETE_SEL));
    drawButtonHint(font, 505, 526, "SEL", "Whitelist");
    drawButtonHint(font, 680, 526, "L/R", "Page");
    drawButtonHint(font, 830, 526, "O", TR(TXT_BTN_BACK));
}

void drawTaiAuditScreen(vita2d_pgf *font, int scanning, int scrollOffset, int selectedItem) {
    drawBackground();
    drawHeader(font, TR(TXT_TAI_TITLE), TR(TXT_TAI_SUB));

    if (scanning) {
        drawSpinner(font, 480, 260, TR(TXT_TAI_WAIT));
        drawFooterBar();
        return;
    }

    drawCard(40, 102, 880, 66);
    vita2d_pgf_draw_text(font, 56, 126,
                         g_taiAudit.conflictDetected ? g_theme.danger : g_theme.success,
                         0.85f,
                         g_taiAudit.conflictDetected ? TR(TXT_TAI_CONFLICT) : TR(TXT_TAI_NO_CONFLICT));

    char summary[160];
    snprintf(summary, sizeof(summary), "Config: %s  |  Unused/Bak: %d  |  Broken: %d  |  Dup: %d",
             g_taiAudit.activeConfig, g_taiAudit.unusedCount, g_taiAudit.brokenCount, g_taiAudit.duplicateCount);
    vita2d_pgf_draw_text(font, 56, 152, g_theme.text_secondary, 0.78f, summary);

    if (g_taiAudit.entryCount == 0) {
        drawCenteredText(font, 480, 280, g_theme.text_secondary, 1.0f, "No TaiHEN entries found.");
        drawFooterBar();
        drawButtonHint(font, 420, 526, "O", TR(TXT_BTN_BACK));
        return;
    }

    int maxVisible = 12;
    int start = scrollOffset;
    int end = start + maxVisible;
    if (end > g_taiAudit.entryCount) end = g_taiAudit.entryCount;

    for (int i = start; i < end; i++) {
        int row = i - start;
        int y = 178 + row * 25;
        TaiAuditEntry *e = &g_taiAudit.entries[i];

        if (i == selectedItem) {
            vita2d_draw_rectangle(40, y, 880, 24, g_theme.selected);
            vita2d_draw_rectangle(40, y, 3, 24, g_theme.accent);
        } else if (row % 2 == 1) {
            vita2d_draw_rectangle(40, y, 880, 24, g_theme.row_alt);
        }

        int canSel = (e->status == TAI_UNUSED || e->status == TAI_BACKUP);
        if (canSel) {
            drawCheckBox(50, y + 4, 16, e->userSelected);
        }

        const char *stLabel = "OK";
        int stCol = g_theme.success;
        if (e->status == TAI_UNUSED) { stLabel = "UNUSED"; stCol = g_theme.warning; }
        else if (e->status == TAI_BACKUP) { stLabel = "BACKUP"; stCol = g_theme.accent; }
        else if (e->status == TAI_BROKEN) { stLabel = "BROKEN"; stCol = g_theme.danger; }
        else if (e->status == TAI_DUPLICATE) { stLabel = "DUP"; stCol = g_theme.warning; }

        vita2d_pgf_draw_text(font, 76, y + 18, stCol, 0.74f, stLabel);
        vita2d_pgf_draw_text(font, 160, y + 18, g_theme.text_faint, 0.72f, e->section);

        char shortPath[60];
        truncateMiddleOrTail(e->path, shortPath, 54);
        int pCol = (i == selectedItem) ? g_theme.text_primary : g_theme.text_secondary;
        vita2d_pgf_draw_text(font, 285, y + 18, pCol, 0.74f, shortPath);

        if (e->size > 0) {
            char sz[32];
            formatSize(e->size, sz, sizeof(sz));
            vita2d_pgf_draw_text(font, 820, y + 18, g_theme.accent, 0.74f, sz);
        }
    }

    drawScrollbar(930, 178, 300, maxVisible, g_taiAudit.entryCount, scrollOffset);

    drawFooterBar();
    drawButtonHint(font, 24, 526, "X", TR(TXT_BTN_TOGGLE));
    drawButtonHint(font, 170, 526, "□", TR(TXT_BTN_BACKUP_CFG));
    drawButtonHint(font, 360, 526, "△", TR(TXT_BTN_DELETE_SEL));
    drawButtonHint(font, 560, 526, "R", TR(TXT_BTN_RESCAN));
    drawButtonHint(font, 790, 526, "O", TR(TXT_BTN_BACK));
}

void drawHistoryScreen(vita2d_pgf *font, int scrollOffset) {
    drawBackground();
    drawHeader(font, TR(TXT_HISTORY_TITLE), TR(TXT_HISTORY_SUB));

    LifetimeStats st;
    historyGetLifetimeStats(&st);
    char totalBuf[32];
    formatSize(st.totalBytesFreed, totalBuf, sizeof(totalBuf));

    drawCard(40, 102, 880, 50);
    char hdr[140];
    snprintf(hdr, sizeof(hdr), "Lifetime Cleanups: %u   |   Files Deleted: %llu   |   Total Freed: %s",
             st.totalCleanups, st.totalFilesDeleted, totalBuf);
    drawCenteredText(font, 480, 134, g_theme.accent, 0.88f, hdr);

    int count = historyGetCount();
    if (count == 0) {
        drawCard(240, 220, 480, 100);
        drawCenteredText(font, 480, 276, g_theme.text_secondary, 1.0f, TR(TXT_HISTORY_EMPTY));
        drawFooterBar();
        drawButtonHint(font, 420, 526, "O", TR(TXT_BTN_BACK));
        return;
    }

    int maxVisible = 13;
    int start = scrollOffset;
    int end = start + maxVisible;
    if (end > count) end = count;

    for (int i = start; i < end; i++) {
        int row = i - start;
        int y = 164 + row * 24;
        const HistoryEntry *e = historyGetEntry(i);
        if (!e) continue;

        if (row % 2 == 1) {
            vita2d_draw_rectangle(40, y, 880, 23, g_theme.row_alt);
        }

        char sz[32];
        formatSize(e->bytesFreed, sz, sizeof(sz));
        vita2d_pgf_draw_text(font, 52, y + 17, g_theme.text_faint, 0.74f, e->timestamp);
        vita2d_pgf_draw_text(font, 250, y + 17, g_theme.text_primary, 0.76f, e->mode);

        char files[32];
        snprintf(files, sizeof(files), "%d files", e->deleted);
        vita2d_pgf_draw_text(font, 510, y + 17, g_theme.text_secondary, 0.76f, files);
        vita2d_pgf_draw_text(font, 650, y + 17, g_theme.accent, 0.76f, sz);
        vita2d_pgf_draw_text(font, 795, y + 17, e->interrupted ? g_theme.warning : g_theme.success, 0.72f,
                             e->interrupted ? "PARTIAL" : "OK");
    }

    drawScrollbar(930, 164, 312, maxVisible, count, scrollOffset);

    drawFooterBar();
    drawButtonHint(font, 80, 526, "↕", TR(TXT_BTN_NAVIGATE));
    drawButtonHint(font, 360, 526, "□", TR(TXT_BTN_CLEAR));
    drawButtonHint(font, 760, 526, "O", TR(TXT_BTN_BACK));
}

void drawWhitelistScreen(vita2d_pgf *font, int scrollOffset, int selectedItem) {
    drawBackground();
    drawHeader(font, TR(TXT_WHITELIST_TITLE), TR(TXT_WHITELIST_SUB));

    int count = whitelistGetCount();
    if (count == 0) {
        drawCard(220, 210, 520, 100);
        drawCenteredText(font, 480, 268, g_theme.text_secondary, 1.0f, TR(TXT_WHITELIST_EMPTY));
        drawFooterBar();
        drawButtonHint(font, 260, 526, "□", "Add Entry");
        drawButtonHint(font, 620, 526, "O", TR(TXT_BTN_BACK));
        return;
    }

    int maxVisible = 14;
    int start = scrollOffset;
    int end = start + maxVisible;
    if (end > count) end = count;

    for (int i = start; i < end; i++) {
        int row = i - start;
        int y = 114 + row * 26;

        if (i == selectedItem) {
            vita2d_draw_rectangle(40, y, 880, 25, g_theme.selected);
            vita2d_draw_rectangle(40, y, 3, 25, g_theme.accent);
        } else if (row % 2 == 1) {
            vita2d_draw_rectangle(40, y, 880, 25, g_theme.row_alt);
        }

        char buf[88];
        truncateMiddleOrTail(whitelistGetItem(i), buf, 82);
        int col = (i == selectedItem) ? g_theme.text_primary : g_theme.text_secondary;
        vita2d_pgf_draw_text(font, 56, y + 18, col, 0.80f, buf);
    }

    drawScrollbar(930, 114, 364, maxVisible, count, scrollOffset);

    drawFooterBar();
    drawButtonHint(font, 40, 526, "X", TR(TXT_BTN_REMOVE));
    drawButtonHint(font, 230, 526, "□", "Add Entry");
    drawButtonHint(font, 440, 526, "△", TR(TXT_BTN_CLEAR));
    drawButtonHint(font, 760, 526, "O", TR(TXT_BTN_BACK));
}

void drawAboutScreen(vita2d_pgf *font) {
    drawBackground();
    drawHeader(font, TR(TXT_ABOUT_TITLE), TR(TXT_ABOUT_SUB));

    drawCard(40, 108, 430, 368);
    vita2d_pgf_draw_text(font, 60, 140, g_theme.accent, 0.95f, "CONTROLS & SHORTCUTS");
    vita2d_draw_rectangle(60, 152, 390, 1, g_theme.border);

    const char *controls[] = {
        "D-Pad / Left Stick: Navigate lists",
        "L / R Triggers: Fast Page Up / Down",
        "Touch Screen: Tap row to select / scroll",
        "Dashboard [□]: Switch Cleaning Mode",
        "Dashboard [L]: Open Tools & Diagnostics",
        "Dashboard [SEL]: Per-App Cleaning",
        "Preview [□]: Filter extension (tmp/log/dmp)",
        "Preview [SEL]: Search (IME) & Whitelist",
        "Orphan [SEL]: ZIP Backup for Savedata",
        "Cleaning [O]: Safe Emergency Stop"
    };
    for (int i = 0; i < 10; i++) {
        vita2d_pgf_draw_text(font, 60, 182 + i * 28, g_theme.text_secondary, 0.76f, controls[i]);
    }

    drawCard(490, 108, 430, 368);
    vita2d_pgf_draw_text(font, 510, 140, g_theme.accent, 0.95f, "SAFETY TIERS & CREDITS");
    vita2d_draw_rectangle(510, 152, 390, 1, g_theme.border);

    vita2d_pgf_draw_text(font, 510, 182, g_theme.success, 0.80f, "SAFE / REGENERABLE:");
    vita2d_pgf_draw_text(font, 510, 204, g_theme.text_secondary, 0.74f, "Caches, logs, crash dumps, temp files.");

    vita2d_pgf_draw_text(font, 510, 236, g_theme.accent, 0.80f, "ORPHANED:");
    vita2d_pgf_draw_text(font, 510, 258, g_theme.text_secondary, 0.74f, "Data left by uninstalled Title IDs.");

    vita2d_pgf_draw_text(font, 510, 290, g_theme.warning, 0.80f, "REVIEW:");
    vita2d_pgf_draw_text(font, 510, 312, g_theme.text_secondary, 0.74f, "Screenshots, VPK/PKG, uncertain dirs.");

    vita2d_pgf_draw_text(font, 510, 344, g_theme.danger, 0.80f, "PROTECTED:");
    vita2d_pgf_draw_text(font, 510, 366, g_theme.text_secondary, 0.74f, "os0/vs0/tai/savedata & Whitelist.");

    vita2d_draw_rectangle(510, 392, 390, 1, g_theme.border);
    vita2d_pgf_draw_text(font, 510, 422, g_theme.text_primary, 0.82f, "Creator: theheroGAC");
    vita2d_pgf_draw_text(font, 510, 448, g_theme.text_faint, 0.74f, "Built with VitaSDK & vita2d");

    drawFooterBar();
    drawButtonHint(font, 420, 526, "O", TR(TXT_BTN_BACK));
}

void startSizeCalc() {
    if (isBgBusy()) return;
    requestBgTask(BG_TASK_CALC_SIZE);
}

static void adjustScroll(int selected, int *scroll, int maxVisible) {
    if (selected < *scroll) *scroll = selected;
    if (selected >= *scroll + maxVisible) *scroll = selected - maxVisible + 1;
    if (*scroll < 0) *scroll = 0;
}

static void pageMove(int *selected, int *scroll, int count, int maxVisible, int delta) {
    if (count <= 0) return;
    *selected += delta;
    if (*selected < 0) *selected = 0;
    if (*selected >= count) *selected = count - 1;
    adjustScroll(*selected, scroll, maxVisible);
}

int main() {
    sceSysmoduleLoadModule(SCE_SYSMODULE_PGF);
    sceSysmoduleLoadModule(SCE_SYSMODULE_APPUTIL);
    sceSysmoduleLoadModule(SCE_SYSMODULE_IME);

    SceAppUtilInitParam initParam;
    SceAppUtilBootParam bootParam;
    memset(&initParam, 0, sizeof(initParam));
    memset(&bootParam, 0, sizeof(bootParam));
    sceAppUtilInit(&initParam, &bootParam);

    SceCommonDialogConfigParam dlgCfg;
    sceCommonDialogConfigParamInit(&dlgCfg);
    sceCommonDialogSetConfigParam(&dlgCfg);

    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
    memset(&g_touchPanel, 0, sizeof(g_touchPanel));
    sceTouchGetPanelInfo(SCE_TOUCH_PORT_FRONT, &g_touchPanel);

    vita2d_init();
    vita2d_system_pgf_config fontConfigs[2] = {
        {SCE_FONT_LANGUAGE_LATIN, isLatinChar},
        {SCE_FONT_LANGUAGE_JAPANESE, NULL}
    };
    vita2d_pgf *font = vita2d_load_system_pgf(2, fontConfigs);
    if (!font) font = vita2d_load_default_pgf();
    g_draw_font = font;
    if (!font) {
        sceAppUtilShutdown();
        vita2d_fini();
        return -1;
    }

    SceCtrlData pad;
    SceTouchData touch;
    unsigned int prevButtons = 0;
    int prevAnalogDir = 0;
    int analogCooldown = 0;
    int prevTouchDown = 0;
    int touchStartY = 0;
    int touchLastX = 0;
    int touchLastY = 0;
    int touchDragged = 0;

    initEmergencyStop();
    loadSettings();
    detectSystemLanguage();
    applyThemeColors(&g_theme);
    startBgWorker();

    char spaceValueText[32];
    strcpy(spaceValueText, TR(TXT_SCANNING));
    int spaceKnown = 0;

    MenuOptions menu;
    initMenuOptions(&menu, PROFILE_SELECTIVE);

    PreviewState preview;
    memset(&preview, 0, sizeof(preview));
    preview.sortMode = SORT_BY_NAME;

    AppListState appState;
    memset(&appState, 0, sizeof(appState));
    appState.sortMode = SORT_BY_SIZE;

    int previewScanning = 0;
    int appScanning = 0;
    int cleaningInProgress = 0;
    int appCleaningInProgress = 0;
    int sizeCalcQueued = 0;

    int showMenu = 0;
    int showPreview = 0;
    int showPreviewActions = 0;
    int previewActionIdx = 0;
    int showDeleteConfirmation = 0;
    int showCleanAllConfirmation = 0;
    int cleanSubsetTarget = 0;
    int showProfileSelect = 1;
    CleaningProfile selectedProfile = PROFILE_COMPLETE;
    int running = 1;
    int currentFrame = 0;

    int showToolsMenu = 0;
    int toolsMenuSelected = 0;
    int toolsMenuScroll = 0;

    int showAnalyzer = 0;
    int analyzerScanning = 0;
    int analyzerSelected = 0;
    int analyzerScroll = 0;
    int showAnalyzerDeleteConfirm = 0;
    char analyzerPathStack[ANALYZER_MAX_DEPTH][MAX_PATH_LENGTH];

    int showSmartClean = 0;
    int smartCleanPhase = 0;

    int showOrphanScan = 0;
    int orphanScanPhase = 0;
    int orphanScrollOffset = 0;
    int orphanSelectedItem = 0;
    int showOrphanDeleteConfirm = 0;
    int orphanDeleting = 0;
    int showOrphanActions = 0;
    int orphanActionIdx = 0;
    int orphanBackupBusy = 0;

    int showLargeFiles = 0;
    int largeScanning = 0;
    int largeDeleting = 0;
    int largeScrollOffset = 0;
    int largeSelectedItem = 0;
    int showLargeDeleteConfirm = 0;

    int showTaiAudit = 0;
    int taiScanning = 0;
    int taiScrollOffset = 0;
    int taiSelectedItem = 0;
    int showTaiDeleteConfirm = 0;

    int showHistory = 0;
    int historyScroll = 0;

    int showWhitelist = 0;
    int whitelistScroll = 0;
    int whitelistSelected = 0;

    int showAbout = 0;

    ResultKind resultKind = RESULT_NONE;
    int resultCleanupNum = 0;
    int resultFilesDeleted = 0;
    char resultSpaceText[32] = "";
    char resultTitleId[96] = "";

    startSizeCalc();

    while (running) {
        sceCtrlPeekBufferPositive(0, &pad, 1);
        memset(&touch, 0, sizeof(touch));
        sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1);
        currentFrame++;
        g_animFrame++;

        unsigned int pressed = pad.buttons & ~prevButtons;
        prevButtons = pad.buttons;

        int analogDir = 0;
        if (pad.ly < 60) analogDir = -1;
        else if (pad.ly > 196) analogDir = 1;
        int analogUp = 0, analogDown = 0;
        if (analogDir != 0) {
            if (analogDir != prevAnalogDir || analogCooldown <= 0) {
                if (analogDir < 0) analogUp = 1;
                else analogDown = 1;
                analogCooldown = (analogDir != prevAnalogDir) ? 14 : 5;
            } else {
                analogCooldown--;
            }
        } else {
            analogCooldown = 0;
        }
        prevAnalogDir = analogDir;

        int tapX = -1, tapY = -1, swipeSteps = 0;
        if (touch.reportNum > 0) {
            int maxX = (g_touchPanel.maxDispX > 0) ? g_touchPanel.maxDispX : 1920;
            int maxY = (g_touchPanel.maxDispY > 0) ? g_touchPanel.maxDispY : 1088;
            int tx = (touch.report[0].x * 960) / maxX;
            int ty = (touch.report[0].y * 544) / maxY;
            if (!prevTouchDown) {
                prevTouchDown = 1;
                touchStartY = ty;
                touchLastX = tx;
                touchLastY = ty;
                touchDragged = 0;
            } else {
                int dy = ty - touchLastY;
                if (abs(ty - touchStartY) > 16) touchDragged = 1;
                touchLastX = tx;
                if (dy >= 24) {
                    swipeSteps = -1;
                    touchLastY = ty;
                } else if (dy <= -24) {
                    swipeSteps = 1;
                    touchLastY = ty;
                }
            }
        } else {
            if (prevTouchDown && !touchDragged) {
                tapX = touchLastX;
                tapY = touchLastY;
            }
            prevTouchDown = 0;
            touchDragged = 0;
        }

        int navUp = (pressed & SCE_CTRL_UP) || analogUp || (swipeSteps < 0);
        int navDown = (pressed & SCE_CTRL_DOWN) || analogDown || (swipeSteps > 0);

        if (appCleaningInProgress && g_bgTaskDone) {
            appCleaningInProgress = 0;
            endOperation();

            resultCleanupNum = loadCleanupCounter() + 1;
            saveCleanupCounter(resultCleanupNum);
            resultFilesDeleted = getDeletedFilesCount();
            formatSize(g_bgSpaceFreed, resultSpaceText, sizeof(resultSpaceText));
            if (appState.appList && appState.selectedRow < appState.visibleCount) {
                AppInfo *a = &appState.appList->apps[appState.visibleIndices[appState.selectedRow]];
                snprintf(resultTitleId, sizeof(resultTitleId), "%s%s%s",
                         a->titleId, a->appName[0] ? " - " : "", a->appName);
            }
            resultKind = RESULT_APP_CLEANED;

            startAppScan(&appState);
            appScanning = 1;
            sizeCalcQueued = 1;
            spaceKnown = 0;
            strcpy(spaceValueText, TR(TXT_SCANNING));
        }

        if (showAnalyzer && analyzerScanning && g_bgTaskDone) {
            analyzerScanning = 0;
            analyzerSelected = 0;
            analyzerScroll = 0;
        }

        if (showSmartClean && g_bgTaskDone) {
            if (smartCleanPhase == 0) {
                smartCleanPhase = 1;
            } else if (smartCleanPhase == 2) {
                smartCleanPhase = 3;
            }
        }

        if (showOrphanScan && orphanScanPhase == 0 && g_bgTaskDone && !orphanDeleting && !orphanBackupBusy) {
            orphanScanPhase = 1;
            orphanSelectedItem = 0;
            orphanScrollOffset = 0;
        }

        if (orphanBackupBusy && g_bgTaskDone) {
            orphanBackupBusy = 0;
            if (g_bgLastActionOk) {
                showNotificationKind("Backup ZIP OK", g_bgLastActionMsg, 1);
            } else {
                showNotificationKind("Backup Failed", "Could not create ZIP archive", 3);
            }
            if (orphanSelectedItem >= g_orphanItemCount && g_orphanItemCount > 0) {
                orphanSelectedItem = g_orphanItemCount - 1;
            }
        }

        if (orphanDeleting && g_bgTaskDone) {
            orphanDeleting = 0;
            showOrphanScan = 0;
            showSmartClean = 1;
            smartCleanPhase = 3;
            if (orphanSelectedItem >= g_orphanItemCount) orphanSelectedItem = 0;
        }

        if (showLargeFiles && largeScanning && g_bgTaskDone) {
            largeScanning = 0;
            largeSelectedItem = 0;
            largeScrollOffset = 0;
        }

        if (largeDeleting && g_bgTaskDone) {
            largeDeleting = 0;
            showLargeFiles = 0;
            showSmartClean = 1;
            smartCleanPhase = 3;
        }

        if (showTaiAudit && taiScanning && g_bgTaskDone) {
            taiScanning = 0;
            taiSelectedItem = 0;
            taiScrollOffset = 0;
        }

        if (cleaningInProgress && g_bgTaskDone) {
            cleaningInProgress = 0;
            g_progressCallback = NULL;

            int cleaningInterrupted = isEmergencyStopRequested();
            endOperation();

            resultFilesDeleted = getDeletedFilesCount();
            unsigned long long freedDisp = g_bgSpaceFreed;
            if (freedDisp == 0 && preview.fileList) freedDisp = preview.totalVisibleSize;
            formatSize(freedDisp, resultSpaceText, sizeof(resultSpaceText));

            if (cleaningInterrupted) {
                cleanupAfterEmergencyStop();
                resultKind = RESULT_INTERRUPTED;
            } else {
                resultCleanupNum = loadCleanupCounter() + 1;
                saveCleanupCounter(resultCleanupNum);
                showNotification("PSV Cleaner", TR(TXT_CLEANING_DONE));
                resultKind = RESULT_CLEAN_DONE;
            }

            if (preview.fileList) {
                freeFileList(preview.fileList);
                preview.fileList = NULL;
                preview.visibleCount = 0;
            }

            spaceKnown = 0;
            strcpy(spaceValueText, TR(TXT_SCANNING));
            startSizeCalc();
        }

        if (g_bgTaskDone) {
            if (previewScanning) {
                previewScanning = 0;
                rebuildPreviewView(&preview);
                g_bgPreviewList = NULL;
            }
            if (appScanning) {
                appScanning = 0;
                rebuildAppView(&appState);
                g_bgAppList = NULL;
            }
            if (sizeCalcQueued) {
                sizeCalcQueued = 0;
                requestBgTask(BG_TASK_CALC_SIZE);
            } else {
                formatSize(g_cachedSpaceSize, spaceValueText, sizeof(spaceValueText));
                spaceKnown = 1;
            }
        }

        if (g_bgTaskDone && !showSmartClean && !showAnalyzer && !showOrphanScan && !showLargeFiles && !showTaiAudit && currentFrame % 360 == 0) {
            requestBgTask(BG_TASK_CALC_SIZE);
        }

        if ((previewScanning || appScanning || cleaningInProgress || appCleaningInProgress || largeScanning || orphanBackupBusy) && currentFrame % 120 == 0) {
            sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
        }

        vita2d_start_drawing();
        vita2d_clear_screen();

        if (cleaningInProgress || largeDeleting || (showOrphanScan && orphanDeleting) || (showSmartClean && smartCleanPhase == 2)) {
            drawProgressBar(font, cleaningInProgress ? getLastProgressPercent() : getScanProgress());
        } else if (appCleaningInProgress) {
            drawAppCleaningScreen(font);
        } else if (resultKind != RESULT_NONE) {
            drawResultScreen(font, resultKind, resultCleanupNum, resultTitleId, resultSpaceText, resultFilesDeleted);
        } else if (orphanBackupBusy) {
            drawBackground();
            drawHeader(font, TR(TXT_ORPHAN_TITLE), "ZIP Save Backup");
            drawSpinner(font, 480, 260, "Creating ZIP backup...");
            drawFooterBar();
        } else if (showCleanAllConfirmation) {
            drawPreviewScreen(font, &preview, previewScanning);
            int cnt = 0;
            unsigned long long sz = 0;
            if (cleanSubsetTarget == 1) {
                cnt = countPreviewSelected(&preview);
                sz = sumPreviewSelectedSize(&preview);
            } else {
                cnt = preview.visibleCount;
                sz = preview.totalVisibleSize;
            }
            drawCleanAllConfirmation(font, cnt, sz);
        } else if (showDeleteConfirmation) {
            drawPreviewScreen(font, &preview, previewScanning);
            drawDeleteConfirmation(font, &preview);
        } else if (showPreviewActions) {
            drawPreviewScreen(font, &preview, previewScanning);
            drawPreviewActionsModal(font, previewActionIdx);
        } else if (appState.showAppList) {
            drawAppListScreen(font, &appState, appScanning);
        } else if (showOrphanDeleteConfirm) {
            drawOrphanScanScreen(font, orphanScanPhase, orphanScrollOffset, orphanSelectedItem);
            drawOrphanDeleteConfirmation(font);
        } else if (showOrphanActions && g_orphanItemCount > 0) {
            drawOrphanScanScreen(font, orphanScanPhase, orphanScrollOffset, orphanSelectedItem);
            SafetyItem *it = &g_orphanItems[orphanSelectedItem];
            int isSave = (it->category == CAT_PROTECTED && strncasecmp(it->path, "ux0:user/00/savedata/", 21) == 0);
            drawOrphanActionsModal(font, it, orphanActionIdx, isSave);
        } else if (showOrphanScan) {
            drawOrphanScanScreen(font, orphanScanPhase, orphanScrollOffset, orphanSelectedItem);
        } else if (showSmartClean) {
            if (smartCleanPhase == 0) {
                drawBackground(); drawHeader(font, TR(TXT_SMART_TITLE), ""); drawSpinner(font, 480, 260, TR(TXT_SMART_SCAN_WAIT)); drawFooterBar();
            } else if (smartCleanPhase == 1) {
                drawSmartPreview(font);
            } else if (smartCleanPhase == 3) {
                drawSmartReport(font);
            }
        } else if (showAnalyzerDeleteConfirm) {
            drawAnalyzerScreen(font, analyzerScanning, analyzerSelected, analyzerScroll);
            vita2d_draw_rectangle(0, 0, 960, 544, RGBA(0, 0, 0, 170));
            drawCard(220, 180, 520, 190);
            vita2d_draw_rectangle(221, 181, 518, 3, g_theme.danger);
            drawCenteredText(font, 480, 220, g_theme.text_primary, 1.25f, TR(TXT_DELETE_FILE));
            char nm[52];
            truncateMiddleOrTail(g_analyzerDirEntries[analyzerSelected].name, nm, 48);
            drawCenteredText(font, 480, 265, g_theme.warning, 0.9f, nm);
            drawButtonHint(font, 300, 336, "X", TR(TXT_BTN_CONFIRM));
            drawButtonHint(font, 540, 336, "O", TR(TXT_BTN_CANCEL));
        } else if (showAnalyzer) {
            drawAnalyzerScreen(font, analyzerScanning, analyzerSelected, analyzerScroll);
        } else if (showLargeDeleteConfirm) {
            drawLargeFilesScreen(font, largeScanning, largeScrollOffset, largeSelectedItem);
            int selCount = 0;
            unsigned long long selBytes = 0;
            for (int i = 0; i < g_largeItemCount; i++) {
                if (g_largeItems[i].selected) { selCount++; selBytes += g_largeItems[i].size; }
            }
            drawCleanAllConfirmation(font, selCount, selBytes);
        } else if (showLargeFiles) {
            drawLargeFilesScreen(font, largeScanning, largeScrollOffset, largeSelectedItem);
        } else if (showTaiDeleteConfirm) {
            drawTaiAuditScreen(font, taiScanning, taiScrollOffset, taiSelectedItem);
            int selCount = 0;
            unsigned long long selBytes = 0;
            for (int i = 0; i < g_taiAudit.entryCount; i++) {
                if (g_taiAudit.entries[i].userSelected) { selCount++; selBytes += g_taiAudit.entries[i].size; }
            }
            drawCleanAllConfirmation(font, selCount, selBytes);
        } else if (showTaiAudit) {
            drawTaiAuditScreen(font, taiScanning, taiScrollOffset, taiSelectedItem);
        } else if (showHistory) {
            drawHistoryScreen(font, historyScroll);
        } else if (showWhitelist) {
            drawWhitelistScreen(font, whitelistScroll, whitelistSelected);
        } else if (showAbout) {
            drawAboutScreen(font);
        } else if (showToolsMenu) {
            drawToolsMenu(font, toolsMenuSelected, toolsMenuScroll);
        } else if (showPreview) {
            drawPreviewScreen(font, &preview, previewScanning);
        } else if (showMenu) {
            drawOptionsMenu(font, &menu);
        } else if (showProfileSelect) {
            drawProfileSelect(font, selectedProfile);
        } else {
            drawMainScreen(font, spaceValueText, spaceKnown);
        }

        drawToastOverlay(font);
        vita2d_end_drawing();
        vita2d_swap_buffers();
        gpuMemoryCleanup();

        if (cleaningInProgress || appCleaningInProgress || (showSmartClean && smartCleanPhase == 2) ||
            (showOrphanScan && orphanDeleting) || largeDeleting) {
            if (pad.buttons & SCE_CTRL_CIRCLE) {
                requestEmergencyStop();
            }
        } else if (resultKind != RESULT_NONE) {
            if (pressed & (SCE_CTRL_CROSS | SCE_CTRL_CIRCLE)) {
                resultKind = RESULT_NONE;
            }
        } else if (showOrphanDeleteConfirm) {
            if (pressed & SCE_CTRL_CROSS) {
                showOrphanDeleteConfirm = 0;
                orphanDeleting = 1;
                resetDeletedFilesCount();
                resetScanProgress();
                requestBgTask(BG_TASK_ORPHAN_DELETE);
            } else if (pressed & SCE_CTRL_CIRCLE) {
                showOrphanDeleteConfirm = 0;
            }
        } else if (showOrphanActions && g_orphanItemCount > 0) {
            SafetyItem *it = &g_orphanItems[orphanSelectedItem];
            int isSave = (it->category == CAT_PROTECTED && strncasecmp(it->path, "ux0:user/00/savedata/", 21) == 0);
            int maxAct = isSave ? 3 : 1;
            if (navUp && orphanActionIdx > 0) orphanActionIdx--;
            if (navDown && orphanActionIdx < maxAct - 1) orphanActionIdx++;
            if (pressed & SCE_CTRL_CROSS) {
                showOrphanActions = 0;
                if (isSave && (orphanActionIdx == 0 || orphanActionIdx == 1)) {
                    safe_strncpy(g_bgTargetTitleId, it->titleId, sizeof(g_bgTargetTitleId));
                    safe_strncpy(g_bgTargetPath, it->path, sizeof(g_bgTargetPath));
                    orphanBackupBusy = 1;
                    resetDeletedFilesCount();
                    requestBgTask(orphanActionIdx == 1 ? BG_TASK_BACKUP_DELETE_SAVE : BG_TASK_BACKUP_SAVE);
                } else {
                    if (whitelistAdd(it->titleId[0] ? it->titleId : it->path)) {
                        showNotification("Whitelist", "Added to whitelist");
                    }
                }
            } else if (pressed & SCE_CTRL_CIRCLE) {
                showOrphanActions = 0;
            }
        } else if (showOrphanScan) {
            if (orphanScanPhase == 1) {
                if (navUp && orphanSelectedItem > 0) {
                    orphanSelectedItem--;
                    adjustScroll(orphanSelectedItem, &orphanScrollOffset, 14);
                }
                if (navDown && orphanSelectedItem < g_orphanItemCount - 1) {
                    orphanSelectedItem++;
                    adjustScroll(orphanSelectedItem, &orphanScrollOffset, 14);
                }
                if (pressed & SCE_CTRL_LTRIGGER) pageMove(&orphanSelectedItem, &orphanScrollOffset, g_orphanItemCount, 14, -14);
                if (pressed & SCE_CTRL_RTRIGGER) pageMove(&orphanSelectedItem, &orphanScrollOffset, g_orphanItemCount, 14, 14);
                if (tapY >= 132 && tapY < 482 && g_orphanItemCount > 0) {
                    int tapped = orphanScrollOffset + (tapY - 132) / 25;
                    if (tapped >= 0 && tapped < g_orphanItemCount) {
                        if (tapped == orphanSelectedItem && g_orphanItems[tapped].category != CAT_PROTECTED) {
                            g_orphanItems[tapped].user_selected = !g_orphanItems[tapped].user_selected;
                            safetyRecompute(&g_orphanItems[tapped]);
                        }
                        orphanSelectedItem = tapped;
                    }
                }
                if (pressed & SCE_CTRL_CROSS) {
                    if (g_orphanItemCount > 0) {
                        SafetyItem *item = &g_orphanItems[orphanSelectedItem];
                        if (item->category != CAT_PROTECTED) {
                            item->user_selected = !item->user_selected;
                            safetyRecompute(item);
                        }
                    }
                }
                if (pressed & SCE_CTRL_SQUARE) {
                    for (int i = 0; i < g_orphanItemCount; i++) {
                        if (g_orphanItems[i].category != CAT_PROTECTED) {
                            g_orphanItems[i].user_selected = 1;
                            safetyRecompute(&g_orphanItems[i]);
                        }
                    }
                }
                if (pressed & (SCE_CTRL_TRIANGLE | SCE_CTRL_START)) {
                    int anySel = 0;
                    for (int i = 0; i < g_orphanItemCount; i++) {
                        if (g_orphanItems[i].user_selected) anySel = 1;
                    }
                    if (anySel) showOrphanDeleteConfirm = 1;
                }
                if (pressed & SCE_CTRL_SELECT) {
                    if (g_orphanItemCount > 0) {
                        showOrphanActions = 1;
                        orphanActionIdx = 0;
                    }
                }
            }
            if (pressed & SCE_CTRL_CIRCLE) {
                showOrphanScan = 0;
                showToolsMenu = 0;
            }
        } else if (showSmartClean) {
            if (smartCleanPhase == 1) {
                if (pressed & SCE_CTRL_CROSS) {
                    smartCleanPhase = 2;
                    resetDeletedFilesCount();
                    resetScanProgress();
                    requestBgTask(BG_TASK_SMART_CLEAN);
                } else if (pressed & SCE_CTRL_CIRCLE) {
                    showSmartClean = 0;
                    showToolsMenu = 0;
                }
            } else if (smartCleanPhase == 3) {
                if (pressed & (SCE_CTRL_CROSS | SCE_CTRL_CIRCLE)) {
                    showSmartClean = 0;
                    showToolsMenu = 0;
                    spaceKnown = 0;
                    strcpy(spaceValueText, TR(TXT_SCANNING));
                    startSizeCalc();
                }
            }
        } else if (showAnalyzerDeleteConfirm) {
            if (pressed & SCE_CTRL_CROSS) {
                showAnalyzerDeleteConfirm = 0;
                if (analyzerSelected >= 0 && analyzerSelected < g_analyzerDirEntryCount) {
                    if (analyzerDeleteEntry(g_analyzerDirEntries[analyzerSelected].fullPath)) {
                        showNotification("Storage Analyzer", "Item removed");
                        safe_strncpy(g_bgTargetPath, g_analyzerBrowsePath, sizeof(g_bgTargetPath));
                        analyzerScanning = 1;
                        requestBgTask(BG_TASK_ANALYZE_DIR);
                    } else {
                        showNotificationKind("Protected", "Cannot remove protected item", 3);
                    }
                }
            } else if (pressed & SCE_CTRL_CIRCLE) {
                showAnalyzerDeleteConfirm = 0;
            }
        } else if (showAnalyzer) {
            if (!analyzerScanning) {
                if (navUp && analyzerSelected > 0) {
                    analyzerSelected--;
                    adjustScroll(analyzerSelected, &analyzerScroll, 8);
                }
                if (navDown && analyzerSelected < g_analyzerDirEntryCount - 1) {
                    analyzerSelected++;
                    adjustScroll(analyzerSelected, &analyzerScroll, 8);
                }
                if (g_analyzerPathDepth == 0) {
                    if (pressed & SCE_CTRL_LTRIGGER) {
                        if (g_analyzerVolumeCount > 0) {
                            g_analyzerActiveVolume = (g_analyzerActiveVolume - 1 + g_analyzerVolumeCount) % g_analyzerVolumeCount;
                            analyzerScanning = 1;
                            requestBgTask(BG_TASK_ANALYZE_VOLUME);
                        }
                    }
                    if (pressed & SCE_CTRL_RTRIGGER) {
                        if (g_analyzerVolumeCount > 0) {
                            g_analyzerActiveVolume = (g_analyzerActiveVolume + 1) % g_analyzerVolumeCount;
                            analyzerScanning = 1;
                            requestBgTask(BG_TASK_ANALYZE_VOLUME);
                        }
                    }
                }
                if (pressed & SCE_CTRL_CROSS) {
                    if (g_analyzerDirEntryCount > 0 && analyzerSelected < g_analyzerDirEntryCount &&
                        g_analyzerDirEntries[analyzerSelected].isDir && g_analyzerPathDepth + 1 < ANALYZER_MAX_DEPTH) {
                        safe_strncpy(analyzerPathStack[g_analyzerPathDepth], g_analyzerBrowsePath, MAX_PATH_LENGTH);
                        g_analyzerPathDepth++;
                        safe_strncpy(g_bgTargetPath, g_analyzerDirEntries[analyzerSelected].fullPath, sizeof(g_bgTargetPath));
                        analyzerScanning = 1;
                        requestBgTask(BG_TASK_ANALYZE_DIR);
                    }
                }
                if (pressed & SCE_CTRL_SQUARE) {
                    if (g_analyzerPathDepth > 0) {
                        safe_strncpy(g_bgTargetPath, g_analyzerBrowsePath, sizeof(g_bgTargetPath));
                        analyzerScanning = 1;
                        requestBgTask(BG_TASK_ANALYZE_DIR);
                    } else if (g_analyzerVolumeCount > 0) {
                        analyzerScanning = 1;
                        requestBgTask(BG_TASK_ANALYZE_VOLUME);
                    }
                }
                if (pressed & SCE_CTRL_TRIANGLE) {
                    if (g_analyzerDirEntryCount > 0 && analyzerSelected < g_analyzerDirEntryCount) {
                        if (g_analyzerDirEntries[analyzerSelected].deletable) {
                            showAnalyzerDeleteConfirm = 1;
                        } else {
                            showNotificationKind("Protected", "System/Whitelisted directory is locked", 2);
                        }
                    }
                }
            }
            if (pressed & SCE_CTRL_CIRCLE) {
                if (g_analyzerPathDepth > 0 && !analyzerScanning) {
                    g_analyzerPathDepth--;
                    safe_strncpy(g_bgTargetPath, analyzerPathStack[g_analyzerPathDepth], sizeof(g_bgTargetPath));
                    analyzerScanning = 1;
                    requestBgTask(BG_TASK_ANALYZE_DIR);
                } else {
                    showAnalyzer = 0;
                    showToolsMenu = 0;
                }
            }
        } else if (showLargeDeleteConfirm) {
            if (pressed & SCE_CTRL_CROSS) {
                showLargeDeleteConfirm = 0;
                largeDeleting = 1;
                resetDeletedFilesCount();
                resetScanProgress();
                requestBgTask(BG_TASK_LARGE_DELETE);
            } else if (pressed & SCE_CTRL_CIRCLE) {
                showLargeDeleteConfirm = 0;
            }
        } else if (showLargeFiles) {
            if (!largeScanning) {
                if (navUp && largeSelectedItem > 0) {
                    largeSelectedItem--;
                    adjustScroll(largeSelectedItem, &largeScrollOffset, 14);
                }
                if (navDown && largeSelectedItem < g_largeItemCount - 1) {
                    largeSelectedItem++;
                    adjustScroll(largeSelectedItem, &largeScrollOffset, 14);
                }
                if (pressed & SCE_CTRL_LTRIGGER) pageMove(&largeSelectedItem, &largeScrollOffset, g_largeItemCount, 14, -14);
                if (pressed & SCE_CTRL_RTRIGGER) pageMove(&largeSelectedItem, &largeScrollOffset, g_largeItemCount, 14, 14);
                if (pressed & SCE_CTRL_CROSS) {
                    if (g_largeItemCount > 0) g_largeItems[largeSelectedItem].selected = !g_largeItems[largeSelectedItem].selected;
                }
                if (pressed & SCE_CTRL_SQUARE) {
                    if (g_largeMinSizeMB == 10) g_largeMinSizeMB = 50;
                    else if (g_largeMinSizeMB == 50) g_largeMinSizeMB = 100;
                    else if (g_largeMinSizeMB == 100) g_largeMinSizeMB = 250;
                    else g_largeMinSizeMB = 10;
                    g_bgLargeMinMB = g_largeMinSizeMB;
                    largeScanning = 1;
                    requestBgTask(BG_TASK_LARGE_SCAN);
                }
                if (pressed & (SCE_CTRL_TRIANGLE | SCE_CTRL_START)) {
                    int anySel = 0;
                    for (int i = 0; i < g_largeItemCount; i++) {
                        if (g_largeItems[i].selected) anySel = 1;
                    }
                    if (anySel) showLargeDeleteConfirm = 1;
                }
                if (pressed & SCE_CTRL_SELECT) {
                    if (g_largeItemCount > 0 && whitelistAdd(g_largeItems[largeSelectedItem].path)) {
                        showNotification("Whitelist", "File excluded permanently");
                        largeScanning = 1;
                        requestBgTask(BG_TASK_LARGE_SCAN);
                    }
                }
            }
            if (pressed & SCE_CTRL_CIRCLE) {
                showLargeFiles = 0;
                showToolsMenu = 0;
            }
        } else if (showTaiDeleteConfirm) {
            if (pressed & SCE_CTRL_CROSS) {
                showTaiDeleteConfirm = 0;
                int del = taiDeleteUnusedSelected();
                if (del > 0) {
                    showNotification("TaiHEN Auditor", "Unused plugins/backups deleted");
                }
                taiSelectedItem = 0;
                taiScrollOffset = 0;
            } else if (pressed & SCE_CTRL_CIRCLE) {
                showTaiDeleteConfirm = 0;
            }
        } else if (showTaiAudit) {
            if (!taiScanning) {
                if (navUp && taiSelectedItem > 0) {
                    taiSelectedItem--;
                    adjustScroll(taiSelectedItem, &taiScrollOffset, 12);
                }
                if (navDown && taiSelectedItem < g_taiAudit.entryCount - 1) {
                    taiSelectedItem++;
                    adjustScroll(taiSelectedItem, &taiScrollOffset, 12);
                }
                if (pressed & SCE_CTRL_CROSS) {
                    if (g_taiAudit.entryCount > 0) {
                        TaiAuditEntry *e = &g_taiAudit.entries[taiSelectedItem];
                        if (e->status == TAI_UNUSED || e->status == TAI_BACKUP) {
                            e->userSelected = !e->userSelected;
                        }
                    }
                }
                if (pressed & SCE_CTRL_SQUARE) {
                    char bPath[128];
                    if (taiBackupConfig(bPath, sizeof(bPath))) {
                        showNotification("Config Backup", bPath);
                        taiScanning = 1;
                        requestBgTask(BG_TASK_TAI_AUDIT);
                    } else {
                        showNotificationKind("Backup Error", "No config.txt found", 3);
                    }
                }
                if (pressed & SCE_CTRL_TRIANGLE) {
                    int anySel = 0;
                    for (int i = 0; i < g_taiAudit.entryCount; i++) {
                        if (g_taiAudit.entries[i].userSelected) anySel = 1;
                    }
                    if (anySel) showTaiDeleteConfirm = 1;
                }
                if (pressed & SCE_CTRL_RTRIGGER) {
                    taiScanning = 1;
                    requestBgTask(BG_TASK_TAI_AUDIT);
                }
            }
            if (pressed & SCE_CTRL_CIRCLE) {
                showTaiAudit = 0;
                showToolsMenu = 0;
            }
        } else if (showHistory) {
            int cnt = historyGetCount();
            if (navUp && historyScroll > 0) historyScroll--;
            if (navDown && historyScroll + 13 < cnt) historyScroll++;
            if (pressed & SCE_CTRL_SQUARE) {
                historyClear();
                historyScroll = 0;
                showNotification("History", "Cleanup log cleared");
            }
            if (pressed & SCE_CTRL_CIRCLE) {
                showHistory = 0;
                showToolsMenu = 0;
            }
        } else if (showWhitelist) {
            int cnt = whitelistGetCount();
            if (navUp && whitelistSelected > 0) {
                whitelistSelected--;
                adjustScroll(whitelistSelected, &whitelistScroll, 14);
            }
            if (navDown && whitelistSelected < cnt - 1) {
                whitelistSelected++;
                adjustScroll(whitelistSelected, &whitelistScroll, 14);
            }
            if (pressed & SCE_CTRL_CROSS) {
                if (cnt > 0 && whitelistRemove(whitelistSelected)) {
                    cnt = whitelistGetCount();
                    if (whitelistSelected >= cnt && cnt > 0) whitelistSelected = cnt - 1;
                    showNotification("Whitelist", "Entry removed");
                }
            }
            if (pressed & SCE_CTRL_SQUARE) {
                char entryBuf[128] = "";
                if (openImeKeyboard(font, "Add Path or Title ID", "ux0:data/", entryBuf, sizeof(entryBuf))) {
                    if (whitelistAdd(entryBuf)) showNotification("Whitelist", entryBuf);
                }
            }
            if (pressed & SCE_CTRL_TRIANGLE) {
                whitelistClear();
                whitelistSelected = 0;
                whitelistScroll = 0;
                showNotification("Whitelist", "Whitelist cleared");
            }
            if (pressed & SCE_CTRL_CIRCLE) {
                showWhitelist = 0;
                showToolsMenu = 0;
            }
        } else if (showAbout) {
            if (pressed & (SCE_CTRL_CIRCLE | SCE_CTRL_CROSS)) {
                showAbout = 0;
                showToolsMenu = 0;
            }
        } else if (showToolsMenu) {
            if (navUp) {
                toolsMenuSelected = (toolsMenuSelected - 1 + TOOLS_MENU_COUNT) % TOOLS_MENU_COUNT;
                adjustScroll(toolsMenuSelected, &toolsMenuScroll, 5);
            }
            if (navDown) {
                toolsMenuSelected = (toolsMenuSelected + 1) % TOOLS_MENU_COUNT;
                adjustScroll(toolsMenuSelected, &toolsMenuScroll, 5);
            }
            if (tapY >= 108 && tapY < 488 && tapX >= 110 && tapX <= 850) {
                int idx = toolsMenuScroll + (tapY - 108) / 76;
                if (idx >= 0 && idx < TOOLS_MENU_COUNT) {
                    if (idx == toolsMenuSelected) pressed |= SCE_CTRL_CROSS;
                    else toolsMenuSelected = idx;
                }
            }
            if (pressed & SCE_CTRL_CROSS) {
                showToolsMenu = 0;
                if (toolsMenuSelected == 0) {
                    showAnalyzer = 1;
                    g_analyzerPathDepth = 0;
                    if (g_analyzerVolumeCount > 0 && g_analyzerActiveVolume >= 0 && g_analyzerDirEntryCount > 0) {
                        analyzerScanning = 0;
                    } else {
                        g_analyzerActiveVolume = 0;
                        analyzerScanning = 1;
                        requestBgTask(BG_TASK_ANALYZE_VOLUME);
                    }
                } else if (toolsMenuSelected == 1) {
                    showSmartClean = 1;
                    smartCleanPhase = 0;
                    requestBgTask(BG_TASK_SMART_SCAN);
                } else if (toolsMenuSelected == 2) {
                    showOrphanScan = 1;
                    orphanScanPhase = 0;
                    orphanSelectedItem = 0;
                    orphanScrollOffset = 0;
                    requestBgTask(BG_TASK_ORPHAN_SCAN);
                } else if (toolsMenuSelected == 3) {
                    showLargeFiles = 1;
                    largeScanning = 1;
                    g_bgLargeMinMB = g_largeMinSizeMB;
                    requestBgTask(BG_TASK_LARGE_SCAN);
                } else if (toolsMenuSelected == 4) {
                    showTaiAudit = 1;
                    taiScanning = 1;
                    requestBgTask(BG_TASK_TAI_AUDIT);
                } else if (toolsMenuSelected == 5) {
                    showHistory = 1;
                    historyScroll = 0;
                } else if (toolsMenuSelected == 6) {
                    showWhitelist = 1;
                    whitelistSelected = 0;
                    whitelistScroll = 0;
                } else if (toolsMenuSelected == 7) {
                    showAbout = 1;
                }
            }
            if (pressed & SCE_CTRL_CIRCLE) {
                showToolsMenu = 0;
            }
        } else if (showProfileSelect) {
            if (navUp) {
                int p = (int)selectedProfile - 1;
                if (p < 0) p = 5;
                selectedProfile = (CleaningProfile)p;
            }
            if (navDown) {
                int p = (int)selectedProfile + 1;
                if (p > 5) p = 0;
                selectedProfile = (CleaningProfile)p;
            }
            if (tapY >= 110 && tapY < 482 && tapX >= 110 && tapX <= 850) {
                int p = (tapY - 110) / 62;
                if (p >= 0 && p < 6) {
                    if (p == (int)selectedProfile) pressed |= SCE_CTRL_CROSS;
                    else selectedProfile = (CleaningProfile)p;
                }
            }
            if (pressed & SCE_CTRL_CROSS) {
                showProfileSelect = 0;
                if (selectedProfile == PROFILE_QUICK || selectedProfile == PROFILE_COMPLETE) {
                    safe_strncpy(g_bgActiveModeLabel, selectedProfile == PROFILE_QUICK ? "Quick Clean" : "Complete Clean", sizeof(g_bgActiveModeLabel));
                    initMenuOptions(&menu, selectedProfile);
                    showPreview = 1;
                    startPreviewScan(&preview);
                    previewScanning = 1;
                } else if (selectedProfile == PROFILE_SELECTIVE) {
                    safe_strncpy(g_bgActiveModeLabel, "Selective Clean", sizeof(g_bgActiveModeLabel));
                    initMenuOptions(&menu, selectedProfile);
                    showMenu = 1;
                } else if (selectedProfile == PROFILE_SMART) {
                    showSmartClean = 1;
                    smartCleanPhase = 0;
                    requestBgTask(BG_TASK_SMART_SCAN);
                } else if (selectedProfile == PROFILE_ORPHAN) {
                    showOrphanScan = 1;
                    orphanScanPhase = 0;
                    orphanSelectedItem = 0;
                    orphanScrollOffset = 0;
                    requestBgTask(BG_TASK_ORPHAN_SCAN);
                } else if (selectedProfile == PROFILE_ANALYZER) {
                    showAnalyzer = 1;
                    g_analyzerPathDepth = 0;
                    if (g_analyzerVolumeCount > 0 && g_analyzerActiveVolume >= 0 && g_analyzerDirEntryCount > 0) {
                        analyzerScanning = 0;
                    } else {
                        g_analyzerActiveVolume = 0;
                        analyzerScanning = 1;
                        requestBgTask(BG_TASK_ANALYZE_VOLUME);
                    }
                }
            }
            if (pressed & SCE_CTRL_CIRCLE) {
                showProfileSelect = 0;
            }
        } else if (showCleanAllConfirmation) {
            if (pressed & SCE_CTRL_CROSS) {
                if (preview.fileList && preview.fileList->count > 0) {
                    showCleanAllConfirmation = 0;
                    showPreview = 0;
                    cleaningInProgress = 1;

                    waitBgIdle();
                    sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
                    resetScanProgress();
                    g_progressCallback = NULL;
                    startOperation();

                    if (cleanSubsetTarget == 1) {
                        g_bgPreviewList = preview.fileList;
                        g_bgCleanSubsetMode = 1;
                        requestBgTask(BG_TASK_CLEAN_PREVIEW_SUBSET);
                    } else if (cleanSubsetTarget == 2) {
                        g_bgPreviewList = preview.fileList;
                        g_bgCleanSubsetMode = 2;
                        safe_strncpy(g_bgFileFilter, preview.fileFilter, MAX_FILE_FILTER_LENGTH);
                        safe_strncpy(g_bgSearchQuery, preview.searchQuery, sizeof(g_bgSearchQuery));
                        requestBgTask(BG_TASK_CLEAN_PREVIEW_SUBSET);
                    } else {
                        requestBgTask(BG_TASK_CLEAN);
                    }
                }
            }
            if (pressed & SCE_CTRL_CIRCLE) {
                showCleanAllConfirmation = 0;
            }
        } else if (showDeleteConfirmation) {
            if (pressed & SCE_CTRL_CROSS) {
                if (preview.fileList && preview.selectedRow >= 0 && preview.selectedRow < preview.visibleCount) {
                    int idx = preview.visibleIndices[preview.selectedRow];
                    if (deleteSingleFileFromList(preview.fileList, idx)) {
                        rebuildPreviewView(&preview);
                        showNotification("Preview", "File deleted");
                    }
                }
                showDeleteConfirmation = 0;
            }
            if (pressed & SCE_CTRL_CIRCLE) {
                showDeleteConfirmation = 0;
            }
        } else if (showPreviewActions) {
            if (navUp && previewActionIdx > 0) previewActionIdx--;
            if (navDown && previewActionIdx < 5) previewActionIdx++;
            if (pressed & SCE_CTRL_CROSS) {
                showPreviewActions = 0;
                if (previewActionIdx == 0) {
                    if (preview.visibleCount > 0) showDeleteConfirmation = 1;
                } else if (previewActionIdx == 1) {
                    char q[64] = "";
                    safe_strncpy(q, preview.searchQuery, sizeof(q));
                    if (openImeKeyboard(font, "Search in Preview", q, q, sizeof(q))) {
                        safe_strncpy(preview.searchQuery, q, sizeof(preview.searchQuery));
                        safe_strncpy(g_bgSearchQuery, preview.searchQuery, sizeof(g_bgSearchQuery));
                        rebuildPreviewView(&preview);
                    }
                } else if (previewActionIdx == 2) {
                    preview.searchQuery[0] = '\0';
                    g_bgSearchQuery[0] = '\0';
                    rebuildPreviewView(&preview);
                } else if (previewActionIdx == 3) {
                    for (int i = 0; i < preview.visibleCount; i++) {
                        preview.fileList->files[preview.visibleIndices[i]].selected = 1;
                    }
                } else if (previewActionIdx == 4) {
                    for (int i = 0; i < preview.visibleCount; i++) {
                        preview.fileList->files[preview.visibleIndices[i]].selected = 0;
                    }
                } else if (previewActionIdx == 5) {
                    if (preview.visibleCount > 0) {
                        int idx = preview.visibleIndices[preview.selectedRow];
                        if (whitelistAdd(preview.fileList->files[idx].path)) {
                            showNotification("Whitelist", "Path excluded");
                            startPreviewScan(&preview);
                            previewScanning = 1;
                        }
                    }
                }
            }
            if (pressed & SCE_CTRL_CIRCLE) {
                showPreviewActions = 0;
            }
        } else if (appState.showAppList) {
            if (!appScanning) {
                if (navUp && appState.selectedRow > 0) {
                    appState.selectedRow--;
                    adjustScroll(appState.selectedRow, &appState.scrollOffset, 15);
                }
                if (navDown && appState.selectedRow < appState.visibleCount - 1) {
                    appState.selectedRow++;
                    adjustScroll(appState.selectedRow, &appState.scrollOffset, 15);
                }
                if (pressed & SCE_CTRL_LTRIGGER) pageMove(&appState.selectedRow, &appState.scrollOffset, appState.visibleCount, 15, -15);
                if (pressed & SCE_CTRL_RTRIGGER) pageMove(&appState.selectedRow, &appState.scrollOffset, appState.visibleCount, 15, 15);
                if (pressed & SCE_CTRL_TRIANGLE) {
                    appState.sortMode = (appState.sortMode == SORT_BY_SIZE) ? SORT_BY_NAME : SORT_BY_SIZE;
                    sortAppList(appState.appList, appState.sortMode);
                    rebuildAppView(&appState);
                }
                if (pressed & SCE_CTRL_SQUARE) {
                    char q[64] = "";
                    safe_strncpy(q, appState.searchQuery, sizeof(q));
                    if (openImeKeyboard(font, "Search Apps (Name or Title ID)", q, q, sizeof(q))) {
                        safe_strncpy(appState.searchQuery, q, sizeof(appState.searchQuery));
                        rebuildAppView(&appState);
                    }
                }
                if (pressed & SCE_CTRL_SELECT) {
                    if (appState.appList && appState.visibleCount > 0) {
                        int idx = appState.visibleIndices[appState.selectedRow];
                        if (whitelistAdd(appState.appList->apps[idx].titleId)) {
                            showNotification("Whitelist", appState.appList->apps[idx].titleId);
                        }
                    }
                }
                if (pressed & SCE_CTRL_CROSS) {
                    if (appState.appList && appState.visibleCount > 0) {
                        int idx = appState.visibleIndices[appState.selectedRow];
                        appCleaningInProgress = 1;
                        waitBgIdle();
                        sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
                        resetDeletedFilesCount();
                        g_progressCallback = NULL;
                        startOperation();
                        safe_strncpy(g_bgCleanAppTitleId, appState.appList->apps[idx].titleId, sizeof(g_bgCleanAppTitleId));
                        requestBgTask(BG_TASK_CLEAN_APP);
                    }
                }
                if (pressed & SCE_CTRL_CIRCLE) {
                    appState.showAppList = 0;
                    if (appState.appList) {
                        freeAppList(appState.appList);
                        appState.appList = NULL;
                    }
                    appState.visibleCount = 0;
                    appState.searchQuery[0] = '\0';
                }
            }
        } else if (showPreview) {
            if (!previewScanning) {
                if (navUp && preview.selectedRow > 0) {
                    preview.selectedRow--;
                    adjustScroll(preview.selectedRow, &preview.scrollOffset, 16);
                }
                if (navDown && preview.selectedRow < preview.visibleCount - 1) {
                    preview.selectedRow++;
                    adjustScroll(preview.selectedRow, &preview.scrollOffset, 16);
                }
                if (pressed & SCE_CTRL_LTRIGGER) pageMove(&preview.selectedRow, &preview.scrollOffset, preview.visibleCount, 16, -16);
                if (pressed & SCE_CTRL_RTRIGGER) pageMove(&preview.selectedRow, &preview.scrollOffset, preview.visibleCount, 16, 16);
                if (tapY >= 136 && tapY < 472 && preview.visibleCount > 0) {
                    int tapped = preview.scrollOffset + (tapY - 136) / 21;
                    if (tapped >= 0 && tapped < preview.visibleCount) {
                        if (tapped == preview.selectedRow) {
                            int idx = preview.visibleIndices[tapped];
                            preview.fileList->files[idx].selected = !preview.fileList->files[idx].selected;
                        }
                        preview.selectedRow = tapped;
                    }
                }
                if (pressed & SCE_CTRL_CROSS) {
                    if (preview.visibleCount > 0) {
                        int idx = preview.visibleIndices[preview.selectedRow];
                        preview.fileList->files[idx].selected = !preview.fileList->files[idx].selected;
                    }
                }
                if (pressed & SCE_CTRL_TRIANGLE) {
                    preview.sortMode = (preview.sortMode == SORT_BY_NAME) ? SORT_BY_SIZE : SORT_BY_NAME;
                    if (preview.fileList) {
                        sortFileList(preview.fileList, preview.sortMode);
                        rebuildPreviewView(&preview);
                    }
                }
                if (pressed & SCE_CTRL_SQUARE) {
                    const char* filters[] = {"", "tmp", "log", "cache", "dmp", "vpk", "pkg"};
                    int cur = 0;
                    for (int i = 0; i < 7; i++) {
                        if (strcmp(preview.fileFilter, filters[i]) == 0) { cur = i; break; }
                    }
                    cur = (cur + 1) % 7;
                    strcpy(preview.fileFilter, filters[cur]);
                    safe_strncpy(g_bgFileFilter, preview.fileFilter, MAX_FILE_FILTER_LENGTH);
                    rebuildPreviewView(&preview);
                }
                if (pressed & SCE_CTRL_SELECT) {
                    showPreviewActions = 1;
                    previewActionIdx = 0;
                }
                if (pressed & SCE_CTRL_START) {
                    if (preview.fileList && preview.fileList->count > 0) {
                        int selCount = countPreviewSelected(&preview);
                        if (selCount > 0) {
                            cleanSubsetTarget = 1;
                            showCleanAllConfirmation = 1;
                        } else if ((preview.fileFilter[0] != '\0' || preview.searchQuery[0] != '\0') && preview.visibleCount > 0) {
                            cleanSubsetTarget = 2;
                            showCleanAllConfirmation = 1;
                        } else if (preview.visibleCount > 0) {
                            cleanSubsetTarget = 0;
                            showCleanAllConfirmation = 1;
                        }
                    }
                }
                if (pressed & SCE_CTRL_CIRCLE) {
                    showPreview = 0;
                    if (preview.fileList) {
                        freeFileList(preview.fileList);
                        preview.fileList = NULL;
                    }
                    preview.visibleCount = 0;
                }
            }
        } else if (showMenu) {
            if (navUp && menu.selected > 0) {
                menu.selected--;
                adjustScroll(menu.selected, &menu.scrollOffset, 7);
            }
            if (navDown && menu.selected < menu.total_options - 1) {
                menu.selected++;
                adjustScroll(menu.selected, &menu.scrollOffset, 7);
            }
            if (pressed & SCE_CTRL_LTRIGGER) pageMove(&menu.selected, &menu.scrollOffset, menu.total_options, 7, -7);
            if (pressed & SCE_CTRL_RTRIGGER) pageMove(&menu.selected, &menu.scrollOffset, menu.total_options, 7, 7);
            if (pressed & SCE_CTRL_CROSS) {
                if (menu.selected == 7) {
                    int targetState = !menu.enabled[0];
                    for (int i = 0; i < TOTAL_CATEGORIES; i++) {
                        if (!isSpecialOptionIndex(i) && i != 8 && i != 9 && i != 10 && i != 35) {
                            menu.enabled[i] = targetState;
                        }
                    }
                    syncMenuToGlobals(&menu);
                    saveSettings();
                } else if (menu.selected == 47) {
                    setTheme((AppTheme)(((int)currentTheme + 1) % MAX_THEMES));
                    applyThemeColors(&g_theme);
                    saveSettings();
                } else if (menu.selected == 48) {
                    int nextLang = languageMode + 1;
                    if (nextLang >= MAX_LANGUAGES) nextLang = LANGUAGE_AUTO;
                    setLanguageMode(nextLang);
                    saveSettings();
                } else if (menu.selected >= 0 && menu.selected < TOTAL_CATEGORIES) {
                    menu.enabled[menu.selected] = !menu.enabled[menu.selected];
                    syncMenuToGlobals(&menu);
                    saveSettings();
                }
            }
            if (pressed & SCE_CTRL_SELECT) {
                int allEnabled = 1;
                for (int i = 0; i < 7; i++) {
                    if (!menu.enabled[i]) { allEnabled = 0; break; }
                }
                for (int i = 0; i < 7; i++) {
                    menu.enabled[i] = !allEnabled;
                }
                syncMenuToGlobals(&menu);
                saveSettings();
            }
            if (pressed & SCE_CTRL_CIRCLE) {
                showPreview = 1;
                startPreviewScan(&preview);
                previewScanning = 1;
                showMenu = 0;
            }
            if (pressed & SCE_CTRL_TRIANGLE) {
                showMenu = 0;
            }
        } else {
            if (pressed & SCE_CTRL_SQUARE) {
                showProfileSelect = 1;
            }
            if (pressed & SCE_CTRL_TRIANGLE) {
                syncGlobalsToMenu(&menu);
                showMenu = 1;
            }
            if (pressed & SCE_CTRL_SELECT) {
                appState.showAppList = 1;
                appState.scrollOffset = 0;
                appState.selectedRow = 0;
                startAppScan(&appState);
                appScanning = 1;
            }
            if (pressed & SCE_CTRL_LTRIGGER) {
                showToolsMenu = 1;
                toolsMenuSelected = 0;
                toolsMenuScroll = 0;
            }
            if (pressed & SCE_CTRL_START) {
                showAbout = 1;
            }
            if (pressed & SCE_CTRL_CROSS) {
                showPreview = 1;
                startPreviewScan(&preview);
                previewScanning = 1;
            }
            if (pressed & SCE_CTRL_CIRCLE) {
                running = 0;
            }
        }

        sceKernelDelayThread(16 * 1000);
    }

    stopBgWorker();

    if (preview.fileList) freeFileList(preview.fileList);
    if (preview.visibleIndices) free(preview.visibleIndices);
    if (appState.appList) freeAppList(appState.appList);
    if (appState.visibleIndices) free(appState.visibleIndices);

    sceAppUtilShutdown();
    vita2d_free_pgf(font);
    vita2d_fini();

    return 0;
}
