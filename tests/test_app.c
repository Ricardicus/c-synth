#include "test_temp.h"
/* Exercise the real application with a backend warning on an empty event
 * queue, reproducing the condition that previously caused a fatal exit. */
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <stdio.h>

static int backendNotices;
int pollWithBackendNotice(SDL_Event *event);
int waitWithBackendNotice(SDL_Event *event, int timeout);

#define SDL_PollEvent pollWithBackendNotice
#define SDL_WaitEventTimeout waitWithBackendNotice
#define main synthApplicationMain
#include "../main.c"
#undef main
#undef SDL_PollEvent
#undef SDL_WaitEventTimeout

static void backendNotice(void)
{
    ++backendNotices;
    SDL_SetError("Unknown touch device id -1, cannot reset");
}

int pollWithBackendNotice(SDL_Event *event)
{
    int result = SDL_PollEvent(event);
    if (result == 0)
        backendNotice();
    return result;
}

int waitWithBackendNotice(SDL_Event *event, int timeout)
{
    int result = SDL_WaitEventTimeout(event, timeout);
    if (result == 0)
        backendNotice();
    return result;
}

static int inputThread(void *userdata)
{
    (void)userdata;
    SDL_Delay(200);
    SDL_Event event = {0};
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = SDLK_a;
    event.key.keysym.scancode = SDL_SCANCODE_A;
    if (SDL_PushEvent(&event) != 1)
        return 1;
    SDL_Delay(100);
    SDL_Event click = {0};
    click.type = SDL_MOUSEBUTTONDOWN;
    click.button.button = SDL_BUTTON_LEFT;
    click.button.x = 100; click.button.y = 25;
    if (SDL_PushEvent(&click) != 1) return 1;
    SDL_Event select = {0};
    select.type = SDL_KEYDOWN;
    select.key.keysym.sym = SDLK_END;
    if (SDL_PushEvent(&select) != 1) return 1;
    select.key.keysym.sym = SDLK_RETURN;
    if (SDL_PushEvent(&select) != 1) return 1;
    click.button.x = 1160; click.button.y = 194;
    if (SDL_PushEvent(&click) != 1) return 1;
    SDL_Event motion = {0};
    motion.type = SDL_MOUSEMOTION;
    motion.motion.x = 1160; motion.motion.y = 164;
    if (SDL_PushEvent(&motion) != 1) return 1;
    click.type = SDL_MOUSEBUTTONUP;
    if (SDL_PushEvent(&click) != 1) return 1;
    SDL_Delay(100);
    event.type = SDL_KEYUP;
    if (SDL_PushEvent(&event) != 1)
        return 1;
    SDL_Delay(100);
    event.type = SDL_QUIT;
    return SDL_PushEvent(&event) == 1 ? 0 : 1;
}

int main(void)
{
    char folder[1024];
    if (!testTempDirectory(folder,sizeof(folder),"app")) return 1;
    SDL_setenv("SYNTH_PRESET_DIR",folder,1);
    SDL_SetMainReady();
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
    if (SDL_Init(SDL_INIT_EVENTS) != 0)
        return 1;
    SDL_Thread *thread = SDL_CreateThread(inputThread, "test-input", NULL);
    if (thread == NULL)
        return 1;
    char *args[] = {"synth", "--layers", "2", "--op2-vibrato-depth", "15"};
    int result = synthApplicationMain(5, args);
    int threadResult;
    SDL_WaitThread(thread, &threadResult);
    SDL_Quit();
    if (result != 0 || threadResult != 0 || backendNotices == 0) {
        fprintf(stderr, "Application regression: result=%d input=%d notices=%d\n",
                result, threadResult, backendNotices);
        return 1;
    }
    if (TEST_RMDIR(folder)!=0) return 1;
    puts("Application survives backend touch warnings and handles keys, rendering, and quit.");
    return 0;
}
