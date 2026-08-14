#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/ctrl.h>
#include <psp2/sysmodule.h>
#include <psp2/display.h>
#include <psp2/gxm.h>
#include <vita2d.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "psv_cleaner_core.h"

#define RGBA(r,g,b,a) ((a) << 24 | (r) << 16 | (g) << 8 | (b))

#define APP_VERSION "v1.16"

#define COL_CARD        RGBA(30, 41, 59, 235)
#define COL_CARD_SOLID  RGBA(30, 41, 59, 255)
#define COL_CARD_DARK   RGBA(15, 23, 42, 240)
#define COL_BORDER      RGBA(51, 65, 85, 255)
#define COL_ACCENT      RGBA(56, 189, 248, 255)
#define COL_ACCENT_DIM  RGBA(56, 189, 248, 120)
#define COL_ACCENT_SOFT RGBA(56, 189, 248, 35)
#define COL_TEXT        RGBA(241, 245, 249, 255)
#define COL_TEXT_DIM    RGBA(148, 163, 184, 255)
#define COL_TEXT_FAINT  RGBA(100, 116, 139, 255)
#define COL_SUCCESS     RGBA(52, 211, 153, 255)
#define COL_DANGER      RGBA(248, 113, 113, 255)
#define COL_WARNING     RGBA(250, 204, 21, 255)
#define COL_ROW_ALT     RGBA(51, 65, 85, 60)
#define COL_SELECTED    RGBA(56, 189, 248, 40)

static vita2d_pgf *g_draw_font = NULL;
static int g_animFrame = 0;

void drawProgressBar(vita2d_pgf *font, int percent);

void gpuMemoryCleanup() {
    vita2d_wait_rendering_done();
}

void showNotification(const char *title, const char *message) {
    printf("NOTIFICATION: %s - %s\n", title, message);
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

void drawBackground() {
    for (int row = 0; row < 544; row += 8) {
        int r = 15 - (13 * row) / 544;
        int g = 23 - (17 * row) / 544;
        int b = 42 - (19 * row) / 544;
        vita2d_draw_rectangle(0, row, 960, 8, RGBA(r, g, b, 255));
    }
    vita2d_draw_fill_circle(110, 70, 170, COL_ACCENT_SOFT);
    vita2d_draw_fill_circle(880, 500, 200, RGBA(99, 102, 241, 26));
}

void drawCardBorder(int x, int y, int w, int h, int color) {
    vita2d_draw_rectangle(x, y, w, 1, color);
    vita2d_draw_rectangle(x, y + h - 1, w, 1, color);
    vita2d_draw_rectangle(x, y, 1, h, color);
    vita2d_draw_rectangle(x + w - 1, y, 1, h, color);
}

void drawCard(int x, int y, int w, int h) {
    vita2d_draw_rectangle(x, y, w, h, COL_CARD);
    drawCardBorder(x, y, w, h, COL_BORDER);
}

void drawCenteredText(vita2d_pgf *font, int cx, int y, int color, float scale, const char *text) {
    int w = (int)(strlen(text) * 9 * scale);
    vita2d_pgf_draw_text(font, cx - w / 2, y, color, scale, text);
}

void drawHeader(vita2d_pgf *font, const char *title, const char *subtitle) {
    vita2d_draw_rectangle(40, 40, 6, 38, COL_ACCENT);
    vita2d_pgf_draw_text(font, 58, 66, COL_TEXT, 1.7f, title);
    if (subtitle) {
        vita2d_pgf_draw_text(font, 58, 88, COL_TEXT_DIM, 0.9f, subtitle);
    }
    vita2d_pgf_draw_text(font, 880, 62, COL_TEXT_FAINT, 0.8f, APP_VERSION);
    vita2d_draw_rectangle(40, 102, 880, 1, COL_BORDER);
}

void drawFooterBar() {
    vita2d_draw_rectangle(0, 496, 960, 48, COL_CARD_DARK);
    vita2d_draw_rectangle(0, 496, 960, 1, COL_ACCENT_DIM);
}

void drawButtonHint(vita2d_pgf *font, int x, int y, const char *buttonText, const char *actionText) {
    int glyphColor = COL_TEXT_DIM;
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
        glyphColor = RGBA(148, 163, 184, 255);
        kind = 5;
    } else if (strcmp(buttonText, "SEL") == 0 || strcmp(buttonText, "SELECT") == 0) {
        glyphColor = RGBA(148, 163, 184, 255);
        kind = 6;
    }

    int textX = x + 34;

    if (kind >= 1 && kind <= 5) {
        int cx = x + 12;
        int cy = y - 7;

        vita2d_draw_fill_circle(cx, cy, 12, COL_BORDER);
        vita2d_draw_fill_circle(cx, cy, 10, COL_CARD_DARK);

        if (kind == 1) {
            for (int i = -1; i <= 1; i++) {
                vita2d_draw_line(cx - 5, cy - 5 + i, cx + 5, cy + 5 + i, glyphColor);
                vita2d_draw_line(cx - 5, cy + 5 + i, cx + 5, cy - 5 + i, glyphColor);
            }
        } else if (kind == 2) {
            vita2d_draw_fill_circle(cx, cy, 7, glyphColor);
            vita2d_draw_fill_circle(cx, cy, 4, COL_CARD_DARK);
        } else if (kind == 3) {
            for (int i = 0; i <= 1; i++) {
                vita2d_draw_line(cx, cy - 6 + i, cx - 6, cy + 5 + i, glyphColor);
                vita2d_draw_line(cx, cy - 6 + i, cx + 6, cy + 5 + i, glyphColor);
                vita2d_draw_line(cx - 6, cy + 4 + i, cx + 6, cy + 4 + i, glyphColor);
            }
        } else if (kind == 4) {
            vita2d_draw_rectangle(cx - 6, cy - 6, 12, 2, glyphColor);
            vita2d_draw_rectangle(cx - 6, cy + 4, 12, 2, glyphColor);
            vita2d_draw_rectangle(cx - 6, cy - 6, 2, 12, glyphColor);
            vita2d_draw_rectangle(cx + 4, cy - 6, 2, 12, glyphColor);
        } else if (kind == 5) {
            vita2d_draw_line(cx, cy - 8, cx, cy + 8, glyphColor);
            vita2d_draw_line(cx - 4, cy - 4, cx, cy - 8, glyphColor);
            vita2d_draw_line(cx + 4, cy - 4, cx, cy - 8, glyphColor);
            vita2d_draw_line(cx - 4, cy + 4, cx, cy + 8, glyphColor);
            vita2d_draw_line(cx + 4, cy + 4, cx, cy + 8, glyphColor);
        }
    } else if (kind == 6) {
        vita2d_draw_rectangle(x, y - 17, 30, 20, COL_CARD_DARK);
        drawCardBorder(x, y - 17, 30, 20, COL_BORDER);
        vita2d_pgf_draw_text(font, x + 5, y - 2, glyphColor, 0.65f, "SEL");
        textX = x + 40;
    } else {
        char chip[8];
        safe_strncpy(chip, buttonText, sizeof(chip));
        int chipW = (int)(strlen(chip) * 8) + 12;
        vita2d_draw_rectangle(x, y - 17, chipW, 20, COL_CARD_DARK);
        drawCardBorder(x, y - 17, chipW, 20, COL_BORDER);
        vita2d_pgf_draw_text(font, x + 6, y - 2, glyphColor, 0.8f, chip);
        textX = x + chipW + 10;
    }

    vita2d_pgf_draw_text(font, textX, y, COL_TEXT_DIM, 0.9f, actionText);
}

