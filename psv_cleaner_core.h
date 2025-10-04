#ifndef PSV_CLEANER_CORE_H
#define PSV_CLEANER_CORE_H

#include <stddef.h>

// List of temporary folders to clean
extern const char *TEMP_PATHS[];
extern const size_t TEMP_PATHS_COUNT;

// Structure for file preview
typedef struct {
    char path[512];
    unsigned long long size;
} FileInfo;

typedef struct {
    FileInfo *files;
    int count;
    int capacity;
    unsigned long long totalSize;
} FileList;

// Global counter for deleted files
extern int g_deletedFilesCount;

// Cleanup counter functionality
int loadCleanupCounter();
void saveCleanupCounter(int count);

// Function declarations
unsigned long long calculateTempSize();
unsigned long long calculateTempSizeRecursive(const char *path);
unsigned long long cleanTemporaryFiles();
int getDeletedFilesCount();
void resetDeletedFilesCount();
void deleteRecursive(const char *path);
void forceDeleteDumpFiles();
void aggressiveDumpCleanup();

// Preview functions
FileList* createFileList();
void freeFileList(FileList *list);
void addFileToList(FileList *list, const char *path, unsigned long long size);
void scanFilesForPreview(FileList *list);

#endif // PSV_CLEANER_CORE_H
