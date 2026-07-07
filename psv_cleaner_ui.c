#include <psp2/kernel/threadmgr.h>
#include <psp2/ctrl.h>
#include <psp2/sysmodule.h>
#include <psp2/display.h>
#include <psp2/gxm.h>
#include <vita2d.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "psv_cleaner_core.h"

#define RGBA(r,g,b,a) ((a) << 24 | (r) << 16 | (g) << 8 | (b))

static vita2d_pgf *g_draw_font = NULL;
void drawProgressBar(vita2d_pgf *font, int percent);

void updateCleanupProgress(int percent) {
    SceCtrlData pad;
    sceCtrlPeekBufferPositive(0, &pad, 1);
    if (pad.buttons & SCE_CTRL_CIRCLE) {
        requestEmergencyStop();
    }

    if (g_draw_font) {
        vita2d_start_drawing();
        vita2d_clear_screen();
        drawProgressBar(g_draw_font, percent);
        vita2d_end_drawing();
        vita2d_swap_buffers();
    }
}

void gpuMemoryCleanup() {
    vita2d_wait_rendering_done();
}

#define MAX_PARTICLES 50

typedef struct {
    float x, y;
    float vx, vy;
    float life;
    float maxLife;
    int color;
} Particle;

Particle particles[MAX_PARTICLES];
int particleCount = 0;
int lastPercent = 0;

void initParticles() {
    particleCount = 0;
    lastPercent = 0;
}

void addParticle(float x, float y) {
    if (particleCount >= MAX_PARTICLES) return;

    particles[particleCount].x = x;
    particles[particleCount].y = y;
    particles[particleCount].vx = ((rand() % 80) - 40) * 0.08f; // Smoother horizontal velocity
    particles[particleCount].vy = -((rand() % 40) + 15) * 0.08f; // Smoother vertical velocity
    particles[particleCount].life = 1.0f;
    particles[particleCount].maxLife = 1.0f;
    particles[particleCount].color = RGBA(100 + rand() % 155, 200 + rand() % 55, 255, 255);
    particleCount++;
}

void updateParticles() {
    for (int i = 0; i < particleCount; i++) {
        particles[i].x += particles[i].vx;
        particles[i].y += particles[i].vy;
        particles[i].vy += 0.08f; // Slightly reduced gravity for smoother motion
        particles[i].life -= 0.015f; // Slower decay for longer-lasting particles

        if (particles[i].life <= 0) {
            particles[i] = particles[--particleCount];
            i--;
        }
    }
    
    // Limit particle count for performance
    if (particleCount > MAX_PARTICLES / 2) {
        particleCount = MAX_PARTICLES / 2;
    }
}

void drawParticles() {
    for (int i = 0; i < particleCount; i++) {
        float alpha = particles[i].life / particles[i].maxLife;
        int particleColor = (particles[i].color & 0x00FFFFFF) | ((int)(alpha * 255) << 24);
        vita2d_draw_rectangle(particles[i].x - 2, particles[i].y - 2, 4, 4, particleColor);
    }
}

void drawProgressBar(vita2d_pgf *font, int percent) {
    int barWidth = 500;
    int barHeight = 40;
    int x = 480 - (barWidth / 2);
    int y = 272 - (barHeight / 2);

    for (int row = 0; row < 544; row += 8) {
        int r = 10 + (row * 10) / 544;
        int g = 15 + (row * 15) / 544;
        int b = 30 + (row * 25) / 544;
        vita2d_draw_rectangle(0, row, 960, 8, RGBA(r, g, b, 255));
    }

    vita2d_draw_rectangle(x - 25, y - 85, barWidth + 50, barHeight + 170, RGBA(100, 200, 255, 50));
    vita2d_draw_rectangle(x - 24, y - 84, barWidth + 48, barHeight + 168, RGBA(18, 22, 40, 235));

    vita2d_pgf_draw_text(font, 480 - 100, 100, RGBA(0, 0, 0, 150), 1.6f, "PSV Cleaner");
    vita2d_pgf_draw_text(font, 480 - 102, 98, RGBA(100, 200, 255, 100), 1.6f, "PSV Cleaner");
    vita2d_pgf_draw_text(font, 480 - 100, 100, RGBA(255, 255, 255, 255), 1.6f, "PSV Cleaner");

    static int textAnim = 0;
    textAnim = (textAnim + 1) % 40;
    int textOffset = (textAnim < 20 ? textAnim : 40 - textAnim) / 4;

    if (isEmergencyStopRequested()) {
        vita2d_pgf_draw_text(font, 480 - 140, y - 60 + textOffset, RGBA(255, 100, 100, 255), 1.2f, "Stopping operation...");
    } else {
        vita2d_pgf_draw_text(font, 480 - 150, y - 60 + textOffset, RGBA(100, 220, 255, 255), 1.2f, "Cleaning in progress...");
    }

    vita2d_draw_rectangle(x - 2, y - 2, barWidth + 4, barHeight + 4, RGBA(60, 80, 110, 255));
    vita2d_draw_rectangle(x, y, barWidth, barHeight, RGBA(10, 15, 25, 255));

    int filled = (barWidth * percent) / 100;
    if (filled > 0) {
        for (int i = 0; i < filled; i++) {
            int r = 0 + (i * 50) / barWidth;
            int g = 140 + (i * 115) / barWidth;
            int b = 255 - (i * 155) / barWidth;
            vita2d_draw_rectangle(x + i, y, 1, barHeight, RGBA(r, g, b, 255));
        }

        vita2d_draw_rectangle(x, y, filled, barHeight / 2, RGBA(255, 255, 255, 30));

        if (filled > 4) {
            vita2d_draw_rectangle(x + filled - 4, y, 4, barHeight, RGBA(255, 255, 255, 255));
            vita2d_draw_rectangle(x + filled - 6, y - 1, 8, barHeight + 2, RGBA(100, 255, 255, 120));
        }

        for (int i = 0; i < 2; i++) {
            addParticle(x + filled - 5 + (rand() % 10), y + barHeight / 2 + (rand() % 20 - 10));
        }
    }

    updateParticles();
    drawParticles();

    lastPercent = percent;

    static int pulseCounter = 0;
    pulseCounter = (pulseCounter + 1) % 30;
    float pulseScale = 1.0f + (pulseCounter < 15 ? pulseCounter * 0.02f : (30 - pulseCounter) * 0.02f);

    char text[32];
    snprintf(text, sizeof(text), "%d%%", percent);
    vita2d_pgf_draw_text(font, 480 - 30, y + 25, RGBA(0, 0, 0, 150), 1.4f * pulseScale, text);
    vita2d_pgf_draw_text(font, 480 - 30, y + 25, RGBA(255, 255, 255, 255), 1.4f * pulseScale, text);

    static int statusAnim = 0;
    statusAnim = (statusAnim + 1) % 120;

    if (percent < 30) {
        int blue = 200 + (statusAnim % 30);
        vita2d_pgf_draw_text(font, 480 - 80, y + 60, RGBA(100, 200, blue, 255), 1.0f, "Scanning files...");
    } else if (percent < 70) {
        int green = 200 + (statusAnim % 30);
        vita2d_pgf_draw_text(font, 480 - 100, y + 60, RGBA(100, green, 100, 255), 1.0f, "Deleting temporary files...");
    } else {
        int red = 200 + (statusAnim % 30);
        vita2d_pgf_draw_text(font, 480 - 80, y + 60, RGBA(red, 200, 100, 255), 1.0f, "Finalizing cleanup...");
    }

    vita2d_draw_rectangle(0, 500, 960, 44, RGBA(18, 22, 35, 240));
    vita2d_draw_rectangle(0, 500, 960, 2, RGBA(100, 200, 255, 120));

    if (isEmergencyStopRequested()) {
        static int emergencyPulse = 0;
        emergencyPulse = (emergencyPulse + 1) % 20;
        int emergencyAlpha = 200 + (emergencyPulse < 10 ? emergencyPulse * 5 : (20 - emergencyPulse) * 5);
        vita2d_pgf_draw_text(font, 30, 525, RGBA(255, 100, 100, emergencyAlpha), 0.9f, "EMERGENCY STOP REQUESTED - Please wait...");
    } else {
        static int buttonAnim = 0;
        buttonAnim = (buttonAnim + 1) % 40;
        int buttonScale = 25 + (buttonAnim < 20 ? buttonAnim / 2 : (40 - buttonAnim) / 2);

        vita2d_draw_rectangle(30, 510, buttonScale, buttonScale, RGBA(255, 100, 100, 200));
        vita2d_pgf_draw_text(font, 35, 528, RGBA(255, 255, 255, 255), 1.0f, "O");
        vita2d_pgf_draw_text(font, 65, 528, RGBA(255, 255, 255, 255), 0.9f, "Emergency Stop");

        static int instrAnim = 0;
        instrAnim = (instrAnim + 1) % 60;
        int instrColor = 200 + (instrAnim < 30 ? instrAnim / 3 : (60 - instrAnim) / 3);
        vita2d_pgf_draw_text(font, 220, 528, RGBA(instrColor, instrColor, 200, 255), 0.8f, "Press O during cleaning to stop safely");
    }

    static int borderAnim = 0;
    borderAnim = (borderAnim + 1) % 100;
    int borderColor = RGBA(0, 200 + borderAnim / 2, 0, 255);
    vita2d_draw_rectangle(0, 0, 960, 1, RGBA(50, 255, 50, 100));
    vita2d_draw_rectangle(0, 1, 960, 1, RGBA(25, 225, 25, 150));
    vita2d_draw_rectangle(0, 0, 960, 2, borderColor);
    vita2d_draw_rectangle(0, 542, 960, 1, RGBA(50, 255, 50, 100));
    vita2d_draw_rectangle(0, 543, 960, 1, RGBA(25, 225, 25, 150));
    vita2d_draw_rectangle(0, 542, 960, 2, borderColor);
}