void drawSpinnerDots(int cx, int cy, int radius) {
    int step = (g_animFrame / 4) % 8;
    for (int i = 0; i < 8; i++) {
        float angle = i * 0.785398f;
        int px = cx + (int)(radius * cosf(angle));
        int py = cy + (int)(radius * sinf(angle));
        int alpha = 55 + (((i - step + 8) % 8) * 25);
        if (alpha > 255) alpha = 255;
        vita2d_draw_fill_circle(px, py, 4, RGBA(56, 189, 248, alpha));
    }
}

void drawSpinner(vita2d_pgf *font, int cx, int cy, const char *label) {
    drawSpinnerDots(cx, cy, 22);
    drawCenteredText(font, cx, cy + 48, COL_TEXT_DIM, 1.0f, label);
}

void drawProgressBar(vita2d_pgf *font, int percent) {
    drawBackground();

    drawCard(230, 150, 500, 250);
    vita2d_draw_rectangle(231, 151, 498, 3, COL_ACCENT);

    drawCenteredText(font, 480, 195, COL_TEXT, 1.4f, "PSV Cleaner");

    if (isEmergencyStopRequested()) {
        drawCenteredText(font, 480, 230, COL_DANGER, 1.0f, "Stopping operation...");
    } else {
        drawCenteredText(font, 480, 230, COL_ACCENT, 1.0f, "Cleaning in progress...");
    }

    int barX = 270, barY = 256, barW = 420, barH = 18;
    vita2d_draw_rectangle(barX, barY, barW, barH, COL_CARD_DARK);
    drawCardBorder(barX, barY, barW, barH, COL_BORDER);

    int filled = (barW * percent) / 100;
    if (filled > 0) {
        vita2d_draw_rectangle(barX, barY, filled, barH, COL_ACCENT);
        vita2d_draw_rectangle(barX, barY, filled, 6, RGBA(255, 255, 255, 45));
        vita2d_draw_rectangle(barX + filled - 3, barY, 3, barH, RGBA(224, 242, 254, 255));
    }

    char text[32];
    snprintf(text, sizeof(text), "%d%%", percent);
    drawCenteredText(font, 480, 306, COL_TEXT, 1.3f, text);

    char filesText[64];
    snprintf(filesText, sizeof(filesText), "Files deleted: %d", getDeletedFilesCount());
    drawCenteredText(font, 480, 336, COL_TEXT_DIM, 0.85f, filesText);

    const char *stage;
    if (percent < 30) stage = "Scanning files...";
    else if (percent < 70) stage = "Deleting temporary files...";
    else stage = "Finalizing cleanup...";

    drawSpinnerDots(280, 366, 9);
    vita2d_pgf_draw_text(font, 300, 372, COL_TEXT_DIM, 0.9f, stage);

    drawFooterBar();
    if (isEmergencyStopRequested()) {
        drawCenteredText(font, 480, 526, COL_DANGER, 0.9f, "Emergency stop requested - please wait...");
    } else {
        drawButtonHint(font, 350, 526, "O", "Emergency Stop");
    }
}

#define TOTAL_CATEGORIES 36

typedef struct {
    int selected;
    int total_options;
    int scrollOffset;
    char options[TOTAL_CATEGORIES][64];
    int enabled[TOTAL_CATEGORIES];
} MenuOptions;

typedef struct {
    FileList *fileList;
    int scrollOffset;
    int selectedFile;
    SortMode sortMode;
    char fileFilter[16];
    unsigned long long totalVisibleSize;
} PreviewState;

typedef struct {
    AppList *appList;
    int scrollOffset;
    int selectedApp;
    int showAppList;
} AppListState;

typedef enum {
    PROFILE_QUICK = 0,
    PROFILE_COMPLETE,
    PROFILE_SELECTIVE
} CleaningProfile;

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
    excludeVideoFolder = menu->enabled[35];

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

    cleanBrowser = menu->enabled[27];
    cleanVHBB = menu->enabled[28];
    cleanITLS = menu->enabled[29];
    cleanDownloadEnabler = menu->enabled[30];
    cleanMoonlight = menu->enabled[31];
    cleanRetroFlow = menu->enabled[32];
    cleanOrphanedAddcont = menu->enabled[33];
    cleanEmptyLiveareaBubbles = menu->enabled[34];

    cleanThemeCache = menu->enabled[22];
    cleanNotificationCache = menu->enabled[23];
    cleanActivityLog = menu->enabled[24];
    cleanOrphanedLicenseFiles = menu->enabled[25];
    cleanOrphanedDLC = menu->enabled[26];
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
}

const char* getOptionDescription(int index) {
    switch(index) {
        case 0:  return "Deletes temporary files created by the operating system.";
        case 1:  return "Cleans VitaShell icons, logs, and temporary backups.";
        case 2:  return "Removes images and data cached by PKGi/PKGj.";
        case 3:  return "Empties RetroArch & Emu4Vita/Plus shader cache and log files.";
        case 4:  return "Deletes temporary plugin update and install files.";
        case 5:  return "Cleans PSP logs and crash dumps from Adrenaline.";
        case 6:  return "Removes large core dump files (psp2dmp) from the system.";
        case 7:  return "Toggle all cleaning categories at once.";
        case 8:  return "Prevents scanning the Pictures and Screenshots folder.";
        case 9:  return "Do not delete .vpk and .pkg files found in the root directory.";
        case 10: return "Keep VitaDB cache to ensure faster app startup.";
        case 11: return "Scans 'data/' for folders of apps no longer installed.";
        case 12: return "Cleans internal temp folders of ALL installed apps.";
        case 13: return "Cleans icons and preview cache used by EasyVPK.";
        case 14: return "Empties logs from the DAEMON plugin manager.";
        case 15: return "Removes the configuration cache for VitaGrafix.";
        case 16: return "Cleans configurations and icon/theme cache for ONEMenu.";
        case 17: return "Empties runtime logs and temporary folder cache for PCSX ReARMed.";
        case 18: return "Removes configuration and core files cache for mGBA.";
        case 19: return "Deletes temporary logs and emulation cache folders for Flycast.";
        case 20: return "Cleans temporary runtime logs and files for Shellbat.";
        case 21: return "Deletes cache and configurations generated by the Switch User app.";
        case 22: return "Cleans LiveArea theme cache and runtime font data.";
        case 23: return "Empties the system notification history.";
        case 24: return "Deletes logs of recent activities (Activity Log).";
        case 25: return "Removes orphaned .rif licenses from deleted games.";
        case 26: return "Deletes DLC data left behind after game removal.";
        case 27: return "Empties Browser history, cookies, and WebKit cache.";
        case 28: return "Cleans Vita Homebrew Browser temporary data.";
        case 29: return "Removes iTLS-Enso installation logs.";
        case 30: return "Cleans Download Enabler plugin cache.";
        case 31: return "Cleans Moonlight streaming logs and icons.";
        case 32: return "Empties RetroFlow cover and metadata cache.";
        case 33: return "Removes orphaned expansion data (addcont).";
        case 34: return "Deletes bubble folders of apps no longer installed.";
        case 35: return "Prevents scanning the Video thumbnails folder.";
        default: return "Select an option to view specific details.";
    }
}

