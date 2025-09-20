#ifndef PSV_CLEANER_CORE_H
#define PSV_CLEANER_CORE_H

#include <stddef.h>

// List of temporary folders to clean
extern const char *TEMP_PATHS[];
extern const size_t TEMP_PATHS_COUNT;

// Function declarations
unsigned long long calculateTempSize();
unsigned long long cleanTemporaryFiles();
void deleteRecursive(const char *path);
void forceDeleteDumpFiles();
void aggressiveDumpCleanup();

#endif // PSV_CLEANER_CORE_H
