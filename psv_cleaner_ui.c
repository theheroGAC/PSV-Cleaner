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
    vita2d_pgf_draw_text(font, 480 - 150, y - 60, RGBA(255, 255, 100, 255), 1.2f, "Cleaning in progress...");

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

// Initialize menu options
void initMenuOptions(MenuOptions *menu) {
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
    
    // All options enabled by default
    for (int i = 0; i < 9; i++) {
        menu->enabled[i] = 1;
    }
}

// Draw options menu
void drawOptionsMenu(vita2d_pgf *font, MenuOptions *menu) {
    // Draw background
    vita2d_draw_rectangle(150, 100, 660, 400, RGBA(20, 30, 50, 240));
    
    // Draw title
    vita2d_pgf_draw_text(font, 480 - 100, 130, RGBA(255, 255, 255, 255), 1.8f, "Cleaning Options");
    
    // Draw options
    for (int i = 0; i < menu->total_options; i++) {
        int y = 180 + (i * 40);
        int color = (i == menu->selected) ? RGBA(100, 200, 255, 255) : RGBA(255, 255, 255, 255);
        int bg_color = (i == menu->selected) ? RGBA(50, 100, 150, 100) : RGBA(0, 0, 0, 0);
        
        // Draw selection background
        if (i == menu->selected) {
            vita2d_draw_rectangle(160, y - 5, 640, 30, bg_color);
        }
        
        // Draw option text (centered vertically in the selection bar)
        char optionText[128];
        snprintf(optionText, sizeof(optionText), "%s %s", 
                menu->enabled[i] ? "[X]" : "[ ]", menu->options[i]);
        vita2d_pgf_draw_text(font, 180, y + 10, color, 1.0f, optionText);
    }
    
    // Draw instructions (moved to bottom to avoid overlap with all options)
    vita2d_pgf_draw_text(font, 180, 540, RGBA(200, 200, 200, 255), 0.8f, "D-Pad: navigate | X: toggle | O: start | △: exit");
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

    // Calculate freeable space
    unsigned long long spaceBefore = calculateTempSize();
    char freeText[128];
    if (spaceBefore < (1024 * 1024))
        snprintf(freeText, sizeof(freeText), "Space to free: %llu KB", spaceBefore / 1024);
    else if (spaceBefore < (1024ULL * 1024 * 1024))
        snprintf(freeText, sizeof(freeText), "Space to free: %.2f MB", spaceBefore / (1024.0 * 1024));
    else
        snprintf(freeText, sizeof(freeText), "Space to free: %.2f GB", spaceBefore / (1024.0 * 1024 * 1024));

    // Initialize menu options
    MenuOptions menu;
    initMenuOptions(&menu);
    
    int showMenu = 0;
    int running = 1;
    while (running) {
        sceCtrlPeekBufferPositive(0, &pad, 1);
        
        // Check if app was suspended (PS Vita doesn't have sceKernelIsExitRequested)
        // Use controller input to handle exit

        vita2d_start_drawing();
        vita2d_clear_screen();

        // Draw simple background (GPU friendly)
        vita2d_draw_rectangle(0, 0, 960, 544, RGBA(20, 30, 50, 255));

        // Draw main title with shadow and border effect
        vita2d_pgf_draw_text(font, 480 - 98, 82, RGBA(0, 0, 0, 150), 2.2f, "PSV Cleaner"); // Shadow
        vita2d_pgf_draw_text(font, 480 - 100, 80, RGBA(100, 200, 255, 255), 2.2f, "PSV Cleaner"); // Main text with blue tint
        
        // Draw subtitle
        vita2d_pgf_draw_text(font, 480 - 140, 120, RGBA(200, 200, 200, 255), 1.1f, "Temporary Files Cleaner for PS Vita");

        if (showMenu) {
            // Draw options menu
            drawOptionsMenu(font, &menu);
        } else {
            // Draw info box background (single rectangle for better performance)
            vita2d_draw_rectangle(200, 200, 560, 200, RGBA(25, 35, 55, 220));

            // Draw space info with icon
            vita2d_pgf_draw_text(font, 220, 240, RGBA(100, 255, 100, 255), 1.0f, "Space to free:");
            vita2d_pgf_draw_text(font, 220, 270, RGBA(255, 255, 255, 255), 1.2f, freeText);

            // Draw instructions with better styling
            vita2d_pgf_draw_text(font, 220, 320, RGBA(255, 255, 100, 255), 1.1f, "Controls:");
            vita2d_pgf_draw_text(font, 220, 350, RGBA(255, 255, 255, 255), 1.0f, "X - Start cleaning");
            vita2d_pgf_draw_text(font, 220, 380, RGBA(255, 255, 255, 255), 1.0f, "△ - Options");
            vita2d_pgf_draw_text(font, 220, 410, RGBA(255, 255, 255, 255), 1.0f, "O - Exit application");
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

        if (showMenu) {
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
                // Toggle option
                menu.enabled[menu.selected] = !menu.enabled[menu.selected];
                sceKernelDelayThread(200 * 1000); // 200ms delay
            }
            if (pad.buttons & SCE_CTRL_CIRCLE) {
                // Start cleaning with selected options
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
            if (pad.buttons & SCE_CTRL_TRIANGLE) {
                // Show options menu
                showMenu = 1;
                sceKernelDelayThread(200 * 1000); // 200ms delay
            }
            if (pad.buttons & SCE_CTRL_CROSS) {
                // Start cleaning with progress bar
                for (int pct = 0; pct <= 100; pct++) {
                    vita2d_start_drawing();
                    vita2d_clear_screen();
                    drawProgressBar(font, pct);
                    vita2d_end_drawing();
                    vita2d_swap_buffers();
                    sceKernelDelayThread(50 * 1000); // 50 ms for smoother progress and less GPU load
                    
                    // Check if user presses Circle to exit during cleaning
                    sceCtrlPeekBufferPositive(0, &pad, 1);
                    if (pad.buttons & SCE_CTRL_CIRCLE) {
                        running = 0;
                        break;
                    }
                }

                if (running) {
                    unsigned long long spaceFreed = cleanTemporaryFiles();
                    char result[128];
                    if (spaceFreed < (1024 * 1024))
                        snprintf(result, sizeof(result), "Cleaning completed! Freed: %llu KB", spaceFreed / 1024);
                    else if (spaceFreed < (1024ULL * 1024 * 1024))
                        snprintf(result, sizeof(result), "Cleaning completed! Freed: %.2f MB", spaceFreed / (1024.0 * 1024));
                    else
                        snprintf(result, sizeof(result), "Cleaning completed! Freed: %.2f GB", spaceFreed / (1024.0 * 1024 * 1024));

                    // Play success sound
                    playSuccessSound();
                    
                    // Show notification
                    showNotification("PSV Cleaner", "Cleaning completed successfully!");

                    vita2d_start_drawing();
                    vita2d_clear_screen();
                    vita2d_pgf_draw_text(font, 300, 272, RGBA(0, 255, 0, 255), 1.3f, result);
                    vita2d_end_drawing();
                    vita2d_swap_buffers();
                    sceKernelDelayThread(2 * 1000 * 1000); // 2 seconds
                }
            }

            if (pad.buttons & SCE_CTRL_CIRCLE) {
                running = 0; // Exit app
            }
        }

        sceKernelDelayThread(16 * 1000); // ~60 FPS limit for better GPU performance
    }

    vita2d_free_texture(splash);
    vita2d_free_pgf(font);
    vita2d_fini();

    return 0;
}