int isHeader(int index) {
    return index == 7;
}

void initMenuOptions(MenuOptions *menu, CleaningProfile profile) {
    menu->selected = 0;
    menu->scrollOffset = 0;
    menu->total_options = TOTAL_CATEGORIES;

    strcpy(menu->options[0], "System Temp Files");
    strcpy(menu->options[1], "VitaShell Cache");
    strcpy(menu->options[2], "PKGi Cache");
    strcpy(menu->options[3], "RetroArch/Emu4Vita");
    strcpy(menu->options[4], "Autoplugin Cache");
    strcpy(menu->options[5], "Adrenaline Cache");
    strcpy(menu->options[6], "Crash Dumps");
    strcpy(menu->options[7], "All Categories");
    strcpy(menu->options[8], "Exclude Picture Folder");
    strcpy(menu->options[9], "Exclude VPK/PKG Files");
    strcpy(menu->options[10], "Exclude VitaDB Cache");
    strcpy(menu->options[11], "Clean Orphaned App Data");
    strcpy(menu->options[12], "Clean All Apps Temp Files");
    strcpy(menu->options[13], "EasyVPK Cache");
    strcpy(menu->options[14], "DAEMON Cache");
    strcpy(menu->options[15], "VitaGrafix Cache");
    strcpy(menu->options[16], "ONEMenu Cache");
    strcpy(menu->options[17], "PCSX ReARMed Cache");
    strcpy(menu->options[18], "MGBA Cache");
    strcpy(menu->options[19], "Flycast Cache");
    strcpy(menu->options[20], "Shellbat Cache");
    strcpy(menu->options[21], "Switch User Cache");
    strcpy(menu->options[22], "Theme & Font Cache");
    strcpy(menu->options[23], "Notifications Cache");
    strcpy(menu->options[24], "Activity Logs");
    strcpy(menu->options[25], "Orphaned Licenses (.rif)");
    strcpy(menu->options[26], "Orphaned DLC Data");
    strcpy(menu->options[27], "Browser Cache");
    strcpy(menu->options[28], "VHBB Cache");
    strcpy(menu->options[29], "iTLS-Enso Logs");
    strcpy(menu->options[30], "Download Enabler Cache");
    strcpy(menu->options[31], "Moonlight Cache");
    strcpy(menu->options[32], "RetroFlow Cache");
    strcpy(menu->options[33], "Orphaned Addcont");
    strcpy(menu->options[34], "Empty Bubbles");
    strcpy(menu->options[35], "Exclude Video Folder");

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
            for (int i = 13; i <= 21; i++) menu->enabled[i] = 1;
            break;

        case PROFILE_COMPLETE:
            for (int i = 0; i < TOTAL_CATEGORIES; i++) {
                menu->enabled[i] = 1;
            }
            menu->enabled[9] = 0;
            menu->enabled[10] = 0;
            break;

        case PROFILE_SELECTIVE:
            syncGlobalsToMenu(menu);
            break;
    }

    syncMenuToGlobals(menu);
}

void drawScrollbar(int x, int y, int height, int visible, int total, int offset) {
    if (total <= visible) return;
    vita2d_draw_rectangle(x, y, 8, height, RGBA(15, 23, 42, 200));
    int thumbH = (visible * height) / total;
    if (thumbH < 12) thumbH = 12;
    int thumbY = y + (offset * (height - thumbH)) / (total - visible);
    vita2d_draw_rectangle(x, thumbY, 8, thumbH, COL_ACCENT_DIM);
}

void drawMainScreen(vita2d_pgf *font, const char *spaceText, int spaceKnown) {
    drawBackground();
    drawHeader(font, "PSV Cleaner", "Temporary Files Cleaner for PS Vita");

    drawCard(240, 150, 480, 240);
    vita2d_draw_rectangle(241, 151, 478, 3, COL_ACCENT);

    vita2d_pgf_draw_text(font, 280, 195, COL_TEXT_DIM, 0.9f, "SYSTEM STATUS");
    vita2d_pgf_draw_text(font, 280, 232, COL_SUCCESS, 1.3f, "Ready for Cleanup");

    vita2d_draw_rectangle(280, 258, 400, 1, COL_BORDER);

    vita2d_pgf_draw_text(font, 280, 292, COL_TEXT_DIM, 0.9f, "Space to free");
    if (spaceKnown) {
        vita2d_pgf_draw_text(font, 280, 340, COL_ACCENT, 1.9f, spaceText);
    } else {
        drawSpinnerDots(300, 328, 14);
        vita2d_pgf_draw_text(font, 330, 336, COL_TEXT_DIM, 1.0f, "Scanning...");
    }

    drawFooterBar();
    drawButtonHint(font, 40, 526, "□", "Profile");
    drawButtonHint(font, 190, 526, "X", "Preview & Clean");
    drawButtonHint(font, 450, 526, "△", "Options");
    drawButtonHint(font, 630, 526, "SEL", "Apps");
    drawButtonHint(font, 810, 526, "O", "Exit");
}

