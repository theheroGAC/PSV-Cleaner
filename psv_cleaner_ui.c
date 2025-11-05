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
    particles[particleCount].vx = ((rand() % 100) - 50) * 0.1f;
    particles[particleCount].vy = -((rand() % 50) + 20) * 0.1f;
    particles[particleCount].life = 1.0f;
    particles[particleCount].maxLife = 1.0f;
    particles[particleCount].color = RGBA(100 + rand() % 155, 200 + rand() % 55, 255, 255);
    particleCount++;
}

void updateParticles() {
    for (int i = 0; i < particleCount; i++) {
        particles[i].x += particles[i].vx;
        particles[i].y += particles[i].vy;
        particles[i].vy += 0.1f;
        particles[i].life -= 0.02f;

        if (particles[i].life <= 0) {
            particles[i] = particles[--particleCount];
            i--;
        }
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

    static int bgPulse = 0;
    bgPulse = (bgPulse + 1) % 60;
    int bgIntensity = 40 + (bgPulse < 30 ? bgPulse : 60 - bgPulse);
    vita2d_draw_rectangle(0, 0, 960, 544, RGBA(15, 25, bgIntensity / 2, 255));

    vita2d_pgf_draw_text(font, 480 - 100, 100, RGBA(0, 0, 0, 150), 1.6f, "PSV Cleaner");
    vita2d_pgf_draw_text(font, 480 - 102, 98, RGBA(150, 200, 255, 100), 1.6f, "PSV Cleaner");
    vita2d_pgf_draw_text(font, 480 - 100, 100, RGBA(255, 255, 255, 255), 1.6f, "PSV Cleaner");

    static int textAnim = 0;
    textAnim = (textAnim + 1) % 40;
    int textOffset = (textAnim < 20 ? textAnim : 40 - textAnim) / 4;

    if (isEmergencyStopRequested()) {
        vita2d_pgf_draw_text(font, 480 - 140, y - 60 + textOffset, RGBA(255, 100, 100, 255), 1.2f, "Stopping operation...");
    } else {
        vita2d_pgf_draw_text(font, 480 - 150, y - 60 + textOffset, RGBA(255, 255, 100, 255), 1.2f, "Cleaning in progress...");
    }

    vita2d_draw_rectangle(x - 2, y - 2, barWidth + 4, barHeight + 4, RGBA(80, 80, 80, 255));
    vita2d_draw_rectangle(x, y, barWidth, barHeight, RGBA(30, 30, 30, 255));

    int filled = (barWidth * percent) / 100;
    if (filled > 0) {
        vita2d_draw_rectangle(x, y, filled, barHeight, RGBA(0, 180, 0, 255));

        for (int i = 0; i < filled; i += 10) {
            int alpha = 100 + (i * 100) / filled;
            vita2d_draw_rectangle(x + i, y, 10, barHeight, RGBA(50, 220, 50, alpha));
        }

        if (filled > 5) {
            vita2d_draw_rectangle(x + filled - 3, y, 3, barHeight, RGBA(100, 255, 100, 200));
        }

        if (percent > lastPercent && filled > 10) {
            for (int i = 0; i < 2; i++) {
                addParticle(x + filled - 5 + (rand() % 10), y + barHeight / 2 + (rand() % 20 - 10));
            }
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
        vita2d_pgf_draw_text(font, 480 - 80, y + 60, RGBA(100, 100, blue, 255), 1.0f, "Scanning files...");
    } else if (percent < 70) {
        int green = 200 + (statusAnim % 30);
        vita2d_pgf_draw_text(font, 480 - 100, y + 60, RGBA(100, green, 100, 255), 1.0f, "Deleting temporary files...");
    } else {
        int red = 200 + (statusAnim % 30);
        vita2d_pgf_draw_text(font, 480 - 80, y + 60, RGBA(red, 200, 100, 255), 1.0f, "Finalizing cleanup...");
    }

    vita2d_draw_rectangle(0, 500, 960, 44, RGBA(15, 25, 40, 220));

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
    vita2d_draw_rectangle(0, 0, 960, 2, borderColor);
    vita2d_draw_rectangle(0, 542, 960, 2, borderColor);
}

void showNotification(const char *title, const char *message) {
    printf("NOTIFICATION: %s - %s\n", title, message);
}

typedef struct {
    int selected;
    int total_options;
    char options[10][64];
    int enabled[10];
} MenuOptions;

typedef struct {
    FileList *fileList;
    int scrollOffset;
    int selectedFile;
} PreviewState;

typedef enum {
    PROFILE_QUICK = 0,
    PROFILE_COMPLETE,
    PROFILE_SELECTIVE
} CleaningProfile;

void initMenuOptions(MenuOptions *menu, CleaningProfile profile) {
    menu->selected = 0;
    menu->total_options = 9;

    strcpy(menu->options[0], "System Temp Files");
    strcpy(menu->options[1], "VitaShell Cache");
    strcpy(menu->options[2], "PKGi Cache");
    strcpy(menu->options[3], "RetroArch Cache");
    strcpy(menu->options[4], "Autoplugin 2 Cache");
    strcpy(menu->options[5], "Adrenaline Cache");
    strcpy(menu->options[6], "Crash Dumps");
    strcpy(menu->options[7], "Exclusion Settings");
    strcpy(menu->options[8], "All Categories");

    switch (profile) {
        case PROFILE_QUICK:
            menu->enabled[0] = 0;
            menu->enabled[1] = 1;
            menu->enabled[2] = 1;
            menu->enabled[3] = 1;
            menu->enabled[4] = 1;
            menu->enabled[5] = 1;
            menu->enabled[6] = 0;
            menu->enabled[7] = 1;
            menu->enabled[8] = 0;
            break;

        case PROFILE_COMPLETE:
            for (int i = 0; i < 9; i++) {
                menu->enabled[i] = 1;
            }
            break;

        case PROFILE_SELECTIVE:
            for (int i = 0; i < 9; i++) {
                menu->enabled[i] = 0;
            }
            menu->enabled[7] = 1;
            break;
    }
}

void drawOptionsMenu(vita2d_pgf *font, MenuOptions *menu) {
    vita2d_draw_rectangle(0, 0, 960, 544, RGBA(10, 15, 25, 200));

    vita2d_draw_rectangle(100, 60, 760, 450, RGBA(0, 120, 200, 100));
    vita2d_draw_rectangle(105, 65, 750, 440, RGBA(20, 30, 50, 250));

    vita2d_draw_rectangle(105, 65, 750, 50, RGBA(0, 100, 180, 200));
    vita2d_draw_rectangle(105, 65, 750, 3, RGBA(0, 200, 255, 255));
    vita2d_pgf_draw_text(font, 480 - 165, 95, RGBA(255, 255, 255, 255), 1.8f, "Advanced Cleaning Options");

    vita2d_pgf_draw_text(font, 480 - 180, 135, RGBA(200, 200, 200, 255), 0.9f, "Select which categories to clean (X to toggle)");

    vita2d_draw_rectangle(120, 155, 720, 310, RGBA(15, 25, 40, 200));

    for (int i = 0; i < menu->total_options; i++) {
        int y = 175 + (i * 34);
        int isSelected = (i == menu->selected);

        if (isSelected) {
            vita2d_draw_rectangle(125, y - 2, 710, 30, RGBA(0, 150, 255, 80));
            vita2d_draw_rectangle(125, y - 2, 5, 30, RGBA(0, 200, 255, 255));
        }

        int checkboxX = 145;
        int checkboxY = y + 3;

        vita2d_draw_rectangle(checkboxX, checkboxY, 20, 20, RGBA(40, 60, 80, 255));
        vita2d_draw_rectangle(checkboxX + 1, checkboxY + 1, 18, 18, RGBA(20, 30, 45, 255));

        int shouldShowChecked = menu->enabled[i];
        if (i == 8) {
            shouldShowChecked = 1;
            for (int j = 0; j < 7; j++) {
                if (!menu->enabled[j]) {
                    shouldShowChecked = 0;
                    break;
                }
            }
        }

        if (shouldShowChecked) {
            vita2d_draw_rectangle(checkboxX + 4, checkboxY + 4, 12, 12, RGBA(0, 255, 100, 255));
            vita2d_draw_rectangle(checkboxX + 5, checkboxY + 5, 10, 10, RGBA(0, 200, 80, 255));
        }

        int textColor = isSelected ? RGBA(255, 255, 255, 255) : RGBA(200, 200, 200, 255);
        vita2d_pgf_draw_text(font, 175, y + 18, textColor, 1.1f, menu->options[i]);

        if (i < 7) {
            vita2d_draw_rectangle(780, y + 6, 35, 16, RGBA(0, 150, 200, 150));

            const char* sizeIndicators[] = {"~", "~~", "~", "~~", "~", "~~", "~"};
            vita2d_pgf_draw_text(font, 785, y + 17, RGBA(255, 255, 150, 255), 0.8f, sizeIndicators[i]);
        }
    }

    vita2d_draw_rectangle(105, 475, 750, 30, RGBA(15, 25, 40, 220));
    vita2d_draw_rectangle(105, 475, 750, 2, RGBA(0, 150, 255, 255));

    int instrY = 493;

    vita2d_draw_rectangle(125, instrY - 10, 22, 22, RGBA(80, 120, 180, 200));
    vita2d_pgf_draw_text(font, 127, instrY + 3, RGBA(255, 255, 255, 255), 0.8f, "↕");
    vita2d_pgf_draw_text(font, 155, instrY + 3, RGBA(220, 220, 220, 255), 0.9f, "Navigate");

    vita2d_draw_rectangle(265, instrY - 10, 22, 22, RGBA(80, 120, 180, 200));
    vita2d_pgf_draw_text(font, 270, instrY + 3, RGBA(255, 255, 255, 255), 0.8f, "X");
    vita2d_pgf_draw_text(font, 295, instrY + 3, RGBA(220, 220, 220, 255), 0.9f, "Toggle");

    vita2d_draw_rectangle(445, instrY - 10, 22, 22, RGBA(80, 120, 180, 200));
    vita2d_pgf_draw_text(font, 450, instrY + 3, RGBA(255, 255, 255, 255), 0.8f, "O");
    vita2d_pgf_draw_text(font, 475, instrY + 3, RGBA(220, 220, 220, 255), 0.9f, "Preview");

    vita2d_draw_rectangle(645, instrY - 10, 22, 22, RGBA(80, 120, 180, 200));
    vita2d_pgf_draw_text(font, 649, instrY + 3, RGBA(255, 255, 255, 255), 0.8f, "△");
    vita2d_pgf_draw_text(font, 675, instrY + 3, RGBA(220, 220, 220, 255), 0.9f, "Back");
}

void drawPreviewScreen(vita2d_pgf *font, PreviewState *preview) {
    vita2d_draw_rectangle(0, 0, 960, 544, RGBA(15, 25, 40, 255));

    vita2d_pgf_draw_text(font, 480 - 95, 40, RGBA(255, 255, 255, 255), 1.8f, "Preview - Files to Delete");

    if (!preview->fileList || preview->fileList->count == 0) {
        vita2d_pgf_draw_text(font, 480 - 100, 272, RGBA(255, 255, 100, 255), 1.2f, "No temporary files found!");
    } else {
        char infoText[256];
        if (preview->fileList->totalSize < (1024 * 1024))
            snprintf(infoText, sizeof(infoText), "Files: %d | Total: %llu KB",
                    preview->fileList->count, preview->fileList->totalSize / 1024);
        else if (preview->fileList->totalSize < (1024ULL * 1024 * 1024))
            snprintf(infoText, sizeof(infoText), "Files: %d | Total: %.2f MB",
                    preview->fileList->count, preview->fileList->totalSize / (1024.0 * 1024));
        else
            snprintf(infoText, sizeof(infoText), "Files: %d | Total: %.2f GB",
                    preview->fileList->count, preview->fileList->totalSize / (1024.0 * 1024 * 1024));

        vita2d_pgf_draw_text(font, 30, 80, RGBA(100, 255, 100, 255), 1.0f, infoText);

        int maxVisible = 18;
        int startIdx = preview->scrollOffset;
        int endIdx = startIdx + maxVisible;
        if (endIdx > preview->fileList->count) endIdx = preview->fileList->count;

        for (int i = startIdx; i < endIdx; i++) {
            int y = 110 + ((i - startIdx) * 22);
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
            int scrollbarHeight = 400;
            int scrollbarY = 110;
            int thumbHeight = (maxVisible * scrollbarHeight) / preview->fileList->count;
            int thumbY = scrollbarY + (preview->scrollOffset * scrollbarHeight) / preview->fileList->count;

            vita2d_draw_rectangle(945, scrollbarY, 10, scrollbarHeight, RGBA(40, 40, 40, 255));
            vita2d_draw_rectangle(945, thumbY, 10, thumbHeight, RGBA(100, 150, 200, 255));
        }
    }

    vita2d_draw_rectangle(0, 510, 960, 34, RGBA(20, 30, 50, 220));
    if (!preview->fileList || preview->fileList->count == 0) {
        vita2d_pgf_draw_text(font, 30, 530, RGBA(200, 200, 200, 255), 0.8f,
                            "O: Cancel");
    } else {
        vita2d_pgf_draw_text(font, 30, 530, RGBA(200, 200, 200, 255), 0.8f,
                            "D-Pad: scroll | X: Start cleaning | O: Cancel");
    }

    vita2d_draw_rectangle(0, 0, 960, 2, RGBA(0, 150, 255, 255));
    vita2d_draw_rectangle(0, 542, 960, 2, RGBA(0, 150, 255, 255));
}

int main() {
    sceSysmoduleLoadModule(SCE_SYSMODULE_PGF);
    sceSysmoduleLoadModule(SCE_SYSMODULE_APPUTIL);

    vita2d_init();
    vita2d_pgf *font = vita2d_load_default_pgf();
    if (!font) {
        printf("Failed to load default PGF font!\n");
        vita2d_fini();
        return -1;
    }

    SceCtrlData pad;

    initParticles();

    initEmergencyStop();

    detectSystemLanguage();

    unsigned long long spaceBefore = calculateTempSize();
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

    int showMenu = 0;
    int showPreview = 0;
    int showProfileSelect = 1;
    CleaningProfile selectedProfile = PROFILE_COMPLETE;
    int running = 1;
    while (running) {
        sceCtrlPeekBufferPositive(0, &pad, 1);

        if (!showMenu && !showPreview) {
            unsigned long long currentSpace = calculateTempSize();
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

        vita2d_start_drawing();
        vita2d_clear_screen();

        vita2d_draw_rectangle(0, 0, 960, 544, RGBA(20, 30, 50, 255));

        vita2d_pgf_draw_text(font, 480 - 80, 82, RGBA(0, 0, 0, 150), 2.2f, "PSV Cleaner");
        vita2d_pgf_draw_text(font, 480 - 82, 80, RGBA(100, 200, 255, 255), 2.2f, "PSV Cleaner");
        vita2d_pgf_draw_text(font, 480 - 80, 82, RGBA(255, 255, 255, 255), 2.2f, "PSV Cleaner");

        vita2d_pgf_draw_text(font, 480 - 95, 115, RGBA(200, 200, 200, 255), 1.1f, "Temporary Files Cleaner for PS Vita");

        if (showProfileSelect) {
            vita2d_pgf_draw_text(font, 480 - 95, 160, RGBA(255, 255, 255, 255), 1.6f, "Select Cleaning Profile");

            const char* profiles[3] = {
                "Quick Clean - Safe cache files only",
                "Complete Clean - All temporary files",
                "Selective Clean - Choose what to clean"
            };

            for (int i = 0; i < 3; i++) {
                int y = 220 + (i * 80);
                int isSelected = (i == (int)selectedProfile);

                vita2d_draw_rectangle(150, y - 10, 660, 60, RGBA(30, 50, 70, 200));
                if (isSelected) {
                    vita2d_draw_rectangle(150, y - 10, 660, 60, RGBA(0, 150, 255, 100));
                    vita2d_draw_rectangle(150, y - 8, 4, 56, RGBA(0, 200, 255, 255));
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

            vita2d_draw_rectangle(0, 500, 960, 44, RGBA(15, 25, 40, 220));
            vita2d_pgf_draw_text(font, 30, 525, RGBA(200, 200, 200, 255), 0.9f, "D-Pad: Navigate | X: Select Profile | O: Exit");
        } else if (showPreview) {
            drawPreviewScreen(font, &preview);
        } else if (showMenu) {
            drawOptionsMenu(font, &menu);
        } else {
            vita2d_draw_rectangle(195, 175, 570, 260, RGBA(0, 150, 255, 100));
            vita2d_draw_rectangle(200, 180, 560, 250, RGBA(25, 35, 55, 240));

            vita2d_draw_rectangle(200, 180, 560, 3, RGBA(0, 200, 255, 255));

            vita2d_draw_rectangle(220, 210, 50, 45, RGBA(0, 150, 255, 150));
            vita2d_draw_rectangle(225, 215, 40, 35, RGBA(40, 60, 80, 255));
            vita2d_draw_rectangle(230, 220, 30, 25, RGBA(0, 180, 255, 200));

            vita2d_pgf_draw_text(font, 290, 220, RGBA(100, 255, 255, 255), 1.0f, L(lang_ui_text, currentLanguage, 0));
            vita2d_pgf_draw_text(font, 290, 250, RGBA(255, 255, 100, 255), 1.4f, L(lang_ui_text, currentLanguage, 1));

            vita2d_draw_rectangle(220, 280, 520, 2, RGBA(0, 150, 255, 150));

vita2d_pgf_draw_text(font, 220, 310, RGBA(100, 200, 255, 255), 1.2f, L(lang_ui_text, currentLanguage, 2));

vita2d_draw_rectangle(240, 330, 25, 25, RGBA(100, 150, 255, 200));
vita2d_pgf_draw_text(font, 243, 348, RGBA(255, 255, 255, 255), 1.0f, "■");
vita2d_pgf_draw_text(font, 285, 348, RGBA(255, 255, 255, 255), 0.9f, L(lang_ui_text, currentLanguage, 3));

vita2d_draw_rectangle(240, 350, 25, 25, RGBA(100, 150, 255, 200));
vita2d_pgf_draw_text(font, 245, 368, RGBA(255, 255, 255, 255), 1.0f, "X");
vita2d_pgf_draw_text(font, 285, 368, RGBA(255, 255, 255, 255), 0.9f, L(lang_ui_text, currentLanguage, 4));

vita2d_draw_rectangle(240, 370, 25, 25, RGBA(100, 150, 255, 200));
vita2d_pgf_draw_text(font, 245, 388, RGBA(255, 255, 255, 255), 1.0f, "△");
vita2d_pgf_draw_text(font, 285, 388, RGBA(255, 255, 255, 255), 0.9f, L(lang_ui_text, currentLanguage, 5));

vita2d_draw_rectangle(240, 390, 25, 25, RGBA(100, 150, 255, 200));
vita2d_pgf_draw_text(font, 245, 408, RGBA(255, 255, 255, 255), 1.0f, "O");
vita2d_pgf_draw_text(font, 285, 408, RGBA(255, 255, 255, 255), 0.9f, L(lang_ui_text, currentLanguage, 6));

            vita2d_draw_rectangle(0, 470, 960, 74, RGBA(15, 25, 40, 200));
            vita2d_draw_rectangle(0, 470, 960, 2, RGBA(0, 150, 255, 255));

            vita2d_pgf_draw_text(font, 30, 495, RGBA(150, 200, 255, 255), 0.9f, "Version 1.06");

            char statsText[128];
            snprintf(statsText, sizeof(statsText), "Ready to clean temporary files and optimize your PS Vita");
            vita2d_pgf_draw_text(font, 30, 520, RGBA(180, 180, 180, 255), 0.85f, statsText);

            vita2d_pgf_draw_text(font, 600, 495, RGBA(255, 255, 150, 255), 0.85f, "Tip: Use Preview to see");
            vita2d_pgf_draw_text(font, 600, 520, RGBA(255, 255, 150, 255), 0.85f, "files before cleaning");
        }

        vita2d_draw_rectangle(0, 0, 960, 2, RGBA(0, 150, 255, 255));
        vita2d_draw_rectangle(0, 542, 960, 2, RGBA(0, 150, 255, 255));

        vita2d_end_drawing();
        vita2d_swap_buffers();

        gpuMemoryCleanup();

        if (showProfileSelect) {
            if (pad.buttons & SCE_CTRL_UP) {
                selectedProfile = (CleaningProfile)((int)selectedProfile - 1);
                if ((int)selectedProfile < 0) selectedProfile = PROFILE_SELECTIVE;
                sceKernelDelayThread(200 * 1000);
            }
            if (pad.buttons & SCE_CTRL_DOWN) {
                selectedProfile = (CleaningProfile)((int)selectedProfile + 1);
                if ((int)selectedProfile > 2) selectedProfile = PROFILE_QUICK;
                sceKernelDelayThread(200 * 1000);
            }
            if (pad.buttons & SCE_CTRL_CROSS) {
                showProfileSelect = 0;
                initMenuOptions(&menu, selectedProfile);
                sceKernelDelayThread(200 * 1000);
            }
            if (pad.buttons & SCE_CTRL_CIRCLE) {
                running = 0;
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
                    if (preview.selectedFile >= preview.scrollOffset + 18) {
                        preview.scrollOffset = preview.selectedFile - 17;
                    }
                }
                sceKernelDelayThread(100 * 1000);
            }
            if (pad.buttons & SCE_CTRL_CROSS) {
                if (preview.fileList && preview.fileList->count > 0) {
                    showPreview = 0;

                    startOperation();

                    int cleaningInterrupted = 0;
                    for (int pct = 0; pct <= 100 && !isEmergencyStopRequested(); pct++) {
                        vita2d_start_drawing();
                        vita2d_clear_screen();
                        drawProgressBar(font, pct);
                        vita2d_end_drawing();
                        vita2d_swap_buffers();
                        sceKernelDelayThread(50 * 1000);

                        sceCtrlPeekBufferPositive(0, &pad, 1);
                        if (pad.buttons & SCE_CTRL_CIRCLE) {
                            requestEmergencyStop();
                            cleaningInterrupted = 1;
                            break;
                        }
                    }

                    endOperation();

                    if (cleaningInterrupted) {
                        cleanupAfterEmergencyStop();

                        unsigned long long spaceFreedFromPreview = (preview.fileList ? preview.fileList->totalSize : 0);

                        cleanTemporaryFiles();

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

                        cleanTemporaryFiles();

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

                    sceKernelDelayThread(200 * 1000);
                }
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
                menu.selected = (menu.selected - 1 + menu.total_options) % menu.total_options;
                sceKernelDelayThread(200 * 1000);
            }
            if (pad.buttons & SCE_CTRL_DOWN) {
                menu.selected = (menu.selected + 1) % menu.total_options;
                sceKernelDelayThread(200 * 1000);
            }
            if (pad.buttons & SCE_CTRL_CROSS) {
                if (menu.selected == 8) {
                    for (int i = 0; i < 7; i++) {
                        menu.enabled[i] = 1;
                    }
                } else {
                    menu.enabled[menu.selected] = !menu.enabled[menu.selected];
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

    vita2d_free_pgf(font);
    vita2d_fini();

    return 0;
}
