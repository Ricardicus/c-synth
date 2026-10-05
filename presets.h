#ifndef PRESETS_H
#define PRESETS_H
#include "synth_config.h"
#include <stddef.h>
#define PRESET_NAME_MAX 32
#define PRESET_LIBRARY_MAX 256
#define PRESET_PATH_MAX 1024

typedef struct {
    char name[PRESET_NAME_MAX + 1];
    char path[PRESET_PATH_MAX];
    SynthConfig config;
    bool builtin;
} StoredPreset;
typedef struct {
    StoredPreset *items;
    int count;
    char folder[PRESET_PATH_MAX];
    int skipped;
} PresetLibrary;
int presetRead(const char *path, char *name, SynthConfig *config, char *error, size_t size);
int presetWrite(const char *path, const char *name, const SynthConfig *config, char *error, size_t size);
int presetLibraryInit(PresetLibrary *library, const char *folder, char *error, size_t size);
void presetLibraryDestroy(PresetLibrary *library);
/* Saves a new file; duplicate names are rejected. Returns its index or -1. */
int presetLibrarySave(PresetLibrary *library, const char *name, const SynthConfig *config, char *error, size_t size);
int presetLibraryDelete(PresetLibrary *library, int index, char *error, size_t size);
#endif