void showNotification(const char *title, const char *message) {
    printf("NOTIFICATION: %s - %s\n", title, message);
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

void drawButtonHint(vita2d_pgf *font, int x, int y, const char *buttonText, const char *actionText) {
    int btnColor = RGBA(80, 120, 180, 255);
    int txtColor = RGBA(255, 255, 255, 255);
    
    if (strcmp(buttonText, "X") == 0) {
        btnColor = RGBA(40, 100, 220, 255);
    } else if (strcmp(buttonText, "O") == 0) {
        btnColor = RGBA(220, 40, 40, 255);
    } else if (strcmp(buttonText, "T") == 0 || strcmp(buttonText, "△") == 0) {
        btnColor = RGBA(20, 180, 80, 255);
        buttonText = "△";
    } else if (strcmp(buttonText, "S") == 0 || strcmp(buttonText, "□") == 0 || strcmp(buttonText, "■") == 0) {
        btnColor = RGBA(180, 40, 150, 255);
        buttonText = "□";
    } else if (strcmp(buttonText, "↕") == 0 || strcmp(buttonText, "↔") == 0) {
        btnColor = RGBA(50, 50, 70, 255);
    } else if (strcmp(buttonText, "SEL") == 0 || strcmp(buttonText, "SELECT") == 0) {
        btnColor = RGBA(60, 65, 80, 255);
    }

    int btnWidth = 22;
    if (strcmp(buttonText, "SEL") == 0) {
        btnWidth = 32;
    } else if (strcmp(buttonText, "SELECT") == 0) {
        btnWidth = 48;
    }

    vita2d_draw_rectangle(x - 1, y - 11, btnWidth + 2, 24, RGBA(100, 200, 255, 100));
    vita2d_draw_rectangle(x, y - 10, btnWidth, 22, btnColor);

    int textOffset = 5;
    float textScale = 1.0f;
    if (strcmp(buttonText, "X") == 0) textOffset = 5;
    else if (strcmp(buttonText, "O") == 0) textOffset = 4;
    else if (strcmp(buttonText, "△") == 0) textOffset = 3;
    else if (strcmp(buttonText, "□") == 0) textOffset = 3;
    else if (strcmp(buttonText, "↕") == 0) textOffset = 5;
    else {
        textOffset = 4;
        textScale = 0.7f;
    }

    vita2d_pgf_draw_text(font, x + textOffset, y + 6, txtColor, textScale, buttonText);
    vita2d_pgf_draw_text(font, x + btnWidth + 10, y + 6, RGBA(220, 220, 220, 255), 0.9f, actionText);
}

void drawMainControlBar(vita2d_pgf *font, int selected_profile, int min_profile, int max_profile) {
    vita2d_draw_rectangle(0, 500, 960, 44, RGBA(18, 22, 35, 240));
    vita2d_draw_rectangle(0, 500, 960, 2, RGBA(100, 200, 255, 120));

    const char* profile_names[] = {"Quick Clean", "Complete Clean", "Selective Clean"};
    char hint[64];
    safe_snprintf(hint, sizeof(hint), "Selected: %s", profile_names[selected_profile]);
    vita2d_pgf_draw_text(font, 30, 528, RGBA(255, 255, 255, 255), 0.9f, hint);

    drawButtonHint(font, 520, 528, "↕", "Navigate");
    drawButtonHint(font, 680, 528, "X", "Confirm");
    drawButtonHint(font, 820, 528, "O", "Exit");
}

void drawDeleteConfirmation(vita2d_pgf *font, PreviewState *preview) {
    vita2d_draw_rectangle(0, 0, 960, 544, RGBA(0, 0, 0, 180));

    vita2d_draw_rectangle(200, 180, 560, 200, RGBA(20, 30, 50, 250));
    vita2d_draw_rectangle(205, 185, 550, 190, RGBA(30, 40, 60, 255));

    vita2d_pgf_draw_text(font, 480 - 120, 220, RGBA(255, 255, 255, 255), 1.4f, "Delete File Confirmation");

    char filename[80];
    if (preview->fileList && preview->selectedFile >= 0 && preview->selectedFile < preview->fileList->count) {
        char *path = preview->fileList->files[preview->selectedFile].path;
        char *basename = strrchr(path, '/');
        if (basename) {
            basename++;
        } else {
            basename = path;
        }
        if (strlen(basename) > 70) {
            strncpy(filename, basename, 67);
            filename[67] = '.';
            filename[68] = '.';
            filename[69] = '.';
            filename[70] = '\0';
        } else {
            strcpy(filename, basename);
        }
    }

    vita2d_pgf_draw_text(font, 480 - 200, 260, RGBA(255, 255, 100, 255), 1.0f, "Are you sure you want to delete this file?");

    vita2d_draw_rectangle(220, 290, 520, 2, RGBA(100, 150, 200, 255));

    vita2d_pgf_draw_text(font, 240, 320, RGBA(200, 200, 200, 255), 0.9f, filename);

    char sizeText[32];
    if (preview->fileList && preview->selectedFile >= 0 && preview->selectedFile < preview->fileList->count) {
        unsigned long long size = preview->fileList->files[preview->selectedFile].size;
        if (size < 1024)
            snprintf(sizeText, sizeof(sizeText), "%llu bytes", size);
        else if (size < (1024 * 1024))
            snprintf(sizeText, sizeof(sizeText), "%.1f KB", size / 1024.0);
        else if (size < (1024ULL * 1024 * 1024))
            snprintf(sizeText, sizeof(sizeText), "%.1f MB", size / (1024.0 * 1024));
        else
            snprintf(sizeText, sizeof(sizeText), "%.1f GB", size / (1024.0 * 1024 * 1024));
    }

    vita2d_pgf_draw_text(font, 240, 350, RGBA(150, 200, 255, 255), 0.9f, sizeText);

    vita2d_draw_rectangle(0, 500, 960, 44, RGBA(18, 22, 35, 240));
    vita2d_draw_rectangle(0, 500, 960, 2, RGBA(100, 200, 255, 120));
    drawButtonHint(font, 30, 528, "X", "Delete File");
    drawButtonHint(font, 190, 528, "O", "Cancel");
}

void drawCleanAllConfirmation(vita2d_pgf *font, FileList *fileList) {
    vita2d_draw_rectangle(0, 0, 960, 544, RGBA(0, 0, 0, 180));

    vita2d_draw_rectangle(200, 180, 560, 200, RGBA(20, 30, 50, 250));
    vita2d_draw_rectangle(205, 185, 550, 190, RGBA(30, 40, 60, 255));

    vita2d_pgf_draw_text(font, 480 - 100, 220, RGBA(255, 255, 255, 255), 1.4f, "Clean All Files");

    vita2d_pgf_draw_text(font, 480 - 200, 260, RGBA(255, 255, 100, 255), 1.0f, "Are you sure you want to clean all files?");

    vita2d_draw_rectangle(220, 290, 520, 2, RGBA(100, 150, 200, 255));

    char filesCountText[64];
    snprintf(filesCountText, sizeof(filesCountText), "Total files to delete: %d", fileList ? fileList->count : 0);
    vita2d_pgf_draw_text(font, 240, 320, RGBA(200, 200, 200, 255), 0.9f, filesCountText);

    vita2d_draw_rectangle(0, 500, 960, 44, RGBA(18, 22, 35, 240));
    vita2d_draw_rectangle(0, 500, 960, 2, RGBA(100, 200, 255, 120));
    drawButtonHint(font, 30, 528, "X", "Clean All");
    drawButtonHint(font, 190, 528, "O", "Cancel");
}

typedef enum {
    PROFILE_QUICK = 0,
    PROFILE_COMPLETE,
    PROFILE_SELECTIVE
} CleaningProfile;

void syncMenuToGlobals(MenuOptions *menu) {
    cleanSystem = menu->enabled[0];
    cleanVitaShell = menu->enabled[1];
    cleanRetroArch = menu->enabled[3];
    cleanAdrenaline = menu->enabled[5];
    
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

const char* getOptionDescription(int index) {
    switch(index) {
        case 0:  return "Deletes temporary files created by the operating system.";
        case 1:  return "Cleans VitaShell icons, logs, and temporary backups.";
        case 2:  return "Removes images and data cached by PKGi/PKGj.";
        case 3:  return "Empties RetroArch shader cache and log files.";
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
    return 0;
}

void initMenuOptions(MenuOptions *menu, CleaningProfile profile) {
    menu->selected = 0;
    menu->scrollOffset = 0;
    menu->total_options = TOTAL_CATEGORIES;

    strcpy(menu->options[0], "System Temp Files");
    strcpy(menu->options[1], "VitaShell Cache");
    strcpy(menu->options[2], "PKGi Cache");
    strcpy(menu->options[3], "RetroArch Cache");
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
            menu->enabled[1] = 1;  // VitaShell
            menu->enabled[2] = 1;  // PKGi
            menu->enabled[3] = 1;  // RetroArch
            menu->enabled[4] = 1;  // Autoplugin
            menu->enabled[5] = 1;  // Adrenaline
            menu->enabled[11] = 0; // Orphaned data off by default
            menu->enabled[12] = 0; // All apps temp off
            for (int i = 13; i <= 21; i++) menu->enabled[i] = 1;
            for (int i = 22; i <= 26; i++) menu->enabled[i] = 0;
            break;

        case PROFILE_COMPLETE:
            for (int i = 0; i < TOTAL_CATEGORIES; i++) {
                menu->enabled[i] = 1;
            }
            menu->enabled[8] = 1; 
            menu->enabled[35] = 1; 
            menu->enabled[9] = 0;
            menu->enabled[10] = 0;
            break;

        case PROFILE_SELECTIVE:
            for (int i = 0; i < TOTAL_CATEGORIES; i++) {
                menu->enabled[i] = 0;
            }
            menu->enabled[7] = 1;  // Exclusion settings header always visible
            break;
    }

    syncMenuToGlobals(menu);
}

void drawOptionsMenu(vita2d_pgf *font, MenuOptions *menu) {
    vita2d_draw_rectangle(0, 0, 960, 544, RGBA(5, 10, 20, 220));

    vita2d_draw_rectangle(80, 40, 800, 460, RGBA(100, 200, 255, 60));
    vita2d_draw_rectangle(82, 42, 796, 456, RGBA(18, 22, 40, 240));

    vita2d_draw_rectangle(82, 42, 796, 53, RGBA(25, 30, 55, 255));
    vita2d_draw_rectangle(82, 95, 796, 2, RGBA(0, 220, 255, 255));
    vita2d_pgf_draw_text(font, 480 - 165, 80, RGBA(255, 255, 255, 255), 1.6f, "Advanced Configuration");

    vita2d_pgf_draw_text(font, 480 - 180, 125, RGBA(150, 200, 255, 255), 0.9f, "Use X to toggle specific categories");

    vita2d_draw_rectangle(100, 140, 760, 260, RGBA(10, 15, 30, 255));

    int maxVisible = 7;
    int startIdx = menu->scrollOffset;
    int endIdx = startIdx + maxVisible;
    if (endIdx > menu->total_options) endIdx = menu->total_options;

    for (int i = startIdx; i < endIdx; i++) {
        int displayIndex = i - startIdx;
        int y = 145 + (displayIndex * 34);
        int isSelected = (i == menu->selected);
        int isAnHeader = isHeader(i);

        if (isSelected) {
            vita2d_draw_rectangle(105, y - 5, 750, 32, RGBA(30, 60, 110, 180));
            vita2d_draw_rectangle(105, y - 5, 4, 32, RGBA(0, 255, 255, 255));
        }

        if (i != 7) {
            int checkboxX = 120;
            int checkboxY = y + 3;

            vita2d_draw_rectangle(checkboxX - 1, checkboxY - 1, 22, 22, menu->enabled[i] ? RGBA(0, 255, 150, 180) : RGBA(180, 50, 50, 150));
            vita2d_draw_rectangle(checkboxX, checkboxY, 20, 20, RGBA(15, 20, 35, 255));

            if (menu->enabled[i]) {
                vita2d_draw_rectangle(checkboxX + 4, checkboxY + 4, 12, 12, RGBA(0, 255, 150, 255));
            } else {
                vita2d_draw_rectangle(checkboxX + 4, checkboxY + 4, 12, 12, RGBA(180, 50, 50, 255));
            }
        }

        int textColor = isSelected ? RGBA(255, 255, 255, 255) : RGBA(180, 180, 180, 255);
        if (isAnHeader) textColor = RGBA(255, 255, 100, 255);
        
        vita2d_pgf_draw_text(font, 155, y + 18, textColor, 1.0f, menu->options[i]);
    }

    vita2d_draw_rectangle(100, 410, 760, 55, RGBA(15, 25, 45, 255));
    vita2d_draw_rectangle(100, 410, 760, 2, RGBA(0, 150, 255, 255));
    vita2d_pgf_draw_text(font, 115, 435, RGBA(200, 200, 200, 255), 0.85f, "Details:");
    vita2d_pgf_draw_text(font, 115, 458, RGBA(255, 255, 255, 255), 0.85f, getOptionDescription(menu->selected));

    if (menu->total_options > maxVisible) {
        int scrollbarHeight = 260;
        int scrollbarY = 140;
        int thumbHeight = (maxVisible * scrollbarHeight) / menu->total_options;
        int thumbY = scrollbarY + (menu->scrollOffset * scrollbarHeight) / menu->total_options;

        vita2d_draw_rectangle(862, scrollbarY, 12, scrollbarHeight, RGBA(5, 10, 20, 255));
        vita2d_draw_rectangle(862, thumbY, 12, thumbHeight, RGBA(0, 255, 255, 255));
        vita2d_draw_rectangle(862, thumbY, 2, thumbHeight, RGBA(255, 255, 255, 120));
    }

    vita2d_draw_rectangle(100, 475, 760, 30, RGBA(18, 22, 35, 240));
    vita2d_draw_rectangle(100, 475, 760, 2, RGBA(0, 220, 255, 120));

    int instrY = 492;

    drawButtonHint(font, 125, instrY, "↕", "Navigate");
    drawButtonHint(font, 285, instrY, "X", "Toggle");
    drawButtonHint(font, 445, instrY, "O", "Preview");
    drawButtonHint(font, 645, instrY, "△", "Back");
}

void drawPreviewScreen(vita2d_pgf *font, PreviewState *preview) {
    vita2d_draw_rectangle(0, 0, 960, 544, RGBA(15, 25, 40, 255));

    vita2d_pgf_draw_text(font, 480 - 190, 40, RGBA(255, 255, 255, 255), 1.8f, "Preview - Files to Delete");

    if (preview->fileList && preview->fileList->count > 0) {
        char sortText[64];
        const char* sortLabels[] = {"Name", "Size"};

        snprintf(sortText, sizeof(sortText), "Sort: %s | Filter: %s",
                sortLabels[preview->sortMode],
                strlen(preview->fileFilter) > 0 ? preview->fileFilter : "All");

        vita2d_pgf_draw_text(font, 30, 70, RGBA(150, 200, 255, 255), 0.9f, sortText);

        char totalText[128];
        if (preview->totalVisibleSize < (1024 * 1024))
            snprintf(totalText, sizeof(totalText), "Files: %d | Total: %llu KB",
                    preview->fileList->count, preview->totalVisibleSize / 1024);
        else if (preview->totalVisibleSize < (1024ULL * 1024 * 1024))
            snprintf(totalText, sizeof(totalText), "Files: %d | Total: %.2f MB",
                    preview->fileList->count, preview->totalVisibleSize / (1024.0 * 1024));
        else
            snprintf(totalText, sizeof(totalText), "Files: %d | Total: %.2f GB",
                    preview->fileList->count, preview->totalVisibleSize / (1024.0 * 1024 * 1024));

        vita2d_pgf_draw_text(font, 30, 90, RGBA(100, 255, 100, 255), 1.0f, totalText);

        int maxVisible = 16;
        int startIdx = preview->scrollOffset;
        int endIdx = startIdx + maxVisible;
        if (endIdx > preview->fileList->count) endIdx = preview->fileList->count;

        for (int i = startIdx; i < endIdx; i++) {
            int y = 120 + ((i - startIdx) * 22);
            int color = (i == preview->selectedFile) ? RGBA(255, 255, 100, 255) : RGBA(200, 200, 200, 255);

            if (i == preview->selectedFile) {
                vita2d_draw_rectangle(20, y - 3, 920, 20, RGBA(50, 100, 150, 100));
            }

            char displayPath[80];
            if (strlen(preview->fileList->files[i].path) > 75) {
                strncpy(displayPath, preview->fileList->files[i].path, 72);
                displayPath[72] = '.';
                displayPath[73] = '.';
                displayPath[74] = '.';
                displayPath[75] = '\0';
            } else {
                strcpy(displayPath, preview->fileList->files[i].path);
            }

            char sizeText[32];
            unsigned long long size = preview->fileList->files[i].size;
            if (size < 1024)
                snprintf(sizeText, sizeof(sizeText), "%llu B", size);
            else if (size < (1024 * 1024))
                snprintf(sizeText, sizeof(sizeText), "%llu KB", size / 1024);
            else if (size < (1024ULL * 1024 * 1024))
                snprintf(sizeText, sizeof(sizeText), "%.2f MB", size / (1024.0 * 1024));
            else
                snprintf(sizeText, sizeof(sizeText), "%.2f GB", size / (1024.0 * 1024 * 1024));

            vita2d_pgf_draw_text(font, 25, y + 12, color, 0.7f, displayPath);
            vita2d_pgf_draw_text(font, 850, y + 12, color, 0.7f, sizeText);
        }

        if (preview->fileList->count > maxVisible) {
            int scrollbarHeight = 350;
            int scrollbarY = 120;
            int thumbHeight = (maxVisible * scrollbarHeight) / preview->fileList->count;
            int thumbY = scrollbarY + (preview->scrollOffset * scrollbarHeight) / preview->fileList->count;

            vita2d_draw_rectangle(945, scrollbarY, 10, scrollbarHeight, RGBA(40, 40, 40, 255));
            vita2d_draw_rectangle(945, thumbY, 10, thumbHeight, RGBA(100, 150, 200, 255));
        }
    } else {
        vita2d_pgf_draw_text(font, 480 - 100, 272, RGBA(255, 255, 100, 255), 1.2f, "No temporary files found!");
    }

    vita2d_draw_rectangle(0, 500, 960, 44, RGBA(18, 22, 35, 240));
    vita2d_draw_rectangle(0, 500, 960, 2, RGBA(100, 200, 255, 120));
    if (!preview->fileList || preview->fileList->count == 0) {
        drawButtonHint(font, 30, 528, "O", "Cancel");
    } else {
        drawButtonHint(font, 30, 528, "↕", "Navigate");
        drawButtonHint(font, 180, 528, "△", "Sort");
        drawButtonHint(font, 290, 528, "□", "Filter");
        drawButtonHint(font, 410, 528, "SEL", "Delete");
        drawButtonHint(font, 540, 528, "X", "Clean All");
        drawButtonHint(font, 690, 528, "O", "Cancel");
    }
}

void drawAppListScreen(vita2d_pgf *font, AppListState *appState) {
    vita2d_draw_rectangle(0, 0, 960, 544, RGBA(15, 25, 40, 255));

    vita2d_pgf_draw_text(font, 480 - 140, 40, RGBA(255, 255, 255, 255), 1.8f, "Select App to Clean");

    if (appState->appList && appState->appList->count > 0) {
        char totalText[128];
        unsigned long long totalSize = 0;
        for (int i = 0; i < appState->appList->count; i++) {
            totalSize += appState->appList->apps[i].tempSize;
        }

        if (totalSize < (1024 * 1024))
            snprintf(totalText, sizeof(totalText), "Apps: %d | Total Temp Files: %llu KB",
                    appState->appList->count, totalSize / 1024);
        else if (totalSize < (1024ULL * 1024 * 1024))
            snprintf(totalText, sizeof(totalText), "Apps: %d | Total Temp Files: %.2f MB",
                    appState->appList->count, totalSize / (1024.0 * 1024));
        else
            snprintf(totalText, sizeof(totalText), "Apps: %d | Total Temp Files: %.2f GB",
                    appState->appList->count, totalSize / (1024.0 * 1024 * 1024));

        vita2d_pgf_draw_text(font, 30, 70, RGBA(100, 255, 100, 255), 1.0f, totalText);

        int maxVisible = 18;
        int startIdx = appState->scrollOffset;
        int endIdx = startIdx + maxVisible;
        if (endIdx > appState->appList->count) endIdx = appState->appList->count;

        for (int i = startIdx; i < endIdx; i++) {
            int y = 100 + ((i - startIdx) * 22);
            int color = (i == appState->selectedApp) ? RGBA(255, 255, 100, 255) : RGBA(200, 200, 200, 255);

            if (i == appState->selectedApp) {
                vita2d_draw_rectangle(20, y - 3, 920, 20, RGBA(50, 100, 150, 100));
            }

            char titleIdText[16];
            safe_strncpy(titleIdText, appState->appList->apps[i].titleId, sizeof(titleIdText));

            char sizeText[32];
            unsigned long long size = appState->appList->apps[i].tempSize;
            if (size == 0) {
                strcpy(sizeText, "0 B");
            } else if (size < 1024) {
                snprintf(sizeText, sizeof(sizeText), "%llu B", size);
            } else if (size < (1024 * 1024)) {
                snprintf(sizeText, sizeof(sizeText), "%llu KB", size / 1024);
            } else if (size < (1024ULL * 1024 * 1024)) {
                snprintf(sizeText, sizeof(sizeText), "%.2f MB", size / (1024.0 * 1024));
            } else {
                snprintf(sizeText, sizeof(sizeText), "%.2f GB", size / (1024.0 * 1024 * 1024));
            }

            vita2d_pgf_draw_text(font, 25, y + 12, color, 0.8f, titleIdText);
            vita2d_pgf_draw_text(font, 850, y + 12, color, 0.8f, sizeText);
        }

        if (appState->appList->count > maxVisible) {
            int scrollbarHeight = 395;
            int scrollbarY = 100;
            int thumbHeight = (maxVisible * scrollbarHeight) / appState->appList->count;
            int thumbY = scrollbarY + (appState->scrollOffset * scrollbarHeight) / appState->appList->count;

            vita2d_draw_rectangle(945, scrollbarY, 10, scrollbarHeight, RGBA(40, 40, 40, 255));
            vita2d_draw_rectangle(945, thumbY, 10, thumbHeight, RGBA(100, 150, 200, 255));
        }
    } else {
        vita2d_pgf_draw_text(font, 480 - 120, 272, RGBA(255, 255, 100, 255), 1.2f, "Scanning apps...");
    }

    vita2d_draw_rectangle(0, 500, 960, 44, RGBA(18, 22, 35, 240));
    vita2d_draw_rectangle(0, 500, 960, 2, RGBA(100, 200, 255, 120));
    if (!appState->appList || appState->appList->count == 0) {
        drawButtonHint(font, 30, 528, "O", "Cancel");
    } else {
        drawButtonHint(font, 30, 528, "↕", "Navigate");
        drawButtonHint(font, 190, 528, "X", "Clean Selected");
        drawButtonHint(font, 390, 528, "O", "Cancel");
    }
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

    initParticles();

    initEmergencyStop();

    detectSystemLanguage();

    // Initialize space cache
    g_cachedSpaceSize = 0;
    g_spaceCalculationNeeded = 1;
    g_lastCalculationFrame = 0;
    
    unsigned long long spaceBefore = calculateTempSizeOptimized();
    char freeText[128];
    if (spaceBefore == 0) {
        strcpy(freeText, "Space to free: 0 KB");
    } else if (spaceBefore < (1024 * 1024))
        snprintf(freeText, sizeof(freeText), "Space to free: %llu KB", spaceBefore / 1024);
    else if (spaceBefore < (1024ULL * 1024 * 1024))
        snprintf(freeText, sizeof(freeText), "Space to free: %.2f MB", spaceBefore / (1024.0 * 1024));
    else
        snprintf(freeText, sizeof(freeText), "Space to free: %.2f GB", spaceBefore / (1024.0 * 1024 * 1024));

    MenuOptions menu;
    initMenuOptions(&menu, PROFILE_COMPLETE);

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

    int showMenu = 0;
    int showPreview = 0;
    int showDeleteConfirmation = 0;
    int showCleanAllConfirmation = 0;
    int showProfileSelect = 1;
    CleaningProfile selectedProfile = PROFILE_COMPLETE;
    int running = 1;
    int currentFrame = 0;
    
    while (running) {
        sceCtrlPeekBufferPositive(0, &pad, 1);
        currentFrame++;

        if (!showMenu && !showPreview) {
            // Only update space cache every 120 frames (2 seconds) to reduce overhead
            if (currentFrame % 120 == 0) {
                updateSpaceCacheIfNeeded(currentFrame);
            }
            unsigned long long currentSpace = g_cachedSpaceSize;
            if (currentSpace != spaceBefore || spaceBefore == 0) {
                spaceBefore = currentSpace;
                if (spaceBefore == 0) {
                    strcpy(freeText, "Space to free: 0 KB");
                } else if (spaceBefore < (1024 * 1024))
                    snprintf(freeText, sizeof(freeText), "Space to free: %llu KB", spaceBefore / 1024);
                else if (spaceBefore < (1024ULL * 1024 * 1024))
                    snprintf(freeText, sizeof(freeText), "Space to free: %.2f MB", spaceBefore / (1024.0 * 1024));
                else
                    snprintf(freeText, sizeof(freeText), "Space to free: %.2f GB", spaceBefore / (1024.0 * 1024 * 1024));
            }
        }

        // Safe rendering with fallback
        vita2d_start_drawing();
        vita2d_clear_screen();

    for (int row = 0; row < 544; row += 8) {
        int r = 10 + (row * 10) / 544;
        int g = 15 + (row * 15) / 544;
        int b = 30 + (row * 25) / 544;
        vita2d_draw_rectangle(0, row, 960, 8, RGBA(r, g, b, 255));
    }

    vita2d_draw_rectangle(150, 25, 660, 86, RGBA(100, 200, 255, 60));
    vita2d_draw_rectangle(151, 26, 658, 84, RGBA(18, 22, 40, 230));

    vita2d_draw_rectangle(151, 26, 658, 3, RGBA(0, 220, 255, 255));

    vita2d_pgf_draw_text(font, 480 - 130, 67, RGBA(0, 0, 0, 150), 2.2f, "PSV Cleaner");
    vita2d_pgf_draw_text(font, 480 - 132, 65, RGBA(100, 200, 255, 255), 2.2f, "PSV Cleaner");
    vita2d_pgf_draw_text(font, 480 - 130, 67, RGBA(255, 255, 255, 255), 2.2f, "PSV Cleaner");

    vita2d_pgf_draw_text(font, 480 - 155, 98, RGBA(180, 220, 255, 255), 0.9f, "Temporary Files Cleaner for PS Vita");

        if (showProfileSelect) {
            vita2d_pgf_draw_text(font, 480 - 150, 158, RGBA(255, 255, 255, 255), 1.6f, "Select Cleaning Profile");

            const char* profiles[3] = {
                "Quick Clean - Safe cache files only",
                "Complete Clean - All temporary files",
                "Selective Clean - Choose what to clean"
            };

            for (int i = 0; i < 3; i++) {
                int y = 220 + (i * 80);
                int isSelected = (i == (int)selectedProfile);

                vita2d_draw_rectangle(150 - 1, y - 10 - 1, 660 + 2, 60 + 2, isSelected ? RGBA(0, 200, 255, 200) : RGBA(60, 80, 110, 100));
                vita2d_draw_rectangle(150, y - 10, 660, 60, isSelected ? RGBA(30, 45, 80, 240) : RGBA(20, 25, 45, 200));

                if (isSelected) {
                    vita2d_draw_rectangle(150, y - 10, 4, 60, RGBA(0, 255, 255, 255));
                }

                vita2d_pgf_draw_text(font, 180, y + 10, RGBA(255, 255, 255, 255), 1.2f, profiles[i]);

                if (i == 0) {
                    vita2d_pgf_draw_text(font, 180, y + 35, RGBA(180, 180, 200, 255), 0.8f, "Removes app caches, browser temp, logs (fast & safe)");
                } else if (i == 1) {
                    vita2d_pgf_draw_text(font, 180, y + 35, RGBA(180, 180, 200, 255), 0.8f, "Removes all temp files, crash dumps, system cache");
                } else {
                    vita2d_pgf_draw_text(font, 180, y + 35, RGBA(180, 180, 200, 255), 0.8f, "Manually choose which categories to clean");
                }
            }

            drawMainControlBar(font, selectedProfile, PROFILE_QUICK, PROFILE_SELECTIVE);
        } else if (showCleanAllConfirmation) {
            drawCleanAllConfirmation(font, preview.fileList);
        } else if (showDeleteConfirmation) {
            drawDeleteConfirmation(font, &preview);
        } else if (appState.showAppList) {
            drawAppListScreen(font, &appState);
        } else if (showPreview) {
            drawPreviewScreen(font, &preview);
        } else if (showMenu) {
            drawOptionsMenu(font, &menu);
        } else {
            vita2d_draw_rectangle(195, 175, 570, 260, RGBA(100, 200, 255, 60));
            vita2d_draw_rectangle(196, 176, 568, 258, RGBA(18, 22, 40, 230));

            vita2d_draw_rectangle(196, 176, 568, 3, RGBA(0, 220, 255, 255));

            for (int i = 0; i < 45; i++) {
                int alpha = 150 + (i * 50) / 45;
                if (alpha > 200) alpha = 200;
                vita2d_draw_rectangle(220, 210 + i, 50, 1, RGBA(0, 150 + i/3, 255 + i/5, alpha));
            }
            vita2d_draw_rectangle(225, 215, 40, 35, RGBA(40, 60, 80, 255));
            vita2d_draw_rectangle(230, 220, 30, 25, RGBA(0, 180, 255, 200));
            vita2d_draw_rectangle(220, 210, 50, 3, RGBA(100, 200, 255, 180));

            vita2d_pgf_draw_text(font, 290, 220, RGBA(100, 255, 255, 255), 1.0f, L(lang_ui_text, currentLanguage, 0));
            vita2d_pgf_draw_text(font, 290, 250, RGBA(255, 255, 100, 255), 1.4f, L(lang_ui_text, currentLanguage, 1));

            vita2d_draw_rectangle(220, 280, 520, 2, RGBA(0, 150, 255, 150));

            vita2d_pgf_draw_text(font, 220, 305, RGBA(100, 200, 255, 255), 1.2f, L(lang_ui_text, currentLanguage, 2));

            drawButtonHint(font, 240, 342, "□", L(lang_ui_text, currentLanguage, 3));
            drawButtonHint(font, 240, 367, "X", L(lang_ui_text, currentLanguage, 4));
            drawButtonHint(font, 240, 392, "△", L(lang_ui_text, currentLanguage, 5));
            drawButtonHint(font, 240, 417, "O", L(lang_ui_text, currentLanguage, 6));

            vita2d_draw_rectangle(0, 470, 960, 74, RGBA(18, 22, 35, 240));
            vita2d_draw_rectangle(0, 470, 960, 2, RGBA(100, 200, 255, 120));

            vita2d_pgf_draw_text(font, 30, 498, RGBA(150, 200, 255, 255), 0.9f, "Version 1.14");
            vita2d_pgf_draw_text(font, 30, 525, RGBA(180, 180, 180, 255), 0.8f, "Ready to clean temporary files and optimize your PS Vita");

            drawButtonHint(font, 560, 515, "X", "Preview & Clean");
            drawButtonHint(font, 760, 515, "SEL", "App List");
        }

        vita2d_draw_rectangle(0, 0, 960, 2, RGBA(0, 150, 255, 255));
        vita2d_draw_rectangle(0, 542, 960, 2, RGBA(0, 150, 255, 255));

        vita2d_end_drawing();
        vita2d_swap_buffers();

        gpuMemoryCleanup();

        if (showProfileSelect) {
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
                initMenuOptions(&menu, selectedProfile); // Inizializza il profilo scelto

                if (selectedProfile == PROFILE_SELECTIVE) {
                    // Selective Clean: vai direttamente alle opzioni avanzate
                    showMenu = 1;
                } else {
                    // Quick Clean e Complete Clean: vai alla Preview
                    showPreview = 1;
                    preview.scrollOffset = 0;
                    preview.selectedFile = 0;

                    vita2d_start_drawing();
                    vita2d_clear_screen();
                    vita2d_draw_rectangle(0, 0, 960, 544, RGBA(15, 20, 35, 255));
                    vita2d_pgf_draw_text(font, 480 - 100, 272, RGBA(255, 255, 100, 255), 1.3f, "Scanning files...");
                    vita2d_end_drawing();
                    vita2d_swap_buffers();

                    preview.fileList = createFileList();
                    if (preview.fileList) {
                        scanFilesForPreview(preview.fileList);
                        filterAndSortFileList(preview.fileList, preview.sortMode, preview.fileFilter, &preview.totalVisibleSize);
                    }
                }

                sceKernelDelayThread(200 * 1000);
            }
            if (pad.buttons & SCE_CTRL_CIRCLE) {
                running = 0;
            }
        } else if (showCleanAllConfirmation) {
            if (pad.buttons & SCE_CTRL_CROSS) {
                if (preview.fileList && preview.fileList->count > 0) {
                    showCleanAllConfirmation = 0;
                    showPreview = 0;

                    resetScanProgress();

                    g_draw_font = font;
                    g_progressCallback = updateCleanupProgress;

                    startOperation();

                    int cleaningInterrupted = 0;

                    cleanTemporaryFiles();

                    g_progressCallback = NULL;

                    if (isEmergencyStopRequested()) {
                        cleaningInterrupted = 1;
                    }

                    endOperation();

                    if (cleaningInterrupted) {
                        cleanupAfterEmergencyStop();

                        unsigned long long spaceFreedFromPreview = (preview.fileList ? preview.fileList->totalSize : 0);

                        int filesDeleted = getDeletedFilesCount();

                        spaceBefore = calculateTempSize();

                        char spaceText[128];
                        if (spaceFreedFromPreview < (1024 * 1024))
                            snprintf(spaceText, sizeof(spaceText), "Space Freed: %llu KB", spaceFreedFromPreview / 1024);
                        else if (spaceFreedFromPreview < (1024ULL * 1024 * 1024))
                            snprintf(spaceText, sizeof(spaceText), "Space Freed: %.2f MB", spaceFreedFromPreview / (1024.0 * 1024));
                        else
                            snprintf(spaceText, sizeof(spaceText), "Space Freed: %.2f GB", spaceFreedFromPreview / (1024.0 * 1024 * 1024));

                        vita2d_start_drawing();
                        vita2d_clear_screen();
                        vita2d_draw_rectangle(0, 0, 960, 544, RGBA(40, 40, 80, 255));

                        vita2d_pgf_draw_text(font, 480 - 130, 120, RGBA(255, 255, 255, 255), 2.0f, "OPERATION INTERRUPTED");

                        vita2d_pgf_draw_text(font, 480 - 180, 180, RGBA(255, 200, 200, 255), 1.2f, "Cleaning was safely interrupted by user request");
                        vita2d_pgf_draw_text(font, 480 - 160, 210, RGBA(255, 200, 200, 255), 0.9f, "Some temporary files may have been deleted");

                        vita2d_draw_rectangle(360, 260, 240, 40, RGBA(30, 80, 120, 180));
                        vita2d_draw_rectangle(360, 260, 240, 3, RGBA(100, 200, 255, 255));
                        vita2d_draw_rectangle(360, 297, 240, 3, RGBA(100, 200, 255, 255));
                        vita2d_pgf_draw_text(font, 480 - 105, 285, RGBA(255, 255, 255, 255), 1.2f, spaceText);

                        vita2d_draw_rectangle(360, 310, 240, 40, RGBA(30, 80, 120, 180));
                        vita2d_draw_rectangle(360, 310, 240, 3, RGBA(100, 200, 255, 255));
                        vita2d_draw_rectangle(360, 347, 240, 3, RGBA(100, 200, 255, 255));
                        char filesText[64];
                        snprintf(filesText, sizeof(filesText), "Files Deleted: %d", filesDeleted);
                        vita2d_pgf_draw_text(font, 480 - 105, 335, RGBA(255, 255, 255, 255), 1.2f, filesText);

                        vita2d_pgf_draw_text(font, 480 - 120, 400, RGBA(200, 200, 200, 255), 0.9f, "Returning to main menu...");

                        vita2d_end_drawing();
                        vita2d_swap_buffers();
                        sceKernelDelayThread(4 * 1000 * 1000);

                        if (preview.fileList) {
                            freeFileList(preview.fileList);
                            preview.fileList = NULL;
                        }
                        sceKernelDelayThread(200 * 1000);
                        continue;
                    }

                    if (running) {
                        int cleanupCount = loadCleanupCounter() + 1;
                        saveCleanupCounter(cleanupCount);

                        unsigned long long spaceFreedFromPreview = (preview.fileList ? preview.fileList->totalSize : 0);

                        int filesDeleted = getDeletedFilesCount();

                        spaceBefore = calculateTempSize();
                        if (spaceBefore < (1024 * 1024))
                            snprintf(freeText, sizeof(freeText), "Space to free: %llu KB", spaceBefore / 1024);
                        else if (spaceBefore < (1024ULL * 1024 * 1024))
                            snprintf(freeText, sizeof(freeText), "Space to free: %.2f MB", spaceBefore / (1024.0 * 1024));
                        else
                            snprintf(freeText, sizeof(freeText), "Space to free: %.2f GB", spaceBefore / (1024.0 * 1024 * 1024));

                        char spaceText[128];
                        if (spaceFreedFromPreview < (1024 * 1024))
                            snprintf(spaceText, sizeof(spaceText), "Space Freed: %llu KB", spaceFreedFromPreview / 1024);
                        else if (spaceFreedFromPreview < (1024ULL * 1024 * 1024))
                            snprintf(spaceText, sizeof(spaceText), "Space Freed: %.2f MB", spaceFreedFromPreview / (1024.0 * 1024));
                        else
                            snprintf(spaceText, sizeof(spaceText), "Space Freed: %.2f GB", spaceFreedFromPreview / (1024.0 * 1024 * 1024));

                        showNotification("PSV Cleaner", "Cleaning completed successfully!");

                        vita2d_start_drawing();
                        vita2d_clear_screen();

                        vita2d_draw_rectangle(0, 0, 960, 544, RGBA(15, 40, 25, 255));

                        vita2d_pgf_draw_text(font, 480 - 155, 78, RGBA(0, 0, 0, 200), 1.9f, "CLEANING COMPLETED!");
                        vita2d_pgf_draw_text(font, 480 - 157, 75, RGBA(100, 255, 100, 255), 1.9f, "CLEANING COMPLETED!");

                        vita2d_pgf_draw_text(font, 480 - 130, 250, RGBA(255, 255, 150, 255), 1.1f, "Great job! Your PS Vita is cleaner!");

                        vita2d_draw_rectangle(430, 270, 100, 25, RGBA(50, 100, 50, 200));
                        vita2d_draw_rectangle(430, 270, 100, 2, RGBA(0, 255, 100, 255));
                        char countText[64];
                        snprintf(countText, sizeof(countText), "Cleanup #%d", cleanupCount);
                        vita2d_pgf_draw_text(font, 480 - 45, 290, RGBA(255, 255, 200, 255), 1.0f, countText);

                        vita2d_draw_rectangle(360, 320, 240, 40, RGBA(30, 80, 120, 180));
                        vita2d_draw_rectangle(360, 320, 240, 3, RGBA(100, 200, 255, 255));
                        vita2d_draw_rectangle(360, 357, 240, 3, RGBA(100, 200, 255, 255));
                        vita2d_pgf_draw_text(font, 480 - 105, 345, RGBA(255, 255, 255, 255), 1.1f, spaceText);

                        vita2d_draw_rectangle(360, 370, 240, 40, RGBA(30, 80, 120, 180));
                        vita2d_draw_rectangle(360, 370, 240, 3, RGBA(100, 200, 255, 255));
                        vita2d_draw_rectangle(360, 407, 240, 3, RGBA(100, 200, 255, 255));
                        char filesText[64];
                        snprintf(filesText, sizeof(filesText), "Files Deleted: %d", filesDeleted);
                        vita2d_pgf_draw_text(font, 480 - 105, 395, RGBA(255, 255, 255, 255), 1.1f, filesText);

                        vita2d_pgf_draw_text(font, 480 - 100, 460, RGBA(200, 255, 200, 255), 0.9f, "Keep your PS Vita clean and healthy!");

                        vita2d_end_drawing();
                        vita2d_swap_buffers();
                        sceKernelDelayThread(3 * 1000 * 1000);
                    }

                    if (preview.fileList) {
                        freeFileList(preview.fileList);
                        preview.fileList = NULL;
                    }
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
                    if (appState.selectedApp >= appState.scrollOffset + 18) {
                        appState.scrollOffset = appState.selectedApp - 17;
                    }
                }
                sceKernelDelayThread(100 * 1000);
            }
            if (pad.buttons & SCE_CTRL_CROSS) {
                if (appState.appList && appState.appList->count > 0 && 
                    appState.selectedApp >= 0 && appState.selectedApp < appState.appList->count) {
                    
                    appState.showAppList = 0;
                    startOperation();

                    unsigned long long spaceFreed = cleanSingleAppTempFiles(
                        appState.appList->apps[appState.selectedApp].titleId);

                    endOperation();

                    int cleanupCount = loadCleanupCounter() + 1;
                    saveCleanupCounter(cleanupCount);

                    int filesDeleted = getDeletedFilesCount();
                    spaceBefore = calculateTempSize();

                    vita2d_start_drawing();
                    vita2d_clear_screen();
                    vita2d_draw_rectangle(0, 0, 960, 544, RGBA(15, 40, 25, 255));

                    vita2d_pgf_draw_text(font, 480 - 120, 78, RGBA(0, 0, 0, 200), 1.9f, "APP CLEANED!");
                    vita2d_pgf_draw_text(font, 480 - 122, 75, RGBA(100, 255, 100, 255), 1.9f, "APP CLEANED!");

                    char titleText[64];
                    safe_snprintf(titleText, sizeof(titleText), "App: %s", 
                        appState.appList->apps[appState.selectedApp].titleId);
                    vita2d_pgf_draw_text(font, 480 - 100, 200, RGBA(255, 255, 150, 255), 1.1f, titleText);

                    char spaceText[128];
                    if (spaceFreed < (1024 * 1024))
                        snprintf(spaceText, sizeof(spaceText), "Space Freed: %llu KB", spaceFreed / 1024);
                    else if (spaceFreed < (1024ULL * 1024 * 1024))
                        snprintf(spaceText, sizeof(spaceText), "Space Freed: %.2f MB", spaceFreed / (1024.0 * 1024));
                    else
                        snprintf(spaceText, sizeof(spaceText), "Space Freed: %.2f GB", spaceFreed / (1024.0 * 1024 * 1024));

                    vita2d_draw_rectangle(360, 250, 240, 40, RGBA(30, 80, 120, 180));
                    vita2d_draw_rectangle(360, 250, 240, 3, RGBA(100, 200, 255, 255));
                    vita2d_draw_rectangle(360, 287, 240, 3, RGBA(100, 200, 255, 255));
                    vita2d_pgf_draw_text(font, 480 - 105, 275, RGBA(255, 255, 255, 255), 1.1f, spaceText);

                    char filesText[64];
                    snprintf(filesText, sizeof(filesText), "Files Deleted: %d", filesDeleted);
                    vita2d_draw_rectangle(360, 300, 240, 40, RGBA(30, 80, 120, 180));
                    vita2d_draw_rectangle(360, 300, 240, 3, RGBA(100, 200, 255, 255));
                    vita2d_draw_rectangle(360, 337, 240, 3, RGBA(100, 200, 255, 255));
                    vita2d_pgf_draw_text(font, 480 - 105, 325, RGBA(255, 255, 255, 255), 1.1f, filesText);

                    vita2d_pgf_draw_text(font, 480 - 100, 400, RGBA(200, 255, 200, 255), 0.9f, "Returning to app list...");

                    vita2d_end_drawing();
                    vita2d_swap_buffers();
                    sceKernelDelayThread(2 * 1000 * 1000);

                    populateAppListWithSizes(appState.appList);
                    appState.showAppList = 1;
                    sceKernelDelayThread(200 * 1000);
                }
            }
            if (pad.buttons & SCE_CTRL_CIRCLE) {
                appState.showAppList = 0;
                if (appState.appList) {
                    freeAppList(appState.appList);
                    appState.appList = NULL;
                }
                sceKernelDelayThread(200 * 1000);
            }
        } else if (showPreview) {
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
        } else if (showMenu) {
            if (pad.buttons & SCE_CTRL_UP) {
                if (menu.selected > 0) {
                    menu.selected--;
                    if (menu.selected < menu.scrollOffset && menu.scrollOffset > 0) {
                        menu.scrollOffset = menu.selected;
                    }
                }
                
                if (menu.selected < 0) menu.selected = 0;
                if (menu.selected >= menu.total_options) menu.selected = menu.total_options - 1;
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
                
                if (menu.selected < 0) menu.selected = 0;
                if (menu.selected >= menu.total_options) menu.selected = menu.total_options - 1;
                sceKernelDelayThread(150 * 1000); 
            }
            if (pad.buttons & SCE_CTRL_CROSS) {
                if (menu.selected == 7) {
                    int targetState = !menu.enabled[0]; 
                    for (int i = 0; i < TOTAL_CATEGORIES; i++) {
                        if (i != 7) menu.enabled[i] = targetState;
                    }
                    syncMenuToGlobals(&menu);
                } else if (menu.selected >= 0 && menu.selected < TOTAL_CATEGORIES) {
                    menu.enabled[menu.selected] = !menu.enabled[menu.selected];
                    syncMenuToGlobals(&menu);
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
                sceKernelDelayThread(200 * 1000);
            }
            if (pad.buttons & SCE_CTRL_CIRCLE) {
                showPreview = 1;
                preview.scrollOffset = 0;
                preview.selectedFile = 0;

                vita2d_start_drawing();
                vita2d_clear_screen();
                vita2d_draw_rectangle(0, 0, 960, 544, RGBA(15, 25, 40, 255));
                vita2d_pgf_draw_text(font, 480 - 100, 272, RGBA(255, 255, 100, 255), 1.3f, "Scanning files...");
                vita2d_end_drawing();
                vita2d_swap_buffers();

                preview.fileList = createFileList();
                if (preview.fileList) {
                    scanFilesForPreview(preview.fileList);
                    filterAndSortFileList(preview.fileList, preview.sortMode, preview.fileFilter, &preview.totalVisibleSize);
                }

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

                if (!appState.appList) {
                    appState.appList = createAppList();
                }

                vita2d_start_drawing();
                vita2d_clear_screen();
                vita2d_draw_rectangle(0, 0, 960, 544, RGBA(15, 25, 40, 255));
                vita2d_pgf_draw_text(font, 480 - 120, 272, RGBA(255, 255, 100, 255), 1.3f, "Scanning apps...");
                vita2d_end_drawing();
                vita2d_swap_buffers();

                if (appState.appList) {
                    populateAppListWithSizes(appState.appList);
                }

                sceKernelDelayThread(200 * 1000);
            }
            if (pad.buttons & SCE_CTRL_CROSS) {
                showPreview = 1;
                preview.scrollOffset = 0;
                preview.selectedFile = 0;

                vita2d_start_drawing();
                vita2d_clear_screen();
                vita2d_draw_rectangle(0, 0, 960, 544, RGBA(15, 25, 40, 255));
                vita2d_pgf_draw_text(font, 480 - 100, 272, RGBA(255, 255, 100, 255), 1.3f, "Scanning files...");
                vita2d_end_drawing();
                vita2d_swap_buffers();

                preview.fileList = createFileList();
                if (preview.fileList) {
                    scanFilesForPreview(preview.fileList);
                    filterAndSortFileList(preview.fileList, preview.sortMode, preview.fileFilter, &preview.totalVisibleSize);
                }

                sceKernelDelayThread(200 * 1000);
            }

            if (pad.buttons & SCE_CTRL_CIRCLE) {
                running = 0;
            }
        }

        sceKernelDelayThread(16 * 1000);
    }

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