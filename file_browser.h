#ifndef FILE_BROWSER_H
#define FILE_BROWSER_H
#include <stdbool.h>
#include <stddef.h>
#define BROWSER_MAX 512
#define BROWSER_PATH_MAX 1024
typedef enum { BROWSER_MIDI, BROWSER_AUDIO, BROWSER_SAMPLE_MAP } BrowserFilter;
typedef struct { char name[256]; bool directory; } BrowserEntry;
typedef struct {
    char directory[BROWSER_PATH_MAX];
    BrowserEntry entries[BROWSER_MAX];
    int count, scroll, selected;
    BrowserFilter filter;
} FileBrowser;
int fileBrowserOpenFiltered(FileBrowser *browser, const char *directory, BrowserFilter filter, char *error, size_t size);
int fileBrowserOpen(FileBrowser *browser, const char *directory, char *error, size_t size);
/* Entry -1 means parent. Returns 1 for a matching file, 0 for a directory, -1 on error. */
int fileBrowserChoose(FileBrowser *browser, int entry, char *path, size_t pathSize, char *error, size_t size);
#endif
