#include "presets.h"
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define MAKE_DIRECTORY(p) _mkdir(p)
#else
#include <dirent.h>
#define MAKE_DIRECTORY(p) mkdir(p, 0755)
#endif

static int fail(char *error, size_t size, const char *format, ...)
{
    va_list args; va_start(args, format); vsnprintf(error, size, format, args); va_end(args); return -1;
}
static bool validName(const char *name)
{
    size_t n = strlen(name);
    if (!n || n > PRESET_NAME_MAX || name[0] == ' ' || name[n-1] == ' ') return false;
    bool letter = false;
    for (size_t i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)name[i];
        bool alnum = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        if (!alnum && c != ' ' && c != '-' && c != '_') return false;
        letter |= alnum;
    }
    return letter;
}
static void filename(const char *name, char *result)
{
    for (; *name; ++name) *result++ = *name == ' ' ? '-' : (char)tolower((unsigned char)*name);
    strcpy(result, ".synth");
}
static bool allValid(const SynthConfig *config)
{
    if (!synthConfigValid(config)) return false;
    SynthConfig all = *config; all.layerCount = SYNTH_MAX_LAYERS;
    for (int l=0; l<SYNTH_MAX_LAYERS; ++l) all.layers[l].fm.operatorCount = FM_MAX_OPERATORS;
    return synthConfigValid(&all);
}
int presetWrite(const char *path, const char *name, const SynthConfig *c, char *error, size_t size)
{
    if (!validName(name)) return fail(error, size, "Use 1-32 letters, numbers, spaces, - or _.");
    if (!allValid(c)) return fail(error, size, "Invalid synth settings.");
    FILE *f = fopen(path, "wx");
    if (!f) return fail(error, size, errno == EEXIST ? "That preset already exists." : "Cannot save preset: %s", strerror(errno));
    fprintf(f, "SYNTH_PRESET 1\nname %s\n", name);
    const SynthEnvelopeConfig *e = &c->outputEnvelope;
    fprintf(f, "master %d %d %d %d\n", e->attackMs, e->decayMs, e->sustainPercent, e->releaseMs);
    const SynthEffectsConfig *fx = &c->effects;
    fprintf(f, "effects %.17g %.17g %.17g %.17g %.17g %.17g\n", fx->echoMix, fx->echoDelayMs,
            fx->echoFeedback, fx->reverbMix, fx->reverbRoom, fx->reverbDamping);
    fprintf(f, "layers %d\n", c->layerCount);
    for (int l = 0; l < SYNTH_MAX_LAYERS; ++l) {
        const SynthLayerConfig *layer = &c->layers[l];
        fprintf(f, "layer %.17g %.17g %d\n", layer->gain, layer->detuneCents, layer->fm.operatorCount);
        for (int k = 0; k < FM_MAX_OPERATORS; ++k) {
            const FmOperatorConfig *op = &layer->fm.operators[k];
            fprintf(f, "op %s %.17g %.17g %.17g %.17g %.17g %d %.17g %d %d %d %d\n",
                    oscillatorWaveformName(op->waveform), op->pulseWidth, op->vibratoRateHz,
                    op->vibratoDepthCents, op->rm, op->ratio, op->indexMode, op->decayRate,
                    op->attackMs, op->decayMs, op->sustainPercent, op->releaseMs);
        }
    }
    bool broken = ferror(f) != 0;
    if (fclose(f)) broken = true;
    if (broken) { remove(path); return fail(error, size, "Could not finish writing preset."); }
    return 0;
}
static bool tag(FILE *file, const char *expected)
{
    char token[32]; return fscanf(file," %31s",token)==1 && !strcmp(token,expected);
}
static bool readInt(FILE *file, int *value)
{
    char token[64], *end;
    if (fscanf(file," %63s",token)!=1) return false;
    errno=0; long number=strtol(token,&end,10);
    if (end==token || *end || errno || number<INT_MIN || number>INT_MAX) return false;
    *value=(int)number; return true;
}
static bool readReal(FILE *file, double *value)
{
    char token[64], *end;
    if (fscanf(file," %63s",token)!=1) return false;
    errno=0; double number=strtod(token,&end);
    if (end==token || *end || !isfinite(number) || errno==ERANGE) return false;
    *value=number; return true;
}

