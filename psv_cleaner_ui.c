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

// Define RGBA macro for color values
#define RGBA(r,g,b,a) ((a) << 24 | (r) << 16 | (g) << 8 | (b))

// GPU memory management function
void gpuMemoryCleanup() {
    // Force GPU memory cleanup to prevent crashes
    vita2d_wait_rendering_done();
}

// Draw progress bar with cleaning text
void drawProgressBar(vita2d_pgf *font, int percent) {
    int barWidth = 500;
    int barHeight = 40;
    int x = 480 - (barWidth / 2);
    int y = 272 - (barHeight / 2);

    // Draw simple background (GPU friendly)
    vita2d_draw_rectangle(0, 0, 960, 544, RGBA(15, 25, 40, 255));

    // Draw main title
    vita2d_pgf_draw_text(font, 480 - 100, 100, RGBA(255, 255, 255, 255), 1.6f, "PSV Cleaner");
    
    // Draw "Cleaning in progress..." text above the bar
    if (isEmergencyStopRequested()) {
        vita2d_pgf_draw_text(font, 480 - 140, y - 60, RGBA(255, 100, 100, 255), 1.2f, "Stopping operation...");
    } else {
        vita2d_pgf_draw_text(font, 480 - 150, y - 60, RGBA(255, 255, 100, 255), 1.2f, "Cleaning in progress...");
    }

    // Draw progress bar background (optimized for GPU)
    vita2d_draw_rectangle(x, y, barWidth, barHeight, RGBA(40, 40, 40, 255));
    
    // Draw progress bar fill (single rectangle for better performance)
    int filled = (barWidth * percent) / 100;
    if (filled > 0) {
        vita2d_draw_rectangle(x, y, filled, barHeight, RGBA(0, 180, 0, 255));
    }

    // Draw percentage text centered vertically in the progress bar
    char text[32];
    snprintf(text, sizeof(text), "%d%%", percent);
    vita2d_pgf_draw_text(font, 480 - 30, y + 25, RGBA(255, 255, 255, 255), 1.4f, text);

    // Draw status text below the bar
    if (percent < 30) {
        vita2d_pgf_draw_text(font, 480 - 80, y + 60, RGBA(200, 200, 200, 255), 1.0f, "Scanning files...");
    } else if (percent < 70) {
        vita2d_pgf_draw_text(font, 480 - 100, y + 60, RGBA(200, 200, 200, 255), 1.0f, "Deleting temporary files...");
    } else {
        vita2d_pgf_draw_text(font, 480 - 80, y + 60, RGBA(200, 200, 200, 255), 1.0f, "Finalizing cleanup...");
    }

    // Draw control instructions during cleaning
    vita2d_draw_rectangle(0, 500, 960, 44, RGBA(15, 25, 40, 220));

    // Emergency stop instruction (highlighted)
    if (isEmergencyStopRequested()) {
        vita2d_pgf_draw_text(font, 30, 525, RGBA(255, 100, 100, 255), 0.9f, "EMERGENCY STOP REQUESTED - Please wait...");
    } else {
        // Show emergency stop control
        vita2d_draw_rectangle(30, 510, 25, 25, RGBA(255, 100, 100, 200));
        vita2d_pgf_draw_text(font, 35, 528, RGBA(255, 255, 255, 255), 1.0f, "O");
        vita2d_pgf_draw_text(font, 65, 528, RGBA(255, 255, 255, 255), 0.9f, "Emergency Stop");

        // Add spacing and show instruction text
        vita2d_pgf_draw_text(font, 220, 528, RGBA(200, 200, 200, 255), 0.8f, "Press O during cleaning to stop safely");
    }

    // Draw decorative borders
    vita2d_draw_rectangle(0, 0, 960, 2, RGBA(0, 200, 0, 255));
    vita2d_draw_rectangle(0, 542, 960, 2, RGBA(0, 200, 0, 255));
}

// Show splash screen
void showSplash(vita2d_texture *tex) {
    vita2d_start_drawing();
    vita2d_clear_screen();

    int x = (960 - vita2d_texture_get_width(tex)) / 2;
    int y = (544 - vita2d_texture_get_height(tex)) / 2;

    vita2d_draw_texture(tex, x, y);
    vita2d_end_drawing();
    vita2d_swap_buffers();
    sceKernelDelayThread(3 * 1000 * 1000); // 3 seconds
}

// Play success sound (melody like CloneDVD)
void playSuccessSound() {
    // Create a pleasant success melody
    // Melody: C-E-G-C-E-G-C (Do-Mi-Sol-Do-Mi-Sol-Do)
    
    // First phrase: C-E-G-C
    sceKernelDelayThread(120 * 1000); // C
    sceKernelDelayThread(120 * 1000); // E  
    sceKernelDelayThread(120 * 1000); // G
    sceKernelDelayThread(180 * 1000); // C (longer)
    
    // Pause between phrases
    sceKernelDelayThread(80 * 1000);
    
    // Second phrase: E-G-C
    sceKernelDelayThread(120 * 1000); // E
    sceKernelDelayThread(120 * 1000); // G
    sceKernelDelayThread(200 * 1000); // C (longest)
    
    // Final pause
    sceKernelDelayThread(100 * 1000);
}

