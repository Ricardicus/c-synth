#include "test_temp.h"
#include "midi.h"
#include "file_browser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#define CHECK(c) do { if (!(c)) { fprintf(stderr,"%d: %s\n",__LINE__,#c); exit(1); } } while (0)
static void be32(FILE *file,unsigned long value) { for (int i=3;i>=0;--i) fputc((int)(value>>(i*8))&255,file); }
static void writeFile(const char *path,int format,int division,const unsigned char *a,size_t aSize,const unsigned char *b,size_t bSize)
{
    FILE *f=fopen(path,"wb"); CHECK(f);
    fwrite("MThd",1,4,f); be32(f,6); fputc(0,f); fputc(format,f); fputc(0,f); fputc(b ? 2 : 1,f);
    fputc(division>>8,f); fputc(division&255,f);
    fwrite("MTrk",1,4,f); be32(f,aSize); fwrite(a,1,aSize,f);
    if (b) { fwrite("MTrk",1,4,f); be32(f,bSize); fwrite(b,1,bSize,f); }
    CHECK(fclose(f)==0);
}
typedef struct { bool held[128]; int on,off,velocity; } Notes;
static void note(void *context,int n,bool on,int velocity)
{
    Notes *notes=context; notes->held[n]=on; if (on) { ++notes->on; notes->velocity=velocity; } else ++notes->off;
}
int main(void)
{
    char folder[1024]; CHECK(testTempDirectory(folder,sizeof(folder),"midi"));
    char path[1024]; snprintf(path,sizeof(path),"%s/song.mid",folder); char error[128];
    const unsigned char sustain[]={0,0xff,0x51,3,7,0xa1,0x20, 0,0x90,60,100, 0,0xb0,64,127,
        0x83,0x60,0x80,60,64, 0x83,0x60,0xb0,64,0, 0,0xff,0x2f,0};
    writeFile(path,0,480,sustain,sizeof(sustain),NULL,0);
    Notes notes={0}; MidiPlayer player; midiPlayerInit(&player,note,&notes);
    CHECK(midiPlayerLoad(&player,path,error,sizeof(error))==0 && player.song.durationUs==1000000);
    midiPlayerPlay(&player,2000000); CHECK(notes.held[60] && notes.velocity==100);
    midiPlayerTick(&player,2500000); CHECK(notes.held[60]); /* Pedal holds after key release. */
    midiPlayerStop(&player,2600000); CHECK(!notes.held[60] && !player.playing && player.positionUs==600000);
    midiPlayerPlay(&player,3000000); CHECK(notes.held[60]); /* Resume reconstructs sustained notes. */
    midiPlayerTick(&player,3400000); CHECK(!notes.held[60] && !player.playing);
    midiPlayerRestart(&player,4000000); CHECK(notes.held[60] && player.positionUs==0);
    const unsigned char tempo[]={0,0xff,0x51,3,7,0xa1,0x20, 0x83,0x60,0xff,0x51,3,15,0x42,0x40, 0x83,0x60,0xff,0x2f,0};
    const unsigned char running[]={0,0x90,60,80, 0x81,0x70,64,90, 0x81,0x70,60,0, 0x81,0x70,64,0, 0x81,0x70,0xff,0x2f,0};
    writeFile(path,1,480,tempo,sizeof(tempo),running,sizeof(running));
    CHECK(midiPlayerLoad(&player,path,error,sizeof(error))==0 && !notes.held[60]);
    CHECK(player.song.durationUs==1500000);
    midiPlayerPlay(&player,5000000); CHECK(notes.held[60]);
    midiPlayerTick(&player,5250000); CHECK(notes.held[64] && notes.velocity==90);
    midiPlayerTick(&player,5500000); CHECK(!notes.held[60] && notes.held[64]);
    midiPlayerTick(&player,6000000); CHECK(!notes.held[64]);
    midiPlayerTick(&player,6500000); CHECK(!player.playing);
    const unsigned char broken[]={0,60,100};
    writeFile(path,0,480,broken,sizeof(broken),NULL,0);
    size_t count=player.song.count;
    CHECK(midiPlayerLoad(&player,path,error,sizeof(error))==-1 && player.song.count==count);
    const unsigned char smpte[]={0,0x90,60,127,100,0x80,60,0,0,0xff,0x2f,0};
    writeFile(path,0,0xe764,smpte,sizeof(smpte),NULL,0);
    CHECK(midiPlayerLoad(&player,path,error,sizeof(error))==0 && player.song.durationUs==40000);
    midiPlayerDestroy(&player); CHECK(!notes.held[60]);
    char sub[1024]; snprintf(sub,sizeof(sub),"%s/sub",folder); CHECK(TEST_MKDIR(sub)==0);
    FileBrowser browser; CHECK(fileBrowserOpen(&browser,folder,error,sizeof(error))==0);
    CHECK(browser.count==2 && browser.entries[0].directory);
    char selected[1024]; CHECK(fileBrowserChoose(&browser,1,selected,sizeof(selected),error,sizeof(error))==1);
    CHECK(!strcmp(selected,path));
    CHECK(fileBrowserChoose(&browser,0,selected,sizeof(selected),error,sizeof(error))==0 && browser.count==0);
    CHECK(fileBrowserChoose(&browser,-1,selected,sizeof(selected),error,sizeof(error))==0 && browser.count==2);
    CHECK(remove(path)==0 && TEST_RMDIR(sub)==0 && TEST_RMDIR(folder)==0);
    puts("MIDI tempo, tracks, running status, velocity, sustain, transport, invalid files and browsing passed.");
    return 0;
}