int presetRead(const char *path, char *name, SynthConfig *config, char *error, size_t size)
{
    FILE *f = fopen(path, "r");
    if (!f) return fail(error, size, "Cannot open preset: %s", strerror(errno));
    SynthConfig c = synthDefaultConfig();
    char title[128], readName[PRESET_NAME_MAX+1];
    if (!fgets(title,sizeof(title),f)) goto invalid;
    title[strcspn(title,"\r\n")]=0;
    if (strcmp(title,"SYNTH_PRESET 1")) goto invalid;
    if (!fgets(title, sizeof(title), f) || strncmp(title, "name ", 5)) goto invalid;
    title[strcspn(title, "\r\n")] = 0;
    if (!validName(title + 5)) goto invalid;
    strcpy(readName, title + 5);
    SynthEnvelopeConfig *e = &c.outputEnvelope;
    if (!tag(f,"master") || !readInt(f,&e->attackMs) || !readInt(f,&e->decayMs) ||
        !readInt(f,&e->sustainPercent) || !readInt(f,&e->releaseMs)) goto invalid;
    SynthEffectsConfig *fx = &c.effects;
    if (!tag(f,"effects") || !readReal(f,&fx->echoMix) || !readReal(f,&fx->echoDelayMs) ||
        !readReal(f,&fx->echoFeedback) || !readReal(f,&fx->reverbMix) || !readReal(f,&fx->reverbRoom) ||
        !readReal(f,&fx->reverbDamping)) goto invalid;
    if (!tag(f,"layers") || !readInt(f,&c.layerCount)) goto invalid;
    for (int l = 0; l < SYNTH_MAX_LAYERS; ++l) {
        SynthLayerConfig *layer = &c.layers[l];
        if (!tag(f,"layer") || !readReal(f,&layer->gain) || !readReal(f,&layer->detuneCents) ||
            !readInt(f,&layer->fm.operatorCount)) goto invalid;
        for (int k = 0; k < FM_MAX_OPERATORS; ++k) {
            FmOperatorConfig *op = &layer->fm.operators[k];
            char wave[24]; int mode;
            if (!tag(f,"op") || fscanf(f," %23s",wave)!=1 || !readReal(f,&op->pulseWidth) ||
                !readReal(f,&op->vibratoRateHz) || !readReal(f,&op->vibratoDepthCents) ||
                !readReal(f,&op->rm) || !readReal(f,&op->ratio) || !readInt(f,&mode) ||
                !readReal(f,&op->decayRate) || !readInt(f,&op->attackMs) || !readInt(f,&op->decayMs) ||
                !readInt(f,&op->sustainPercent) || !readInt(f,&op->releaseMs)) goto invalid;
            if (!oscillatorParseWaveform(wave, &op->waveform) || mode < FM_INDEX_SUSTAIN || mode > FM_INDEX_ADSR) goto invalid;
            op->indexMode = (FmIndexMode)mode;
        }
    }
    int ch;
    while ((ch = fgetc(f)) != EOF) if (!isspace((unsigned char)ch)) goto invalid;
    if (ferror(f) || !allValid(&c)) goto invalid;
    fclose(f); strcpy(name, readName); *config = c; return 0;
invalid:
    fclose(f); return fail(error, size, "Invalid or unsupported preset file.");
}
static int ensureFolder(const char *folder, char *error, size_t size)
{
    if (MAKE_DIRECTORY(folder) == 0) return 0;
    struct stat st;
    if (errno == EEXIST && stat(folder, &st) == 0 && (st.st_mode & S_IFDIR)) return 0;
    return fail(error, size, "Cannot create preset folder: %s", strerror(errno));
}
static int compare(const void *a, const void *b) { return strcmp(((const StoredPreset *)a)->name, ((const StoredPreset *)b)->name); }
static void loadUser(PresetLibrary *lib, const char *file)
{
    size_t length = strlen(file);
    if (length < 7 || strcmp(file + length - 6, ".synth") || strchr(file, '/') || strchr(file, '\\')) return;
    if (lib->count == PRESET_LIBRARY_MAX) { ++lib->skipped; return; }
    StoredPreset preset = {0}; char error[128];
    int n = snprintf(preset.path, sizeof(preset.path), "%s/user/%s", lib->folder, file);
    if (n < 0 || (size_t)n >= sizeof(preset.path) || presetRead(preset.path, preset.name, &preset.config, error, sizeof(error))) {
        ++lib->skipped; return;
    }
    lib->items[lib->count++] = preset;
}
int presetLibraryInit(PresetLibrary *lib, const char *folder, char *error, size_t size)
{
    *lib = (PresetLibrary){0};
    if (strlen(folder) > PRESET_PATH_MAX - 80) return fail(error, size, "Preset folder path is too long.");
    strcpy(lib->folder, folder);
    lib->items = calloc(PRESET_LIBRARY_MAX, sizeof(*lib->items));
    if (!lib->items) return fail(error, size, "Cannot allocate preset library.");
    for (int i = 0; i < SYNTH_PRESET_COUNT; ++i) {
        StoredPreset *p = &lib->items[lib->count++];
        strcpy(p->name, synthPresetName(i)); p->config = synthPresetConfig(i); p->builtin = true;
        char file[PRESET_NAME_MAX + 7]; filename(p->name, file);
        snprintf(p->path, sizeof(p->path), "%s/factory/%s", folder, file);
        char name[PRESET_NAME_MAX+1], issue[128]; SynthConfig config;
        if (presetRead(p->path, name, &config, issue, sizeof(issue)) == 0) {
            strcpy(p->name, name); p->config = config;
        }
    }
#ifdef _WIN32
    char pattern[PRESET_PATH_MAX]; snprintf(pattern, sizeof(pattern), "%s/user/*.synth", folder);
    WIN32_FIND_DATAA data; HANDLE handle = FindFirstFileA(pattern, &data);
    if (handle != INVALID_HANDLE_VALUE) {
        do { if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) loadUser(lib, data.cFileName); } while (FindNextFileA(handle, &data));
        FindClose(handle);
    }