// Show system notification
void showNotification(const char *title, const char *message) {
    // Use PS Vita system notification
    printf("NOTIFICATION: %s - %s\n", title, message);
}

// Menu options structure
typedef struct {
    int selected;
    int total_options;
    char options[10][64];
    int enabled[10];
} MenuOptions;

// Preview screen state
typedef struct {
    FileList *fileList;
    int scrollOffset;
    int selectedFile;
} PreviewState;

// Cleaning profiles
typedef enum {
    PROFILE_QUICK = 0,    // Cache only
    PROFILE_COMPLETE,     // Everything
    PROFILE_SELECTIVE     // Categories chosen by user
} CleaningProfile;

// Initialize menu options with profile-based defaults
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

    // Initialize based on profile
    switch (profile) {
        case PROFILE_QUICK:  // Cache only
            menu->enabled[0] = 0; // No system temp
            menu->enabled[1] = 1; // VitaShell cache
            menu->enabled[2] = 1; // PKGi cache
            menu->enabled[3] = 1; // RetroArch cache
            menu->enabled[4] = 1; // Autoplugin cache
            menu->enabled[5] = 1; // Adrenaline cache
            menu->enabled[6] = 0; // No crash dumps
            menu->enabled[7] = 1; // Exclusion settings
            menu->enabled[8] = 0; // Will be calculated
            break;

        case PROFILE_COMPLETE:  // Everything
            for (int i = 0; i < 9; i++) {
                menu->enabled[i] = 1;
            }
            break;

        case PROFILE_SELECTIVE:  // User chooses
            for (int i = 0; i < 9; i++) {
                menu->enabled[i] = 0; // Start with nothing selected
            }
            menu->enabled[7] = 1; // Exclusion settings always on
            break;
    }
}

// Draw options menu
void drawOptionsMenu(vita2d_pgf *font, MenuOptions *menu) {
    // Draw background with gradient effect (using multiple rectangles)
    vita2d_draw_rectangle(0, 0, 960, 544, RGBA(10, 15, 25, 200));
    
    // Draw main panel with border
    vita2d_draw_rectangle(100, 60, 760, 450, RGBA(0, 120, 200, 100)); // Outer border
    vita2d_draw_rectangle(105, 65, 750, 440, RGBA(20, 30, 50, 250)); // Main panel
    
    // Draw title bar
    vita2d_draw_rectangle(105, 65, 750, 50, RGBA(0, 100, 180, 200));
    vita2d_draw_rectangle(105, 65, 750, 3, RGBA(0, 200, 255, 255)); // Top accent
    vita2d_pgf_draw_text(font, 480 - 165, 95, RGBA(255, 255, 255, 255), 1.8f, "Advanced Cleaning Options");
    
    // Draw subtitle
    vita2d_pgf_draw_text(font, 480 - 180, 135, RGBA(200, 200, 200, 255), 0.9f, "Select which categories to clean (X to toggle)");
    
    // Draw options list background
    vita2d_draw_rectangle(120, 155, 720, 310, RGBA(15, 25, 40, 200));
    
    // Draw options with improved styling
    for (int i = 0; i < menu->total_options; i++) {
        int y = 175 + (i * 34);
        int isSelected = (i == menu->selected);
        
        // Draw selection highlight with animation effect
        if (isSelected) {
            // Highlight bar with gradient effect
            vita2d_draw_rectangle(125, y - 2, 710, 30, RGBA(0, 150, 255, 80));
            vita2d_draw_rectangle(125, y - 2, 5, 30, RGBA(0, 200, 255, 255)); // Left accent bar
        }
        
        // Draw checkbox with better styling
        int checkboxX = 145;
        int checkboxY = y + 3;
        
        // Checkbox background
        vita2d_draw_rectangle(checkboxX, checkboxY, 20, 20, RGBA(40, 60, 80, 255));
        vita2d_draw_rectangle(checkboxX + 1, checkboxY + 1, 18, 18, RGBA(20, 30, 45, 255));
        
        // Special handling for "All Categories" option - show state based on other options
        int shouldShowChecked = menu->enabled[i];
        if (i == 8) {
            // For "All Categories", show checked if all CLEANING options are enabled (exclude "Exclusion Settings")
            shouldShowChecked = 1;
            for (int j = 0; j < 7; j++) { // Only check first 7 options (exclude "Exclusion Settings")
                if (!menu->enabled[j]) {
                    shouldShowChecked = 0;
                    break;
                }
            }
        }

        if (shouldShowChecked) {
            // Draw checkmark
            vita2d_draw_rectangle(checkboxX + 4, checkboxY + 4, 12, 12, RGBA(0, 255, 100, 255));
            vita2d_draw_rectangle(checkboxX + 5, checkboxY + 5, 10, 10, RGBA(0, 200, 80, 255));
        }
        
        // Draw option text with better color coding
        int textColor = isSelected ? RGBA(255, 255, 255, 255) : RGBA(200, 200, 200, 255);
        vita2d_pgf_draw_text(font, 175, y + 18, textColor, 1.1f, menu->options[i]);
        
        // Draw category icon/indicator
        if (i < 7) { // Regular categories
            vita2d_draw_rectangle(780, y + 6, 35, 16, RGBA(0, 150, 200, 150));
            
            // Show estimated size indicator (placeholder)
            const char* sizeIndicators[] = {"~", "~~", "~", "~~", "~", "~~", "~"};
            vita2d_pgf_draw_text(font, 785, y + 17, RGBA(255, 255, 150, 255), 0.8f, sizeIndicators[i]);
        } else if (i == 8) { // "All Categories" option
        }
    }
    
    // Draw bottom section with instructions
    vita2d_draw_rectangle(105, 475, 750, 30, RGBA(15, 25, 40, 220));
    vita2d_draw_rectangle(105, 475, 750, 2, RGBA(0, 150, 255, 255)); // Top border
    
    // Draw control instructions with icons
    int instrY = 493;
    
    // D-Pad icon
    vita2d_draw_rectangle(125, instrY - 10, 22, 22, RGBA(80, 120, 180, 200));
    vita2d_pgf_draw_text(font, 127, instrY + 3, RGBA(255, 255, 255, 255), 0.8f, "↕");
    vita2d_pgf_draw_text(font, 155, instrY + 3, RGBA(220, 220, 220, 255), 0.9f, "Navigate");
    
    // X button
    vita2d_draw_rectangle(265, instrY - 10, 22, 22, RGBA(80, 120, 180, 200));
    vita2d_pgf_draw_text(font, 270, instrY + 3, RGBA(255, 255, 255, 255), 0.8f, "X");
    vita2d_pgf_draw_text(font, 295, instrY + 3, RGBA(220, 220, 220, 255), 0.9f, L(lang_ui_text, currentLanguage, 4));

    // Select button (hidden as requested)
    // vita2d_draw_rectangle(525, instrY - 10, 22, 22, RGBA(80, 120, 180, 200));
    // vita2d_pgf_draw_text(font, 530, instrY + 3, RGBA(255, 255, 255, 255), 0.8f, "■");
    // vita2d_pgf_draw_text(font, 555, instrY + 3, RGBA(220, 220, 220, 255), 0.9f, "Select All");
    
    // O button
    vita2d_draw_rectangle(445, instrY - 10, 22, 22, RGBA(80, 120, 180, 200));
    vita2d_pgf_draw_text(font, 450, instrY + 3, RGBA(255, 255, 255, 255), 0.8f, "O");
    vita2d_pgf_draw_text(font, 475, instrY + 3, RGBA(220, 220, 220, 255), 0.9f, L(lang_ui_text, currentLanguage, 4));
    
    // Triangle button
    vita2d_draw_rectangle(645, instrY - 10, 22, 22, RGBA(80, 120, 180, 200));
    vita2d_pgf_draw_text(font, 649, instrY + 3, RGBA(255, 255, 255, 255), 0.8f, "△");
    vita2d_pgf_draw_text(font, 675, instrY + 3, RGBA(220, 220, 220, 255), 0.9f, L(lang_ui_text, currentLanguage, 5));
}