void drawProfileSelect(vita2d_pgf *font, CleaningProfile selected) {
    drawBackground();
    drawHeader(font, "Select Cleaning Profile", "Choose how deep the cleanup should go");

    const char* names[] = {"Quick Clean", "Complete Clean", "Selective Clean"};
    const char* descs[] = {
        "Removes app caches, browser temp and logs (fast & safe)",
        "Removes all temp files, crash dumps and system cache",
        "Manually choose which categories to clean"
    };

    for (int i = 0; i < 3; i++) {
        int y = 135 + i * 105;
        int isSelected = (i == (int)selected);

        vita2d_draw_rectangle(200, y, 560, 86, isSelected ? COL_CARD_SOLID : COL_CARD);
        drawCardBorder(200, y, 560, 86, isSelected ? COL_ACCENT : COL_BORDER);

        if (isSelected) {
            vita2d_draw_rectangle(201, y + 1, 5, 84, COL_ACCENT);
            vita2d_draw_rectangle(201, y + 1, 558, 84, COL_ACCENT_SOFT);
        }

        vita2d_pgf_draw_text(font, 235, y + 36, isSelected ? COL_TEXT : COL_TEXT_DIM, 1.2f, names[i]);
        vita2d_pgf_draw_text(font, 235, y + 64, COL_TEXT_FAINT, 0.85f, descs[i]);
    }

    drawFooterBar();
    drawButtonHint(font, 180, 526, "↕", "Navigate");
    drawButtonHint(font, 400, 526, "X", "Confirm");
    drawButtonHint(font, 620, 526, "O", "Back");
}

void drawOptionsMenu(vita2d_pgf *font, MenuOptions *menu) {
    drawBackground();
    drawHeader(font, "Advanced Options", "Toggle the categories you want to clean");

    drawCard(90, 118, 780, 368);

    int maxVisible = 7;
    int startIdx = menu->scrollOffset;
    int endIdx = startIdx + maxVisible;
    if (endIdx > menu->total_options) endIdx = menu->total_options;

    for (int i = startIdx; i < endIdx; i++) {
        int displayIndex = i - startIdx;
        int y = 128 + displayIndex * 38;
        int isSelected = (i == menu->selected);

        if (isSelected) {
            vita2d_draw_rectangle(92, y, 760, 36, COL_SELECTED);
            vita2d_draw_rectangle(92, y, 4, 36, COL_ACCENT);
        }

        if (i != 7) {
            int cx = 122, cy = y + 9;
            if (menu->enabled[i]) {
                vita2d_draw_rectangle(cx, cy, 18, 18, COL_SUCCESS);
                vita2d_pgf_draw_text(font, cx + 3, cy + 15, RGBA(6, 30, 20, 255), 0.9f, "✓");
            } else {
                vita2d_draw_rectangle(cx, cy, 18, 18, COL_CARD_DARK);
                drawCardBorder(cx, cy, 18, 18, COL_BORDER);
            }
        }

        int textColor = isSelected ? COL_TEXT : (menu->enabled[i] ? COL_TEXT_DIM : COL_TEXT_FAINT);
        vita2d_pgf_draw_text(font, 165, y + 24, textColor, 1.0f, menu->options[i]);

        if (i != 7) {
            const char *stateText = menu->enabled[i] ? "ON" : "OFF";
            int stateColor = menu->enabled[i] ? COL_SUCCESS : COL_TEXT_FAINT;
            vita2d_pgf_draw_text(font, 810, y + 24, stateColor, 0.8f, stateText);
        }
    }

    drawScrollbar(852, 128, 258, maxVisible, menu->total_options, menu->scrollOffset);

    vita2d_draw_rectangle(110, 400, 740, 1, COL_BORDER);
    vita2d_pgf_draw_text(font, 110, 425, COL_ACCENT, 0.85f, "DETAILS");
    vita2d_pgf_draw_text(font, 110, 452, COL_TEXT_DIM, 0.9f, getOptionDescription(menu->selected));

    drawFooterBar();
    drawButtonHint(font, 100, 526, "↕", "Navigate");
    drawButtonHint(font, 290, 526, "X", "Toggle");
    drawButtonHint(font, 470, 526, "O", "Preview");
    drawButtonHint(font, 670, 526, "△", "Back");
}

void drawPreviewScreen(vita2d_pgf *font, PreviewState *preview, int scanning) {
    drawBackground();
    drawHeader(font, "Preview", "Files that will be deleted");

    if (scanning) {
        drawSpinner(font, 480, 260, "Scanning files...");
        drawFooterBar();
        return;
    }

    if (!preview->fileList || preview->fileList->count == 0) {
        drawCard(280, 220, 400, 100);
        drawCenteredText(font, 480, 278, COL_WARNING, 1.1f, "No temporary files found!");
        drawFooterBar();
        drawButtonHint(font, 420, 526, "O", "Back");
        return;
    }

    const char* sortLabels[] = {"Name", "Size"};
    char infoText[128];
    snprintf(infoText, sizeof(infoText), "Sort: %s   |   Filter: %s",
             sortLabels[preview->sortMode],
             strlen(preview->fileFilter) > 0 ? preview->fileFilter : "All");
    vita2d_pgf_draw_text(font, 40, 128, COL_TEXT_DIM, 0.9f, infoText);

    char totalText[96];
    char sizeBuf[32];
    formatSize(preview->totalVisibleSize, sizeBuf, sizeof(sizeBuf));
    snprintf(totalText, sizeof(totalText), "Files: %d   |   Total: %s", preview->fileList->count, sizeBuf);
    vita2d_pgf_draw_text(font, 640, 128, COL_SUCCESS, 0.9f, totalText);

    int maxVisible = 16;
    int startIdx = preview->scrollOffset;
    int endIdx = startIdx + maxVisible;
    if (endIdx > preview->fileList->count) endIdx = preview->fileList->count;

    for (int i = startIdx; i < endIdx; i++) {
        int rowIdx = i - startIdx;
        int y = 142 + rowIdx * 20;

        if (i == preview->selectedFile) {
            vita2d_draw_rectangle(40, y, 880, 20, COL_SELECTED);
            vita2d_draw_rectangle(40, y, 3, 20, COL_ACCENT);
        } else if (rowIdx % 2 == 1) {
            vita2d_draw_rectangle(40, y, 880, 20, COL_ROW_ALT);
        }

        char displayPath[88];
        if (strlen(preview->fileList->files[i].path) > 80) {
            strncpy(displayPath, preview->fileList->files[i].path, 77);
            displayPath[77] = '.';
            displayPath[78] = '.';
            displayPath[79] = '.';
            displayPath[80] = '\0';
        } else {
            strcpy(displayPath, preview->fileList->files[i].path);
        }

        int color = (i == preview->selectedFile) ? COL_TEXT : COL_TEXT_DIM;
        vita2d_pgf_draw_text(font, 52, y + 15, color, 0.7f, displayPath);

        char sizeText[32];
        formatSize(preview->fileList->files[i].size, sizeText, sizeof(sizeText));
        vita2d_pgf_draw_text(font, 830, y + 15, color, 0.7f, sizeText);
    }

    drawScrollbar(928, 142, 320, maxVisible, preview->fileList->count, preview->scrollOffset);

    drawFooterBar();
    drawButtonHint(font, 30, 526, "↕", "Navigate");
    drawButtonHint(font, 180, 526, "△", "Sort");
    drawButtonHint(font, 300, 526, "□", "Filter");
    drawButtonHint(font, 430, 526, "SEL", "Delete");
    drawButtonHint(font, 580, 526, "X", "Clean All");
    drawButtonHint(font, 760, 526, "O", "Back");
}