#else
    char user[PRESET_PATH_MAX]; snprintf(user, sizeof(user), "%s/user", folder);
    DIR *dir = opendir(user);
    if (dir) { struct dirent *entry; while ((entry = readdir(dir))) loadUser(lib, entry->d_name); closedir(dir); }
    else if (errno != ENOENT) {
        presetLibraryDestroy(lib); return fail(error, size, "Cannot read preset folder: %s", strerror(errno));
    }
#endif
    qsort(lib->items + SYNTH_PRESET_COUNT, (size_t)(lib->count - SYNTH_PRESET_COUNT), sizeof(*lib->items), compare);
    return 0;
}
void presetLibraryDestroy(PresetLibrary *lib) { free(lib->items); *lib = (PresetLibrary){0}; }
int presetLibrarySave(PresetLibrary *lib, const char *name, const SynthConfig *config, char *error, size_t size)
{
    if (!validName(name)) return fail(error, size, "Use 1-32 letters, numbers, spaces, - or _.");
    if (lib->count == PRESET_LIBRARY_MAX) return fail(error, size, "Preset library is full.");
    char file[PRESET_NAME_MAX + 7]; filename(name, file);
    for (int i = 0; i < lib->count; ++i) {
        char existing[PRESET_NAME_MAX + 7]; filename(lib->items[i].name, existing);
        if (!strcmp(file, existing)) return fail(error, size, "That preset name already exists.");
    }
    if (ensureFolder(lib->folder, error, size)) return -1;
    char user[PRESET_PATH_MAX]; snprintf(user, sizeof(user), "%s/user", lib->folder);
    if (ensureFolder(user, error, size)) return -1;
    StoredPreset p = {0}; strcpy(p.name, name); p.config = *config;
    snprintf(p.path, sizeof(p.path), "%s/user/%s", lib->folder, file);
    if (presetWrite(p.path, name, config, error, size)) return -1;
    lib->items[lib->count] = p;
    return lib->count++;
}
int presetLibraryDelete(PresetLibrary *lib, int index, char *error, size_t size)
{
    if (index < 0 || index >= lib->count || lib->items[index].builtin) return fail(error, size, "Factory presets cannot be deleted.");
    if (remove(lib->items[index].path)) return fail(error, size, "Cannot delete preset: %s", strerror(errno));
    memmove(lib->items + index, lib->items + index + 1, (size_t)(lib->count - index - 1) * sizeof(*lib->items));
    --lib->count; return 0;
}
