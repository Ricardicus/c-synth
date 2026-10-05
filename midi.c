#include "midi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define MIDI_MAX_BYTES (16*1024*1024)
#define MIDI_MAX_EVENTS 1000000
#define EVENT_NOTE_ON 1
#define EVENT_NOTE_OFF 2
#define EVENT_CONTROL 3
#define EVENT_TEMPO 4
#define EVENT_END 5

typedef struct { const unsigned char *at, *end; } Reader;
static int byte(Reader *r) { return r->at < r->end ? *r->at++ : -1; }
static bool vlq(Reader *r, uint32_t *value)
{
    *value = 0;
    for (int i = 0; i < 4; ++i) {
        int b = byte(r); if (b < 0) return false;
        *value = (*value << 7) | (b & 127);
        if (!(b & 128)) return true;
    }
    return false;
}
static uint32_t be32(const unsigned char *p) { return (uint32_t)p[0]<<24 | (uint32_t)p[1]<<16 | (uint32_t)p[2]<<8 | p[3]; }
static unsigned int be16(const unsigned char *p) { return (unsigned int)p[0]<<8 | p[1]; }
static bool append(MidiSong *song, size_t *capacity, MidiEvent event)
{
    if (song->count == MIDI_MAX_EVENTS) return false;
    if (song->count == *capacity) {
        size_t next = *capacity ? *capacity * 2 : 256;
        if (next > MIDI_MAX_EVENTS) next = MIDI_MAX_EVENTS;
        MidiEvent *events = realloc(song->events, next * sizeof(*events));
        if (!events) return false;
        song->events = events; *capacity = next;
    }
    event.order = (uint32_t)song->count;
    song->events[song->count++] = event;
    return true;
}
static bool track(Reader *r, MidiSong *song, size_t *capacity)
{
    uint64_t tick = 0; int running = 0;
    while (r->at < r->end) {
        uint32_t delta; if (!vlq(r,&delta)) return false; tick += delta;
        int status = byte(r), first = -1;
        if (status < 0) return false;
        if (status < 128) { if (!running) return false; first = status; status = running; }
        MidiEvent event = {.tick=tick};
        if (status == 0xff) {
            running = 0;
            int type = byte(r); uint32_t length;
            if (type < 0 || !vlq(r,&length) || (size_t)(r->end-r->at) < length) return false;
            if (type == 0x51) {
                if (length != 3) return false;
                event.kind = EVENT_TEMPO; event.tempo = (uint32_t)r->at[0]<<16 | (uint32_t)r->at[1]<<8 | r->at[2];
                if (!event.tempo || !append(song,capacity,event)) return false;
            } else if (type == 0x2f) {
                if (length != 0) return false;
                event.kind = EVENT_END;
                return append(song,capacity,event);
            }
            r->at += length; continue;
        }
        if (status == 0xf0 || status == 0xf7) {
            running = 0; uint32_t length;
            if (!vlq(r,&length) || (size_t)(r->end-r->at) < length) return false;
            r->at += length; continue;
        }
        if (status < 0x80 || status >= 0xf0) return false;
        running = status; int type = status >> 4;
        int a = first >= 0 ? first : byte(r), b = (type == 12 || type == 13) ? 0 : byte(r);
        if (a < 0 || a > 127 || b < 0 || b > 127) return false;
        event.channel = status & 15; event.note = (unsigned char)a; event.value = (unsigned char)b;
        if (type == 8 || (type == 9 && b == 0)) event.kind = EVENT_NOTE_OFF;
        else if (type == 9) event.kind = EVENT_NOTE_ON;
        else if (type == 11 && (a == 64 || a == 120 || a == 123)) event.kind = EVENT_CONTROL;
        if (event.kind && !append(song,capacity,event)) return false;
    }
    MidiEvent end = {.tick=tick,.kind=EVENT_END};
    return append(song,capacity,end);
}
static int compare(const void *a, const void *b)
{
    const MidiEvent *x=a, *y=b;
    if (x->tick != y->tick) return x->tick < y->tick ? -1 : 1;
    return x->order < y->order ? -1 : x->order > y->order;
}
void midiSongDestroy(MidiSong *song) { free(song->events); *song = (MidiSong){0}; }
int midiRead(const char *path, MidiSong *song, char *error, size_t size)
{
    FILE *f = fopen(path,"rb");
    if (!f) { snprintf(error,size,"Cannot open MIDI file."); return -1; }
    if (fseek(f,0,SEEK_END)) { fclose(f); snprintf(error,size,"Cannot read MIDI file."); return -1; }
    long length = ftell(f);
    if (length < 14 || length > MIDI_MAX_BYTES || fseek(f,0,SEEK_SET)) {
        fclose(f); snprintf(error,size,"MIDI file is empty, invalid, or too large."); return -1;
    }
    unsigned char *data = malloc((size_t)length);
    if (!data) { fclose(f); snprintf(error,size,"Cannot allocate MIDI data."); return -1; }
    size_t got = fread(data,1,(size_t)length,f); fclose(f);
    MidiSong parsed = {0}; size_t capacity = 0;
    const char *issue = "Invalid or truncated MIDI file.";
    if (got != (size_t)length || memcmp(data,"MThd",4)) goto invalid;
    uint32_t header = be32(data+4);
    if (header < 6 || header > (uint32_t)length-8) goto invalid;
    unsigned int format=be16(data+8), tracks=be16(data+10), division=be16(data+12);
    if (format > 1) { issue="MIDI format 2 is not supported; use format 0 or 1."; goto invalid; }
    if (!tracks || (format == 0 && tracks != 1) || !division) goto invalid;
    double ticksPerSecond = 0;
    if (division & 0x8000) {
        int fps = 256-(int)(division>>8), subframes = division&255;
        if (!subframes || (fps!=24 && fps!=25 && fps!=29 && fps!=30)) goto invalid;
        ticksPerSecond = (fps==29 ? 30000.0/1001 : fps) * subframes;
    }
    Reader file = {data+8+header,data+length};
    for (unsigned int i=0;i<tracks;++i) {
        if (file.end-file.at < 8 || memcmp(file.at,"MTrk",4)) goto invalid;
        uint32_t bytes = be32(file.at+4); file.at += 8;
        if ((size_t)(file.end-file.at) < bytes) goto invalid;
        Reader contents = {file.at,file.at+bytes};
        if (!track(&contents,&parsed,&capacity)) goto invalid;
        file.at += bytes;
    }
    qsort(parsed.events,parsed.count,sizeof(*parsed.events),compare);
    uint64_t lastTick=0; uint32_t tempo=500000; long double time=0;
    for (size_t i=0;i<parsed.count;++i) {
        MidiEvent *e=&parsed.events[i];
        time += (e->tick-lastTick) * (ticksPerSecond ? 1000000.0L/ticksPerSecond : (long double)tempo/division);
        if (time > 86400000000.0L) { issue="MIDI duration exceeds 24 hours."; goto invalid; }
        e->timeUs=(uint64_t)llroundl(time); lastTick=e->tick;
        if (e->kind==EVENT_TEMPO) tempo=e->tempo;
    }
    parsed.durationUs=parsed.count ? parsed.events[parsed.count-1].timeUs : 0;
    free(data); *song=parsed; return 0;
invalid:
    midiSongDestroy(&parsed); free(data); snprintf(error,size,"%s",issue); return -1;
}
static void syncNote(MidiPlayer *p,int note,bool notify)
{
    bool held=false; int velocity=0;
    for (int ch=0;ch<16;++ch) {
        if (p->pressed[ch][note]>0 || p->sustained[ch][note]) {
            held=true; if (p->velocities[ch][note]>velocity) velocity=p->velocities[ch][note];
        }
    }
    if (notify && (held!=p->held[note] || (held && velocity!=p->heldVelocity[note])) && p->callback)
        p->callback(p->context,note,held,velocity);
    p->held[note]=held; p->heldVelocity[note]=(unsigned char)velocity;
}
static void apply(MidiPlayer *p,const MidiEvent *e,bool notify)
{
    int ch=e->channel, note=e->note;
    if (e->kind==EVENT_NOTE_ON) {
        p->velocities[ch][note]=e->value;
        if (p->pressed[ch][note]<UINT16_MAX) ++p->pressed[ch][note];
        syncNote(p,note,notify);
    } else if (e->kind==EVENT_NOTE_OFF) {
        if (p->pressed[ch][note]) {
            --p->pressed[ch][note];
            if (p->pedal[ch] && !p->pressed[ch][note]) p->sustained[ch][note]=true;
        }
        syncNote(p,note,notify);
    } else if (e->kind==EVENT_CONTROL) {
        if (note==64) {
            p->pedal[ch]=e->value>=64;
            if (!p->pedal[ch]) for (int n=0;n<128;++n) { p->sustained[ch][n]=false; syncNote(p,n,notify); }
        } else for (int n=0;n<128;++n) {
            if (note==123 && p->pedal[ch] && p->pressed[ch][n]) p->sustained[ch][n]=true;
            p->pressed[ch][n]=0;
            if (note==120) p->sustained[ch][n]=false;
            syncNote(p,n,notify);
        }
    }
}
static void releaseAll(MidiPlayer *p)
{
    for (int n=0;n<128;++n) if (p->held[n] && p->callback) p->callback(p->context,n,false,0);
    memset(p->pressed,0,sizeof(p->pressed)); memset(p->sustained,0,sizeof(p->sustained));
    memset(p->velocities,0,sizeof(p->velocities)); memset(p->heldVelocity,0,sizeof(p->heldVelocity));
    memset(p->pedal,0,sizeof(p->pedal)); memset(p->held,0,sizeof(p->held));
}
void midiPlayerInit(MidiPlayer *p,MidiNoteCallback callback,void *context) { *p=(MidiPlayer){.callback=callback,.context=context}; }
int midiPlayerLoad(MidiPlayer *p,const char *path,char *error,size_t size)
{
    MidiSong song={0}; if (midiRead(path,&song,error,size)) return -1;
    releaseAll(p); midiSongDestroy(&p->song); p->song=song;
    p->cursor=0; p->positionUs=0; p->playing=false; return 0;
}
void midiPlayerTick(MidiPlayer *p,uint64_t nowUs)
{
    if (!p->playing) return;
    p->positionUs=nowUs>=p->startUs ? nowUs-p->startUs : 0;
    if (p->positionUs>p->song.durationUs) p->positionUs=p->song.durationUs;
    while (p->cursor<p->song.count && p->song.events[p->cursor].timeUs<=p->positionUs)
        apply(p,&p->song.events[p->cursor++],true);
    if (p->cursor==p->song.count) { releaseAll(p); p->playing=false; }
}
void midiPlayerPlay(MidiPlayer *p,uint64_t nowUs)
{
    if (p->playing || !p->song.count) return;
    if (p->positionUs>=p->song.durationUs) { p->positionUs=0; p->cursor=0; }
    releaseAll(p);
    for (size_t i=0;i<p->cursor;++i) apply(p,&p->song.events[i],false);
    for (int n=0;n<128;++n) if (p->held[n] && p->callback) p->callback(p->context,n,true,p->heldVelocity[n]);
    p->startUs=nowUs-p->positionUs; p->playing=true; midiPlayerTick(p,nowUs);
}
void midiPlayerStop(MidiPlayer *p,uint64_t nowUs) { midiPlayerTick(p,nowUs); p->playing=false; releaseAll(p); }
void midiPlayerRestart(MidiPlayer *p,uint64_t nowUs) { midiPlayerStop(p,nowUs); p->cursor=0; p->positionUs=0; midiPlayerPlay(p,nowUs); }
void midiPlayerDestroy(MidiPlayer *p) { releaseAll(p); midiSongDestroy(&p->song); p->playing=false; }
