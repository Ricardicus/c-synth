#ifndef MIDI_H
#define MIDI_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct { uint64_t tick, timeUs; uint32_t order, tempo; unsigned char kind, channel, note, value; } MidiEvent;
typedef struct { MidiEvent *events; size_t count; uint64_t durationUs; } MidiSong;
typedef void (*MidiNoteCallback)(void *context, int note, bool on, int velocity);
typedef struct {
    MidiSong song;
    size_t cursor;
    uint64_t positionUs, startUs;
    bool playing;
    uint16_t pressed[16][128];
    bool sustained[16][128], pedal[16], held[128];
    unsigned char velocities[16][128], heldVelocity[128];
    MidiNoteCallback callback;
    void *context;
} MidiPlayer;
int midiRead(const char *path, MidiSong *song, char *error, size_t size);
void midiSongDestroy(MidiSong *song);
void midiPlayerInit(MidiPlayer *player, MidiNoteCallback callback, void *context);
int midiPlayerLoad(MidiPlayer *player, const char *path, char *error, size_t size);
void midiPlayerPlay(MidiPlayer *player, uint64_t nowUs);
void midiPlayerStop(MidiPlayer *player, uint64_t nowUs);
void midiPlayerRestart(MidiPlayer *player, uint64_t nowUs);
void midiPlayerTick(MidiPlayer *player, uint64_t nowUs);
void midiPlayerDestroy(MidiPlayer *player);
#endif