void drawAppListScreen(vita2d_pgf *font, AppListState *appState, int scanning) {
    drawBackground();
    drawHeader(font, "Select App to Clean", "Per-app temporary files");

    if (scanning) {
        drawSpinner(font, 480, 260, "Scanning apps...");
        drawFooterBar();
        return;
    }

    if (!appState->appList || appState->appList->count == 0) {
        drawCard(280, 220, 400, 100);
        drawCenteredText(font, 480, 278, COL_WARNING, 1.1f, "No installed apps found!");
        drawFooterBar();
        drawButtonHint(font, 420, 526, "O", "Back");
        return;
    }

    unsigned long long totalSize = 0;
    for (int i = 0; i < appState->appList->count; i++) {
        totalSize += appState->appList->apps[i].tempSize;
    }

    char totalText[96];
    char sizeBuf[32];
    formatSize(totalSize, sizeBuf, sizeof(sizeBuf));
    snprintf(totalText, sizeof(totalText), "Apps: %d   |   Total Temp Files: %s", appState->appList->count, sizeBuf);
    vita2d_pgf_draw_text(font, 40, 128, COL_SUCCESS, 0.9f, totalText);

    int maxVisible = 15;
    int startIdx = appState->scrollOffset;
    int endIdx = startIdx + maxVisible;
    if (endIdx > appState->appList->count) endIdx = appState->appList->count;

    for (int i = startIdx; i < endIdx; i++) {
        int rowIdx = i - startIdx;
        int y = 142 + rowIdx * 22;

        if (i == appState->selectedApp) {
            vita2d_draw_rectangle(40, y, 880, 22, COL_SELECTED);
            vita2d_draw_rectangle(40, y, 3, 22, COL_ACCENT);
        } else if (rowIdx % 2 == 1) {
            vita2d_draw_rectangle(40, y, 880, 22, COL_ROW_ALT);
        }

        int color = (i == appState->selectedApp) ? COL_TEXT : COL_TEXT_DIM;
        vita2d_pgf_draw_text(font, 52, y + 16, color, 0.85f, appState->appList->apps[i].titleId);

        char sizeText[32];
        formatSize(appState->appList->apps[i].tempSize, sizeText, sizeof(sizeText));
        vita2d_pgf_draw_text(font, 830, y + 16, color, 0.85f, sizeText);
    }

    drawScrollbar(928, 142, 330, maxVisible, appState->appList->count, appState->scrollOffset);

    drawFooterBar();
    drawButtonHint(font, 120, 526, "↕", "Navigate");
    drawButtonHint(font, 360, 526, "X", "Clean Selected");
    drawButtonHint(font, 640, 526, "O", "Back");
}

void drawAppCleaningScreen(vita2d_pgf *font) {
    drawBackground();
    drawHeader(font, "Cleaning App", "Removing temporary files");
    drawSpinner(font, 480, 260, "Cleaning...");
    drawFooterBar();
    drawButtonHint(font, 380, 526, "O", "Emergency Stop");
}

void drawDeleteConfirmation(vita2d_pgf *font, PreviewState *preview) {
    vita2d_draw_rectangle(0, 0, 960, 544, RGBA(0, 0, 0, 170));

    drawCard(220, 170, 520, 210);
    vita2d_draw_rectangle(221, 171, 518, 3, COL_DANGER);

    drawCenteredText(font, 480, 210, COL_TEXT, 1.3f, "Delete File");
    drawCenteredText(font, 480, 245, COL_WARNING, 0.95f, "Are you sure you want to delete this file?");

    vita2d_draw_rectangle(250, 262, 460, 1, COL_BORDER);

    char filename[80] = {0};
    char sizeText[32] = {0};
    if (preview->fileList && preview->selectedFile >= 0 && preview->selectedFile < preview->fileList->count) {
        char *path = preview->fileList->files[preview->selectedFile].path;
        char *basename = strrchr(path, '/');
        basename = basename ? basename + 1 : path;
        if (strlen(basename) > 70) {
            strncpy(filename, basename, 67);
            filename[67] = '.';
            filename[68] = '.';
            filename[69] = '.';
            filename[70] = '\0';
        } else {
            strcpy(filename, basename);
        }
        formatSize(preview->fileList->files[preview->selectedFile].size, sizeText, sizeof(sizeText));
    }

    drawCenteredText(font, 480, 295, COL_TEXT_DIM, 0.9f, filename);
    drawCenteredText(font, 480, 322, COL_ACCENT, 0.9f, sizeText);

    drawButtonHint(font, 300, 360, "X", "Delete File");
    drawButtonHint(font, 520, 360, "O", "Cancel");
}

void drawCleanAllConfirmation(vita2d_pgf *font, FileList *fileList) {
    vita2d_draw_rectangle(0, 0, 960, 544, RGBA(0, 0, 0, 170));

    drawCard(220, 180, 520, 190);
    vita2d_draw_rectangle(221, 181, 518, 3, COL_WARNING);

    drawCenteredText(font, 480, 220, COL_TEXT, 1.3f, "Clean All Files");
    drawCenteredText(font, 480, 255, COL_WARNING, 0.95f, "Are you sure you want to clean all files?");

    vita2d_draw_rectangle(250, 272, 460, 1, COL_BORDER);

    char filesCountText[64];
    snprintf(filesCountText, sizeof(filesCountText), "Total files to delete: %d", fileList ? fileList->count : 0);
    drawCenteredText(font, 480, 305, COL_TEXT_DIM, 0.95f, filesCountText);

    drawButtonHint(font, 300, 345, "X", "Clean All");
    drawButtonHint(font, 520, 345, "O", "Cancel");
}

void drawCheckIcon(int cx, int cy) {
    vita2d_draw_fill_circle(cx, cy, 48, RGBA(52, 211, 153, 50));
    vita2d_draw_fill_circle(cx, cy, 36, COL_SUCCESS);
    for (int i = -1; i <= 1; i++) {
        vita2d_draw_line(cx - 16, cy + i, cx - 4, cy + 12 + i, RGBA(6, 30, 20, 255));
        vita2d_draw_line(cx - 4, cy + 12 + i, cx + 18, cy - 12 + i, RGBA(6, 30, 20, 255));
    }
}

void drawWarningIcon(int cx, int cy) {
    vita2d_draw_fill_circle(cx, cy, 48, RGBA(250, 204, 21, 45));
    vita2d_draw_fill_circle(cx, cy, 36, COL_WARNING);
    vita2d_pgf_draw_text(g_draw_font, cx - 6, cy + 15, RGBA(40, 30, 0, 255), 1.8f, "!");
}