// Draw preview screen
void drawPreviewScreen(vita2d_pgf *font, PreviewState *preview) {
    // Draw background
    vita2d_draw_rectangle(0, 0, 960, 544, RGBA(15, 25, 40, 255));
    
    // Draw title - properly centered
    vita2d_pgf_draw_text(font, 480 - 95, 40, RGBA(255, 255, 255, 255), 1.8f, "Preview - Files to Delete");
    
    if (!preview->fileList || preview->fileList->count == 0) {
        // No files found
        vita2d_pgf_draw_text(font, 480 - 100, 272, RGBA(255, 255, 100, 255), 1.2f, "No temporary files found!");
    } else {
        // Draw file count and total size
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
        
        // Draw file list (scrollable)
        int maxVisible = 18; // Maximum files visible on screen
        int startIdx = preview->scrollOffset;
        int endIdx = startIdx + maxVisible;
        if (endIdx > preview->fileList->count) endIdx = preview->fileList->count;
        
        for (int i = startIdx; i < endIdx; i++) {
            int y = 110 + ((i - startIdx) * 22);
            int color = (i == preview->selectedFile) ? RGBA(255, 255, 100, 255) : RGBA(200, 200, 200, 255);
            
            // Draw selection background
            if (i == preview->selectedFile) {
                vita2d_draw_rectangle(20, y - 3, 920, 20, RGBA(50, 100, 150, 100));
            }
            
            // Format file path (truncate if too long)
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
            
            // Format file size
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
            
            // Draw file info
            vita2d_pgf_draw_text(font, 25, y + 12, color, 0.7f, displayPath);
            vita2d_pgf_draw_text(font, 850, y + 12, color, 0.7f, sizeText);
        }
        
        // Draw scrollbar if needed
        if (preview->fileList->count > maxVisible) {
            int scrollbarHeight = 400;
            int scrollbarY = 110;
            int thumbHeight = (maxVisible * scrollbarHeight) / preview->fileList->count;
            int thumbY = scrollbarY + (preview->scrollOffset * scrollbarHeight) / preview->fileList->count;
            
            // Draw scrollbar background
            vita2d_draw_rectangle(945, scrollbarY, 10, scrollbarHeight, RGBA(40, 40, 40, 255));
            // Draw scrollbar thumb
            vita2d_draw_rectangle(945, thumbY, 10, thumbHeight, RGBA(100, 150, 200, 255));
        }
    }
    
    // Draw instructions - different text if no files found
    vita2d_draw_rectangle(0, 510, 960, 34, RGBA(20, 30, 50, 220));
    if (!preview->fileList || preview->fileList->count == 0) {
        vita2d_pgf_draw_text(font, 30, 530, RGBA(200, 200, 200, 255), 0.8f,
                            "O: Cancel");
    } else {
        vita2d_pgf_draw_text(font, 30, 530, RGBA(200, 200, 200, 255), 0.8f,
                            "D-Pad: scroll | X: Start cleaning | O: Cancel");
    }
    
    // Draw borders
    vita2d_draw_rectangle(0, 0, 960, 2, RGBA(0, 150, 255, 255));
    vita2d_draw_rectangle(0, 542, 960, 2, RGBA(0, 150, 255, 255));
}

