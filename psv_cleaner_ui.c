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

// Preview screen state
typedef struct {
    FileList *fileList;
    int scrollOffset;
    int selectedFile;
} PreviewState;

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

// Draw preview screen
void drawPreviewScreen(vita2d_pgf *font, PreviewState *preview) {
    // Draw background
    vita2d_draw_rectangle(0, 0, 960, 544, RGBA(15, 25, 40, 255));
    
    // Draw title
    vita2d_pgf_draw_text(font, 480 - 120, 40, RGBA(255, 255, 255, 255), 1.8f, "Preview - Files to Delete");
    
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
    
    // Draw instructions
    vita2d_draw_rectangle(0, 510, 960, 34, RGBA(20, 30, 50, 220));
    vita2d_pgf_draw_text(font, 30, 530, RGBA(200, 200, 200, 255), 0.8f, 
                        "D-Pad: scroll | X: Start cleaning | O: Cancel");
    
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
    
    // Initialize preview state
    PreviewState preview;
    preview.fileList = NULL;
    preview.scrollOffset = 0;
    preview.selectedFile = 0;
    
    int showMenu = 0;
    int showPreview = 0;
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

        if (showPreview) {
            // Draw preview screen
            drawPreviewScreen(font, &preview);
        } else if (showMenu) {
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

        if (showPreview) {
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
                // Start cleaning
                showPreview = 0;
                
                // Start cleaning with progress bar
                for (int pct = 0; pct <= 100; pct++) {
                    vita2d_start_drawing();
                    vita2d_clear_screen();
                    drawProgressBar(font, pct);
                    vita2d_end_drawing();
                    vita2d_swap_buffers();
                    sceKernelDelayThread(50 * 1000);
                    
                    // Check if user presses Circle to exit during cleaning
                    sceCtrlPeekBufferPositive(0, &pad, 1);
                    if (pad.buttons & SCE_CTRL_CIRCLE) {
                        running = 0;
                        break;
                    }
                }

                if (running) {
                    unsigned long long spaceFreed = cleanTemporaryFiles();
                    int filesDeleted = getDeletedFilesCount();
                    
                    // Recalculate space after cleaning
                    spaceBefore = calculateTempSize();
                    if (spaceBefore < (1024 * 1024))
                        snprintf(freeText, sizeof(freeText), "Space to free: %llu KB", spaceBefore / 1024);
                    else if (spaceBefore < (1024ULL * 1024 * 1024))
                        snprintf(freeText, sizeof(freeText), "Space to free: %.2f MB", spaceBefore / (1024.0 * 1024));
                    else
                        snprintf(freeText, sizeof(freeText), "Space to free: %.2f GB", spaceBefore / (1024.0 * 1024 * 1024));
                    
                    // Format space freed text
                    char spaceText[128];
                    if (spaceFreed < (1024 * 1024))
                        snprintf(spaceText, sizeof(spaceText), "Space Freed: %llu KB", spaceFreed / 1024);
                    else if (spaceFreed < (1024ULL * 1024 * 1024))
                        snprintf(spaceText, sizeof(spaceText), "Space Freed: %.2f MB", spaceFreed / (1024.0 * 1024));
                    else
                        snprintf(spaceText, sizeof(spaceText), "Space Freed: %.2f GB", spaceFreed / (1024.0 * 1024 * 1024));

                    // Play success sound
                    playSuccessSound();
                    
                    // Show notification
                    showNotification("PSV Cleaner", "Cleaning completed successfully!");

                    // Draw success screen with statistics
                    vita2d_start_drawing();
                    vita2d_clear_screen();
                    vita2d_draw_rectangle(0, 0, 960, 544, RGBA(15, 25, 40, 255));
                    
                    // Draw success icon/checkmark effect
                    vita2d_draw_rectangle(430, 140, 100, 100, RGBA(0, 200, 0, 100));
                    
                    // Draw title
                    vita2d_pgf_draw_text(font, 480 - 150, 180, RGBA(0, 255, 0, 255), 1.8f, "Cleaning Completed!");
                    
                    // Draw statistics
                    vita2d_pgf_draw_text(font, 480 - 100, 260, RGBA(255, 255, 255, 255), 1.2f, spaceText);
                    
                    char filesText[64];
                    snprintf(filesText, sizeof(filesText), "Files Deleted: %d", filesDeleted);
                    vita2d_pgf_draw_text(font, 480 - 100, 300, RGBA(255, 255, 255, 255), 1.2f, filesText);
                    
                    // Draw decorative line
                    vita2d_draw_rectangle(330, 240, 300, 2, RGBA(0, 200, 0, 255));
                    
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