void drawStatBox(vita2d_pgf *font, int x, int y, const char *label, const char *value, int valueColor) {
    drawCard(x, y, 230, 96);
    vita2d_pgf_draw_text(font, x + 22, y + 34, COL_TEXT_DIM, 0.9f, label);
    vita2d_pgf_draw_text(font, x + 22, y + 70, valueColor, 1.15f, value);
}

void drawCompletionScreen(vita2d_pgf *font, int cleanupCount, const char *spaceText, int filesDeleted) {
    drawBackground();
    drawCheckIcon(480, 130);

    drawCenteredText(font, 480, 215, COL_SUCCESS, 1.9f, "CLEANING COMPLETED");
    drawCenteredText(font, 480, 252, COL_TEXT_DIM, 1.0f, "Great job! Your PS Vita is cleaner!");

    drawStatBox(font, 235, 285, "Space Freed", spaceText, COL_ACCENT);

    char filesText[64];
    snprintf(filesText, sizeof(filesText), "%d", filesDeleted);
    drawStatBox(font, 495, 285, "Files Deleted", filesText, COL_SUCCESS);

    char countText[64];
    snprintf(countText, sizeof(countText), "Cleanup #%d", cleanupCount);
    drawCenteredText(font, 480, 415, COL_TEXT_FAINT, 0.9f, countText);
}

void drawInterruptedScreen(vita2d_pgf *font, const char *spaceText, int filesDeleted) {
    drawBackground();
    drawWarningIcon(480, 130);

    drawCenteredText(font, 480, 215, COL_WARNING, 1.9f, "OPERATION INTERRUPTED");
    drawCenteredText(font, 480, 252, COL_TEXT_DIM, 1.0f, "Cleaning was safely interrupted - some files may have been deleted");

    drawStatBox(font, 235, 285, "Space Freed", spaceText, COL_ACCENT);

    char filesText[64];
    snprintf(filesText, sizeof(filesText), "%d", filesDeleted);
    drawStatBox(font, 495, 285, "Files Deleted", filesText, COL_SUCCESS);
}

void drawAppCleanedScreen(vita2d_pgf *font, const char *titleId, const char *spaceText, int filesDeleted) {
    drawBackground();
    drawCheckIcon(480, 120);

    drawCenteredText(font, 480, 200, COL_SUCCESS, 1.8f, "APP CLEANED");

    char titleText[64];
    snprintf(titleText, sizeof(titleText), "App: %s", titleId);
    drawCenteredText(font, 480, 238, COL_TEXT_DIM, 1.0f, titleText);

    drawStatBox(font, 235, 275, "Space Freed", spaceText, COL_ACCENT);

    char filesText[64];
    snprintf(filesText, sizeof(filesText), "%d", filesDeleted);
    drawStatBox(font, 495, 275, "Files Deleted", filesText, COL_SUCCESS);
}