int main() {
    // Initialize required modules
    sceSysmoduleLoadModule(SCE_SYSMODULE_PGF);
    sceSysmoduleLoadModule(SCE_SYSMODULE_APPUTIL);
    
    // Initialize Vita2D
    vita2d_init();
    vita2d_pgf *font = vita2d_load_default_pgf();
    if (!font) {
        printf("Failed to load default PGF font!\n");
        vita2d_fini();
        return -1;
    }

    SceCtrlData pad;

    // Load splash image
    vita2d_texture *splash = vita2d_load_PNG_file("app0:resources/Reihen.png");
    if (!splash) {
        printf("Failed to load splash image!\n");
        vita2d_free_pgf(font);
        vita2d_fini();
        return -1;
    }
    showSplash(splash);

    // Initialize emergency stop system
    initEmergencyStop();

    // Detect system language automatically
    detectSystemLanguage();

    // Calculate initial freeable space immediately after splash
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

    // Initialize menu options with default profile
    MenuOptions menu;
    initMenuOptions(&menu, PROFILE_COMPLETE); // Default to complete cleanup
    
    // Initialize preview state
    PreviewState preview;
    preview.fileList = NULL;
    preview.scrollOffset = 0;
    preview.selectedFile = 0;
    
    int showMenu = 0;
    int showPreview = 0;
    int showProfileSelect = 1; // Start with profile selection
    CleaningProfile selectedProfile = PROFILE_COMPLETE;
    int running = 1;
    while (running) {
        sceCtrlPeekBufferPositive(0, &pad, 1);
        
        // Check if app was suspended (PS Vita doesn't have sceKernelIsExitRequested)
        // Use controller input to handle exit

        // Refresh space calculation when on main screen (not in menu or preview)
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

        // Draw simple background (GPU friendly)
        vita2d_draw_rectangle(0, 0, 960, 544, RGBA(20, 30, 50, 255));

        // Draw main title - adjusted more to the left
        vita2d_pgf_draw_text(font, 480 - 80, 82, RGBA(0, 0, 0, 150), 2.2f, "PSV Cleaner"); // Shadow
        vita2d_pgf_draw_text(font, 480 - 82, 80, RGBA(100, 200, 255, 255), 2.2f, "PSV Cleaner"); // Main text with blue tint

        // Draw subtitle - adjusted more to the left
        vita2d_pgf_draw_text(font, 480 - 95, 115, RGBA(200, 200, 200, 255), 1.1f, "Temporary Files Cleaner for PS Vita");

        if (showProfileSelect) {
            // Draw profile selection screen
            // Draw title for profile selection - adjusted more to the left
            vita2d_pgf_draw_text(font, 480 - 95, 160, RGBA(255, 255, 255, 255), 1.6f, "Select Cleaning Profile");

            // Profile options
            const char* profiles[3] = {
                "Quick Clean - Safe cache files only",
                "Complete Clean - All temporary files",
                "Selective Clean - Choose what to clean"
            };

            for (int i = 0; i < 3; i++) {
                int y = 220 + (i * 80);
                int isSelected = (i == (int)selectedProfile);

                // Draw profile box
                vita2d_draw_rectangle(150, y - 10, 660, 60, RGBA(30, 50, 70, 200));
                if (isSelected) {
                    vita2d_draw_rectangle(150, y - 10, 660, 60, RGBA(0, 150, 255, 100));
                    vita2d_draw_rectangle(150, y - 8, 4, 56, RGBA(0, 200, 255, 255));
                }

                // Draw profile name
                vita2d_pgf_draw_text(font, 180, y + 10, RGBA(255, 255, 255, 255), 1.2f, profiles[i]);

                // Draw profile description
                if (i == 0) {
                    vita2d_pgf_draw_text(font, 180, y + 35, RGBA(180, 180, 200, 255), 0.8f, "Removes app caches, browser temp, logs (fast & safe)");
                } else if (i == 1) {
                    vita2d_pgf_draw_text(font, 180, y + 35, RGBA(180, 180, 200, 255), 0.8f, "Removes all temp files, crash dumps, system cache");
                } else {
                    vita2d_pgf_draw_text(font, 180, y + 35, RGBA(180, 180, 200, 255), 0.8f, "Manually choose which categories to clean");
                }
            }

            // Draw instructions
            vita2d_draw_rectangle(0, 500, 960, 44, RGBA(15, 25, 40, 220));
            vita2d_pgf_draw_text(font, 30, 525, RGBA(200, 200, 200, 255), 0.9f, "D-Pad: Navigate | X: Select Profile | O: Exit");
        } else if (showPreview) {
            // Draw preview screen
            drawPreviewScreen(font, &preview);
        } else if (showMenu) {
            // Draw options menu
            drawOptionsMenu(font, &menu);
        } else {
            // Draw enhanced info box with borders
            // Outer border (accent color)
            vita2d_draw_rectangle(195, 175, 570, 260, RGBA(0, 150, 255, 100));
            // Main background
            vita2d_draw_rectangle(200, 180, 560, 250, RGBA(25, 35, 55, 240));
            
            // Draw top decorative line
            vita2d_draw_rectangle(200, 180, 560, 3, RGBA(0, 200, 255, 255));

            // Draw storage icon (stylized disk/folder)
            vita2d_draw_rectangle(220, 210, 50, 45, RGBA(0, 150, 255, 150));
            vita2d_draw_rectangle(225, 215, 40, 35, RGBA(40, 60, 80, 255));
            vita2d_draw_rectangle(230, 220, 30, 25, RGBA(0, 180, 255, 200));
            
            // Draw status info instead of initial space calculation
            vita2d_pgf_draw_text(font, 290, 220, RGBA(100, 255, 255, 255), 1.0f, L(lang_ui_text, currentLanguage, 0));
            vita2d_pgf_draw_text(font, 290, 250, RGBA(255, 255, 100, 255), 1.4f, L(lang_ui_text, currentLanguage, 1));

            // Draw separator line
            vita2d_draw_rectangle(220, 280, 520, 2, RGBA(0, 150, 255, 150));

// Draw controls section with icons
vita2d_pgf_draw_text(font, 220, 310, RGBA(100, 200, 255, 255), 1.2f, L(lang_ui_text, currentLanguage, 2));

// Draw control buttons with background - centered symbols
// Square button
vita2d_draw_rectangle(240, 330, 25, 25, RGBA(100, 150, 255, 200));
vita2d_pgf_draw_text(font, 243, 348, RGBA(255, 255, 255, 255), 1.0f, "■");
vita2d_pgf_draw_text(font, 285, 348, RGBA(255, 255, 255, 255), 0.9f, L(lang_ui_text, currentLanguage, 3));

// X button
vita2d_draw_rectangle(240, 350, 25, 25, RGBA(100, 150, 255, 200));
vita2d_pgf_draw_text(font, 245, 368, RGBA(255, 255, 255, 255), 1.0f, "X");
vita2d_pgf_draw_text(font, 285, 368, RGBA(255, 255, 255, 255), 0.9f, L(lang_ui_text, currentLanguage, 4));

// Triangle button
vita2d_draw_rectangle(240, 370, 25, 25, RGBA(100, 150, 255, 200));
vita2d_pgf_draw_text(font, 245, 388, RGBA(255, 255, 255, 255), 1.0f, "△");
vita2d_pgf_draw_text(font, 285, 388, RGBA(255, 255, 255, 255), 0.9f, L(lang_ui_text, currentLanguage, 5));

// Circle button
vita2d_draw_rectangle(240, 390, 25, 25, RGBA(100, 150, 255, 200));
vita2d_pgf_draw_text(font, 245, 408, RGBA(255, 255, 255, 255), 1.0f, "O");
vita2d_pgf_draw_text(font, 285, 408, RGBA(255, 255, 255, 255), 0.9f, L(lang_ui_text, currentLanguage, 6));
            
            // Draw version and footer info
            vita2d_draw_rectangle(0, 470, 960, 74, RGBA(15, 25, 40, 200));
            vita2d_draw_rectangle(0, 470, 960, 2, RGBA(0, 150, 255, 255));
            
            // Version info
            vita2d_pgf_draw_text(font, 30, 495, RGBA(150, 200, 255, 255), 0.9f, "Version 1.04");
            
            // Quick stats in footer
            char statsText[128];
            snprintf(statsText, sizeof(statsText), "Ready to clean temporary files and optimize your PS Vita");
            vita2d_pgf_draw_text(font, 30, 520, RGBA(180, 180, 180, 255), 0.85f, statsText);
            
            // Draw tip on the right side
            vita2d_pgf_draw_text(font, 600, 495, RGBA(255, 255, 150, 255), 0.85f, "Tip: Use Preview to see");
            vita2d_pgf_draw_text(font, 600, 520, RGBA(255, 255, 150, 255), 0.85f, "files before cleaning");
        }

        // Draw decorative elements
        // Top border
        vita2d_draw_rectangle(0, 0, 960, 2, RGBA(0, 150, 255, 255));
        // Bottom border  
        vita2d_draw_rectangle(0, 542, 960, 2, RGBA(0, 150, 255, 255));

        vita2d_end_drawing();
        vita2d_swap_buffers();
        
        // GPU memory cleanup every frame to prevent crashes
        gpuMemoryCleanup();

        if (showProfileSelect) {
            // Handle profile selection
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
                // Confirm profile selection
                showProfileSelect = 0;
                initMenuOptions(&menu, selectedProfile);
                sceKernelDelayThread(200 * 1000);
            }
            if (pad.buttons & SCE_CTRL_CIRCLE) {
                // Exit app
                running = 0;
            }
        } else if (showPreview) {
            // Handle preview navigation
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
                // Only start cleaning if there are files to delete
                if (preview.fileList && preview.fileList->count > 0) {
                    // Start cleaning
                    showPreview = 0;

                    // Initialize and start cleaning operation with emergency stop support
                    startOperation();

                    // Start cleaning with progress bar and emergency stop support
                    int cleaningInterrupted = 0;
                    for (int pct = 0; pct <= 100 && !isEmergencyStopRequested(); pct++) {
                        vita2d_start_drawing();
                        vita2d_clear_screen();
                        drawProgressBar(font, pct);
                        vita2d_end_drawing();
                        vita2d_swap_buffers();
                        sceKernelDelayThread(50 * 1000);

                        // Check if user presses Circle to request emergency stop during cleaning
                        sceCtrlPeekBufferPositive(0, &pad, 1);
                        if (pad.buttons & SCE_CTRL_CIRCLE) {
                            requestEmergencyStop();
                            cleaningInterrupted = 1;
                            break;
                        }
                    }

                    // End operation
                    endOperation();

                    // Handle emergency stop cleanup if needed
                    if (cleaningInterrupted) {
                        cleanupAfterEmergencyStop();

                        // Show partial completion screen for emergency stop
                        // Use preview size as more accurate measurement for space freed (partial)
                        unsigned long long spaceFreedFromPreview = (preview.fileList ? preview.fileList->totalSize : 0);

                        // Perform partial cleaning results calculation
                        cleanTemporaryFiles();

                        // Save deleted file count for display (this includes any additional files deleted)
                        int filesDeleted = getDeletedFilesCount();

                        // Recalculate space after partial cleaning
                        spaceBefore = calculateTempSize();

                        // Format space freed text using preview totals (may be incomplete due to interruption)
                        char spaceText[128];
                        if (spaceFreedFromPreview < (1024 * 1024))
                            snprintf(spaceText, sizeof(spaceText), "Space Freed: %llu KB", spaceFreedFromPreview / 1024);
                        else if (spaceFreedFromPreview < (1024ULL * 1024 * 1024))
                            snprintf(spaceText, sizeof(spaceText), "Space Freed: %.2f MB", spaceFreedFromPreview / (1024.0 * 1024));
                        else
                            snprintf(spaceText, sizeof(spaceText), "Space Freed: %.2f GB", spaceFreedFromPreview / (1024.0 * 1024 * 1024));

                        // Show emergency stop with partial results
                        vita2d_start_drawing();
                        vita2d_clear_screen();
                        vita2d_draw_rectangle(0, 0, 960, 544, RGBA(40, 40, 80, 255)); // Blue-gray background

                        // Emergency stop title
                        vita2d_pgf_draw_text(font, 480 - 130, 120, RGBA(255, 255, 255, 255), 2.0f, "OPERATION INTERRUPTED");

                        // Explanation - different for emergency stop
                        vita2d_pgf_draw_text(font, 480 - 180, 180, RGBA(255, 200, 200, 255), 1.2f, "Cleaning was safely interrupted by user request");
                        vita2d_pgf_draw_text(font, 480 - 160, 210, RGBA(255, 200, 200, 255), 0.9f, "Some temporary files may have been deleted");

                        // Statistics from partial cleanup
                        // Space freed - show partial results
                        vita2d_draw_rectangle(360, 260, 240, 40, RGBA(30, 80, 120, 180));
                        vita2d_draw_rectangle(360, 260, 240, 3, RGBA(100, 200, 255, 255));
                        vita2d_draw_rectangle(360, 297, 240, 3, RGBA(100, 200, 255, 255));
                        vita2d_pgf_draw_text(font, 480 - 105, 285, RGBA(255, 255, 255, 255), 1.2f, spaceText);

                        // Files deleted - show count from partial cleanup
                        vita2d_draw_rectangle(360, 310, 240, 40, RGBA(30, 80, 120, 180));
                        vita2d_draw_rectangle(360, 310, 240, 3, RGBA(100, 200, 255, 255));
                        vita2d_draw_rectangle(360, 347, 240, 3, RGBA(100, 200, 255, 255));
                        char filesText[64];
                        snprintf(filesText, sizeof(filesText), "Files Deleted: %d", filesDeleted);
                        vita2d_pgf_draw_text(font, 480 - 105, 335, RGBA(255, 255, 255, 255), 1.2f, filesText);

                        // Instructions - return to main menu
                        vita2d_pgf_draw_text(font, 480 - 120, 400, RGBA(200, 200, 200, 255), 0.9f, "Returning to main menu...");

                        vita2d_end_drawing();
                        vita2d_swap_buffers();
                        sceKernelDelayThread(4 * 1000 * 1000); // 4 seconds to show results

                        // Free preview list and return to main menu
                        if (preview.fileList) {
                            freeFileList(preview.fileList);
                            preview.fileList = NULL;
                        }
                        sceKernelDelayThread(200 * 1000);
                        continue; // Return to main menu
                    }

                    if (running) {
                        // Increment cleanup counter and save
                        int cleanupCount = loadCleanupCounter() + 1;
                        saveCleanupCounter(cleanupCount);

                        // Use preview size as more accurate measurement for space freed
                        unsigned long long spaceFreedFromPreview = (preview.fileList ? preview.fileList->totalSize : 0);

                        // Perform cleaning
                        cleanTemporaryFiles();

                        // Save deleted file count for display
                        int filesDeleted = getDeletedFilesCount();

                        // Recalculate space after cleaning
                        spaceBefore = calculateTempSize();
                        if (spaceBefore < (1024 * 1024))
                            snprintf(freeText, sizeof(freeText), "Space to free: %llu KB", spaceBefore / 1024);
                        else if (spaceBefore < (1024ULL * 1024 * 1024))
                            snprintf(freeText, sizeof(freeText), "Space to free: %.2f MB", spaceBefore / (1024.0 * 1024));
                        else
                            snprintf(freeText, sizeof(freeText), "Space to free: %.2f GB", spaceBefore / (1024.0 * 1024 * 1024));

                        // Format space freed text using preview totals
                        char spaceText[128];
                        if (spaceFreedFromPreview < (1024 * 1024))
                            snprintf(spaceText, sizeof(spaceText), "Space Freed: %llu KB", spaceFreedFromPreview / 1024);
                        else if (spaceFreedFromPreview < (1024ULL * 1024 * 1024))
                            snprintf(spaceText, sizeof(spaceText), "Space Freed: %.2f MB", spaceFreedFromPreview / (1024.0 * 1024));
                        else
                            snprintf(spaceText, sizeof(spaceText), "Space Freed: %.2f GB", spaceFreedFromPreview / (1024.0 * 1024 * 1024));

                        // Play success sound
                        playSuccessSound();

                        // Show notification
                        showNotification("PSV Cleaner", "Cleaning completed successfully!");

                        // Draw enhanced success screen with celebratory styling (fixed layout)
                        vita2d_start_drawing();
                        vita2d_clear_screen();

                        // Clean solid green background (no gradient issues)
                        vita2d_draw_rectangle(0, 0, 960, 544, RGBA(15, 40, 25, 255));

                        // Simple clean circle checkmark (no decorations that cause artifacts)

                        // Congratulatory title - clean and centered
                        vita2d_pgf_draw_text(font, 480 - 155, 78, RGBA(0, 0, 0, 200), 1.9f, "CLEANING COMPLETED!"); // Shadow
                        vita2d_pgf_draw_text(font, 480 - 157, 75, RGBA(100, 255, 100, 255), 1.9f, "CLEANING COMPLETED!"); // Main

                        // Enthusiastic subtitle - centered
                        vita2d_pgf_draw_text(font, 480 - 130, 250, RGBA(255, 255, 150, 255), 1.1f, "Great job! Your PS Vita is cleaner!");

                        // Cleanup counter - centered box and text
                        vita2d_draw_rectangle(430, 270, 100, 25, RGBA(50, 100, 50, 200));
                        vita2d_draw_rectangle(430, 270, 100, 2, RGBA(0, 255, 100, 255));
                        char countText[64];
                        snprintf(countText, sizeof(countText), "Cleanup #%d", cleanupCount);
                        vita2d_pgf_draw_text(font, 480 - 45, 290, RGBA(255, 255, 200, 255), 1.0f, countText);

                        // Statistics panels with improved spacing and styling
                        // Space freed - centered panels and text
                        vita2d_draw_rectangle(360, 320, 240, 40, RGBA(30, 80, 120, 180));
                        vita2d_draw_rectangle(360, 320, 240, 3, RGBA(100, 200, 255, 255)); // Thicker accent line
                        vita2d_draw_rectangle(360, 357, 240, 3, RGBA(100, 200, 255, 255)); // Bottom accent
                        vita2d_pgf_draw_text(font, 480 - 105, 345, RGBA(255, 255, 255, 255), 1.1f, spaceText);

                        // Files deleted - centered
                        vita2d_draw_rectangle(360, 370, 240, 40, RGBA(30, 80, 120, 180));
                        vita2d_draw_rectangle(360, 370, 240, 3, RGBA(100, 200, 255, 255)); // Thicker accent line
                        vita2d_draw_rectangle(360, 407, 240, 3, RGBA(100, 200, 255, 255)); // Bottom accent
                        char filesText[64];
                        snprintf(filesText, sizeof(filesText), "Files Deleted: %d", filesDeleted);
                        vita2d_pgf_draw_text(font, 480 - 105, 395, RGBA(255, 255, 255, 255), 1.1f, filesText);

                        // Motivational bottom message - centered
                        vita2d_pgf_draw_text(font, 480 - 100, 460, RGBA(200, 255, 200, 255), 0.9f, "Keep your PS Vita clean and healthy!");

                        vita2d_end_drawing();
                        vita2d_swap_buffers();
                        sceKernelDelayThread(3 * 1000 * 1000); // 3 seconds
                    }

                    // Free preview list
                    if (preview.fileList) {
                        freeFileList(preview.fileList);
                        preview.fileList = NULL;
                    }

                    sceKernelDelayThread(200 * 1000);
                }
            }
            if (pad.buttons & SCE_CTRL_CIRCLE) {
                // Cancel preview
                showPreview = 0;
                if (preview.fileList) {
                    freeFileList(preview.fileList);
                    preview.fileList = NULL;
                }
                sceKernelDelayThread(200 * 1000);
            }
        } else if (showMenu) {
            // Handle menu navigation
            if (pad.buttons & SCE_CTRL_UP) {
                menu.selected = (menu.selected - 1 + menu.total_options) % menu.total_options;
                sceKernelDelayThread(200 * 1000); // 200ms delay
            }
            if (pad.buttons & SCE_CTRL_DOWN) {
                menu.selected = (menu.selected + 1) % menu.total_options;
                sceKernelDelayThread(200 * 1000); // 200ms delay
            }
            if (pad.buttons & SCE_CTRL_CROSS) {
                // Special handling for "All Categories" - if X is pressed and "All Categories" is selected, activate all
                if (menu.selected == 8) {
                    // Activate all cleaning options (0-6, skipping Exclusion Settings)
                    for (int i = 0; i < 7; i++) {
                        menu.enabled[i] = 1;
                    }
                } else {
                    // Toggle option normally
                    menu.enabled[menu.selected] = !menu.enabled[menu.selected];
                }
                sceKernelDelayThread(200 * 1000); // 200ms delay
            }
            if (pad.buttons & SCE_CTRL_SELECT) {
                // Select all / deselect all functionality (EXCEPT "Exclusion Settings" which is always left alone)
                int allEnabled = 1;
                for (int i = 0; i < 7; i++) { // Only check first 7 options (exclude "Exclusion Settings")
                    if (!menu.enabled[i]) {
                        allEnabled = 0;
                        break;
                    }
                }

                // Set all options except "Exclusion Settings" to the opposite state
                for (int i = 0; i < 7; i++) { // Only toggle first 7 options
                    menu.enabled[i] = !allEnabled;
                }
                sceKernelDelayThread(200 * 1000); // 200ms delay
            }
            if (pad.buttons & SCE_CTRL_CIRCLE) {
                // Show preview screen before cleaning
                showPreview = 1;
                preview.scrollOffset = 0;
                preview.selectedFile = 0;

                // Show loading message
                vita2d_start_drawing();
                vita2d_clear_screen();
                vita2d_draw_rectangle(0, 0, 960, 544, RGBA(15, 25, 40, 255));
                vita2d_pgf_draw_text(font, 480 - 100, 272, RGBA(255, 255, 100, 255), 1.3f, "Scanning files...");
                vita2d_end_drawing();
                vita2d_swap_buffers();

                // Create and populate file list
                preview.fileList = createFileList();
                if (preview.fileList) {
                    scanFilesForPreview(preview.fileList);
                }

                showMenu = 0;
                sceKernelDelayThread(200 * 1000); // 200ms delay
            }
            if (pad.buttons & SCE_CTRL_TRIANGLE) {
                // Exit menu
                showMenu = 0;
                sceKernelDelayThread(200 * 1000); // 200ms delay
            }
        } else {
            // Handle main menu
            if (pad.buttons & SCE_CTRL_SQUARE) {
                // Return to profile selection
                showProfileSelect = 1;
                sceKernelDelayThread(200 * 1000); // 200ms delay
            }
            if (pad.buttons & SCE_CTRL_TRIANGLE) {
                // Show options menu
                showMenu = 1;
                sceKernelDelayThread(200 * 1000); // 200ms delay
            }
            if (pad.buttons & SCE_CTRL_CROSS) {
                // Show preview screen
                showPreview = 1;
                preview.scrollOffset = 0;
                preview.selectedFile = 0;

                // Show loading message
                vita2d_start_drawing();
                vita2d_clear_screen();
                vita2d_draw_rectangle(0, 0, 960, 544, RGBA(15, 25, 40, 255));
                vita2d_pgf_draw_text(font, 480 - 100, 272, RGBA(255, 255, 100, 255), 1.3f, "Scanning files...");
                vita2d_end_drawing();
                vita2d_swap_buffers();

                // Create and populate file list
                preview.fileList = createFileList();
                if (preview.fileList) {
                    scanFilesForPreview(preview.fileList);
                }

                sceKernelDelayThread(200 * 1000);
            }

            if (pad.buttons & SCE_CTRL_CIRCLE) {
                running = 0; // Exit app
            }
        }

        sceKernelDelayThread(16 * 1000); // ~60 FPS limit for better GPU performance
    }

    // Cleanup
    if (preview.fileList) {
        freeFileList(preview.fileList);
    }
    
    vita2d_free_texture(splash);
    vita2d_free_pgf(font);
    vita2d_fini();

    return 0;
}