void startPreviewScan(PreviewState *preview) {
    waitBgIdle();
    preview->scrollOffset = 0;
    preview->selectedFile = 0;
    if (preview->fileList) {
        freeFileList(preview->fileList);
        preview->fileList = NULL;
    }
    preview->fileList = createFileList();
    if (!preview->fileList) return;
    g_bgPreviewList = preview->fileList;
    g_bgSortMode = preview->sortMode;
    safe_strncpy(g_bgFileFilter, preview->fileFilter, MAX_FILE_FILTER_LENGTH);
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

void startSizeCalc() {
    if (isBgBusy()) return;
    requestBgTask(BG_TASK_CALC_SIZE);
}

int main() {
    sceSysmoduleLoadModule(SCE_SYSMODULE_PGF);
    sceSysmoduleLoadModule(SCE_SYSMODULE_APPUTIL);

    vita2d_init();
    vita2d_pgf *font = vita2d_load_default_pgf();
    g_draw_font = font;
    if (!font) {
        printf("Failed to load default PGF font!\n");
        vita2d_fini();
        return -1;
    }

    SceCtrlData pad;

    initEmergencyStop();
    detectSystemLanguage();
    loadSettings();
    startBgWorker();

    char spaceValueText[32];
    strcpy(spaceValueText, "Scanning...");
    int spaceKnown = 0;

    MenuOptions menu;
    initMenuOptions(&menu, PROFILE_SELECTIVE);

    PreviewState preview;
    preview.fileList = NULL;
    preview.scrollOffset = 0;
    preview.selectedFile = 0;
    preview.sortMode = SORT_BY_NAME;
    strcpy(preview.fileFilter, "");
    preview.totalVisibleSize = 0;

    AppListState appState;
    appState.appList = NULL;
    appState.scrollOffset = 0;
    appState.selectedApp = 0;
    appState.showAppList = 0;

    int previewScanning = 0;
    int appScanning = 0;
    int cleaningInProgress = 0;
    int appCleaningInProgress = 0;
    int sizeCalcQueued = 0;

    int showMenu = 0;
    int showPreview = 0;
    int showDeleteConfirmation = 0;
    int showCleanAllConfirmation = 0;
    int showProfileSelect = 1;
    CleaningProfile selectedProfile = PROFILE_COMPLETE;
    int running = 1;
    int currentFrame = 0;

    startSizeCalc();

    while (running) {
        sceCtrlPeekBufferPositive(0, &pad, 1);
        currentFrame++;
        g_animFrame++;

        if (appCleaningInProgress && g_bgTaskDone) {
            appCleaningInProgress = 0;
            endOperation();

            int cleanupCount = loadCleanupCounter() + 1;
            saveCleanupCounter(cleanupCount);

            int filesDeleted = getDeletedFilesCount();

            char spaceText[32];
            formatSize(g_bgSpaceFreed, spaceText, sizeof(spaceText));

            vita2d_start_drawing();
            vita2d_clear_screen();
            drawAppCleanedScreen(font, appState.appList->apps[appState.selectedApp].titleId, spaceText, filesDeleted);
            vita2d_end_drawing();
            vita2d_swap_buffers();
            sceKernelDelayThread(2 * 1000 * 1000);

            startAppScan(&appState);
            appScanning = 1;
            sizeCalcQueued = 1;

            spaceKnown = 0;
            strcpy(spaceValueText, "Scanning...");
        }

        if (cleaningInProgress && g_bgTaskDone) {
            cleaningInProgress = 0;
            g_progressCallback = NULL;

            int cleaningInterrupted = isEmergencyStopRequested();
            endOperation();

            int filesDeleted = getDeletedFilesCount();
            char spaceText[32];
            formatSize(preview.fileList ? preview.fileList->totalSize : 0, spaceText, sizeof(spaceText));

            if (cleaningInterrupted) {
                cleanupAfterEmergencyStop();

                vita2d_start_drawing();
                vita2d_clear_screen();
                drawInterruptedScreen(font, spaceText, filesDeleted);
                vita2d_end_drawing();
                vita2d_swap_buffers();
                sceKernelDelayThread(4 * 1000 * 1000);
            } else {
                int cleanupCount = loadCleanupCounter() + 1;
                saveCleanupCounter(cleanupCount);

                showNotification("PSV Cleaner", "Cleaning completed successfully!");

                vita2d_start_drawing();
                vita2d_clear_screen();
                drawCompletionScreen(font, cleanupCount, spaceText, filesDeleted);
                vita2d_end_drawing();
                vita2d_swap_buffers();
                sceKernelDelayThread(3 * 1000 * 1000);
            }

            if (preview.fileList) {
                freeFileList(preview.fileList);
                preview.fileList = NULL;
            }

            spaceKnown = 0;
            strcpy(spaceValueText, "Scanning...");
            startSizeCalc();
        }

        if (g_bgTaskDone) {
            if (previewScanning) {
                previewScanning = 0;
                preview.totalVisibleSize = g_bgVisibleSize;
                g_bgPreviewList = NULL;
            }
            if (appScanning) {
                appScanning = 0;
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

        if (g_bgTaskDone && currentFrame % 300 == 0) {
            requestBgTask(BG_TASK_CALC_SIZE);
        }

        if ((previewScanning || appScanning || cleaningInProgress || appCleaningInProgress) && currentFrame % 120 == 0) {
            sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
        }

        vita2d_start_drawing();
        vita2d_clear_screen();

        if (cleaningInProgress) {
            drawProgressBar(font, getLastProgressPercent());
        } else if (appCleaningInProgress) {
            drawAppCleaningScreen(font);
        } else if (showCleanAllConfirmation) {
            drawPreviewScreen(font, &preview, previewScanning);
            drawCleanAllConfirmation(font, preview.fileList);
        } else if (showDeleteConfirmation) {
            drawPreviewScreen(font, &preview, previewScanning);
            drawDeleteConfirmation(font, &preview);
        } else if (appState.showAppList) {
            drawAppListScreen(font, &appState, appScanning);
        } else if (showPreview) {
            drawPreviewScreen(font, &preview, previewScanning);
        } else if (showMenu) {
            drawOptionsMenu(font, &menu);
        } else if (showProfileSelect) {
            drawProfileSelect(font, selectedProfile);
        } else {
            drawMainScreen(font, spaceValueText, spaceKnown);
        }

        vita2d_end_drawing();
        vita2d_swap_buffers();

        gpuMemoryCleanup();

        if (cleaningInProgress || appCleaningInProgress) {
            if (pad.buttons & SCE_CTRL_CIRCLE) {
                requestEmergencyStop();
            }
        } else if (showProfileSelect) {
            if (pad.buttons & SCE_CTRL_UP) {
                int p = (int)selectedProfile - 1;
                if (p < 0) p = 2;
                selectedProfile = (CleaningProfile)p;
                sceKernelDelayThread(200 * 1000);
            }
            if (pad.buttons & SCE_CTRL_DOWN) {
                int p = (int)selectedProfile + 1;
                if (p > 2) p = 0;
                selectedProfile = (CleaningProfile)p;
                sceKernelDelayThread(200 * 1000);
            }
            if (pad.buttons & SCE_CTRL_CROSS) {
                showProfileSelect = 0;
                initMenuOptions(&menu, selectedProfile);

                if (selectedProfile == PROFILE_SELECTIVE) {
                    showMenu = 1;
                } else {
                    showPreview = 1;
                    startPreviewScan(&preview);
                    previewScanning = 1;
                }
                sceKernelDelayThread(200 * 1000);
            }
            if (pad.buttons & SCE_CTRL_CIRCLE) {
                showProfileSelect = 0;
                sceKernelDelayThread(200 * 1000);
            }
        } else if (showCleanAllConfirmation) {
            if (pad.buttons & SCE_CTRL_CROSS) {
                if (preview.fileList && preview.fileList->count > 0) {
                    showCleanAllConfirmation = 0;
                    showPreview = 0;
                    cleaningInProgress = 1;

                    waitBgIdle();
                    sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);

                    resetScanProgress();
                    g_progressCallback = NULL;

                    startOperation();
                    requestBgTask(BG_TASK_CLEAN);
                }
                sceKernelDelayThread(200 * 1000);
            }
            if (pad.buttons & SCE_CTRL_CIRCLE) {
                showCleanAllConfirmation = 0;
                sceKernelDelayThread(200 * 1000);
            }
        } else if (showDeleteConfirmation) {
            if (pad.buttons & SCE_CTRL_CROSS) {
                if (preview.fileList && preview.fileList->count > 0 && preview.selectedFile >= 0 && preview.selectedFile < preview.fileList->count) {
                    if (deleteSingleFileFromList(preview.fileList, preview.selectedFile)) {
                        filterAndSortFileList(preview.fileList, preview.sortMode, preview.fileFilter, &preview.totalVisibleSize);

                        if (preview.selectedFile >= preview.fileList->count && preview.fileList->count > 0) {
                            preview.selectedFile = preview.fileList->count - 1;
                        }
                    }
                }
                showDeleteConfirmation = 0;
                sceKernelDelayThread(200 * 1000);
            }
            if (pad.buttons & SCE_CTRL_CIRCLE) {
                showDeleteConfirmation = 0;
                sceKernelDelayThread(200 * 1000);
            }
        } else if (appState.showAppList) {
            if (!appScanning) {
                if (pad.buttons & SCE_CTRL_UP) {
                    if (appState.selectedApp > 0) {
                        appState.selectedApp--;
                        if (appState.selectedApp < appState.scrollOffset) {
                            appState.scrollOffset = appState.selectedApp;
                        }
                    }
                    sceKernelDelayThread(100 * 1000);
                }
                if (pad.buttons & SCE_CTRL_DOWN) {
                    if (appState.appList && appState.selectedApp < appState.appList->count - 1) {
                        appState.selectedApp++;
                        if (appState.selectedApp >= appState.scrollOffset + 15) {
                            appState.scrollOffset = appState.selectedApp - 14;
                        }
                    }
                    sceKernelDelayThread(100 * 1000);
                }
                if (pad.buttons & SCE_CTRL_CROSS) {
                    if (appState.appList && appState.appList->count > 0 &&
                        appState.selectedApp >= 0 && appState.selectedApp < appState.appList->count) {

                        appCleaningInProgress = 1;

                        waitBgIdle();
                        sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);

                        resetDeletedFilesCount();
                        g_progressCallback = NULL;

                        startOperation();
                        safe_strncpy(g_bgCleanAppTitleId,
                                     appState.appList->apps[appState.selectedApp].titleId,
                                     sizeof(g_bgCleanAppTitleId));
                        requestBgTask(BG_TASK_CLEAN_APP);
                    }
                    sceKernelDelayThread(200 * 1000);
                }
                if (pad.buttons & SCE_CTRL_CIRCLE) {
                    appState.showAppList = 0;
                    if (appState.appList) {
                        freeAppList(appState.appList);
                        appState.appList = NULL;
                    }
                    sceKernelDelayThread(200 * 1000);
                }
            }
        } else if (showPreview) {
            if (!previewScanning) {
                if (pad.buttons & SCE_CTRL_UP) {
                    if (preview.selectedFile > 0) {
                        preview.selectedFile--;
                        if (preview.selectedFile < preview.scrollOffset) {
                            preview.scrollOffset = preview.selectedFile;
                        }
                    }
                    sceKernelDelayThread(100 * 1000);
                }
                if (pad.buttons & SCE_CTRL_DOWN) {
                    if (preview.fileList && preview.selectedFile < preview.fileList->count - 1) {
                        preview.selectedFile++;
                        if (preview.selectedFile >= preview.scrollOffset + 16) {
                            preview.scrollOffset = preview.selectedFile - 15;
                        }
                    }
                    sceKernelDelayThread(100 * 1000);
                }
                if (pad.buttons & SCE_CTRL_TRIANGLE) {
                    preview.sortMode = (preview.sortMode == SORT_BY_NAME) ? SORT_BY_SIZE : SORT_BY_NAME;
                    if (preview.fileList) {
                        filterAndSortFileList(preview.fileList, preview.sortMode, preview.fileFilter, &preview.totalVisibleSize);
                    }
                    sceKernelDelayThread(200 * 1000);
                }
                if (pad.buttons & SCE_CTRL_SQUARE) {
                    const char* filters[] = {"", "tmp", "log", "cache", "dmp", "vpk"};
                    int currentFilterIndex = 0;
                    for (int i = 0; i < 6; i++) {
                        if (strcmp(preview.fileFilter, filters[i]) == 0) {
                            currentFilterIndex = i;
                            break;
                        }
                    }
                    currentFilterIndex = (currentFilterIndex + 1) % 6;
                    strcpy(preview.fileFilter, filters[currentFilterIndex]);
                    if (preview.fileList) {
                        filterAndSortFileList(preview.fileList, preview.sortMode, preview.fileFilter, &preview.totalVisibleSize);
                    }
                    sceKernelDelayThread(200 * 1000);
                }
                if (pad.buttons & SCE_CTRL_SELECT) {
                    if (preview.fileList && preview.fileList->count > 0 && preview.selectedFile >= 0 && preview.selectedFile < preview.fileList->count) {
                        showDeleteConfirmation = 1;
                    }
                    sceKernelDelayThread(200 * 1000);
                }
                if (pad.buttons & SCE_CTRL_CROSS) {
                    if (preview.fileList && preview.fileList->count > 0) {
                        showCleanAllConfirmation = 1;
                    }
                    sceKernelDelayThread(200 * 1000);
                }
                if (pad.buttons & SCE_CTRL_CIRCLE) {
                    showPreview = 0;
                    if (preview.fileList) {
                        freeFileList(preview.fileList);
                        preview.fileList = NULL;
                    }
                    sceKernelDelayThread(200 * 1000);
                }
            }
        } else if (showMenu) {
            if (pad.buttons & SCE_CTRL_UP) {
                if (menu.selected > 0) {
                    menu.selected--;
                    if (menu.selected < menu.scrollOffset && menu.scrollOffset > 0) {
                        menu.scrollOffset = menu.selected;
                    }
                }
                sceKernelDelayThread(150 * 1000);
            }
            if (pad.buttons & SCE_CTRL_DOWN) {
                if (menu.selected < menu.total_options - 1) {
                    menu.selected++;
                    int maxVisible = 7;
                    if (menu.selected >= menu.scrollOffset + maxVisible) {
                        menu.scrollOffset = menu.selected - maxVisible + 1;
                    }
                }
                sceKernelDelayThread(150 * 1000);
            }
            if (pad.buttons & SCE_CTRL_CROSS) {
                if (menu.selected == 7) {
                    int targetState = !menu.enabled[0];
                    for (int i = 0; i < TOTAL_CATEGORIES; i++) {
                        if (i != 7) menu.enabled[i] = targetState;
                    }
                    syncMenuToGlobals(&menu);
                    saveSettings();
                } else if (menu.selected >= 0 && menu.selected < TOTAL_CATEGORIES) {
                    menu.enabled[menu.selected] = !menu.enabled[menu.selected];
                    syncMenuToGlobals(&menu);
                    saveSettings();
                }
                sceKernelDelayThread(200 * 1000);
            }
            if (pad.buttons & SCE_CTRL_SELECT) {
                int allEnabled = 1;
                for (int i = 0; i < 7; i++) {
                    if (!menu.enabled[i]) {
                        allEnabled = 0;
                        break;
                    }
                }
                for (int i = 0; i < 7; i++) {
                    menu.enabled[i] = !allEnabled;
                }
                syncMenuToGlobals(&menu);
                saveSettings();
                sceKernelDelayThread(200 * 1000);
            }
            if (pad.buttons & SCE_CTRL_CIRCLE) {
                showPreview = 1;
                startPreviewScan(&preview);
                previewScanning = 1;
                showMenu = 0;
                sceKernelDelayThread(200 * 1000);
            }
            if (pad.buttons & SCE_CTRL_TRIANGLE) {
                showMenu = 0;
                sceKernelDelayThread(200 * 1000);
            }
        } else {
            if (pad.buttons & SCE_CTRL_SQUARE) {
                showProfileSelect = 1;
                sceKernelDelayThread(200 * 1000);
            }
            if (pad.buttons & SCE_CTRL_TRIANGLE) {
                showMenu = 1;
                sceKernelDelayThread(200 * 1000);
            }
            if (pad.buttons & SCE_CTRL_SELECT) {
                appState.showAppList = 1;
                appState.scrollOffset = 0;
                appState.selectedApp = 0;
                startAppScan(&appState);
                appScanning = 1;
                sceKernelDelayThread(200 * 1000);
            }
            if (pad.buttons & SCE_CTRL_CROSS) {
                showPreview = 1;
                startPreviewScan(&preview);
                previewScanning = 1;
                sceKernelDelayThread(200 * 1000);
            }
            if (pad.buttons & SCE_CTRL_CIRCLE) {
                running = 0;
            }
        }

        sceKernelDelayThread(16 * 1000);
    }

    stopBgWorker();

    if (preview.fileList) {
        freeFileList(preview.fileList);
    }
    if (appState.appList) {
        freeAppList(appState.appList);
    }

    vita2d_free_pgf(font);
    vita2d_fini();

    return 0;
}
