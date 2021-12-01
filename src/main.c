/*
 *  This file is part of Permafrost Engine. 
 *  Copyright (C) 2017-2020 Eduard Permyakov 
 *
 *  Permafrost Engine is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Permafrost Engine is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 *  Linking this software statically or dynamically with other modules is making 
 *  a combined work based on this software. Thus, the terms and conditions of 
 *  the GNU General Public License cover the whole combination. 
 *  
 *  As a special exception, the copyright holders of Permafrost Engine give 
 *  you permission to link Permafrost Engine with independent modules to produce 
 *  an executable, regardless of the license terms of these independent 
 *  modules, and to copy and distribute the resulting executable under 
 *  terms of your choice, provided that you also meet, for each linked 
 *  independent module, the terms and conditions of the license of that 
 *  module. An independent module is a module which is not derived from 
 *  or based on Permafrost Engine. If you modify Permafrost Engine, you may 
 *  extend this exception to your version of Permafrost Engine, but you are not 
 *  obliged to do so. If you do not wish to do so, delete this exception 
 *  statement from your version.
 *
 */

#include "main.h"
#include "asset_load.h"
#include "config.h"
#include "cursor.h"
#include "render/public/render.h"
#include "render/public/render_ctrl.h"
#include "lib/public/stb_image.h"
#include "lib/public/vec.h"
#include "lib/public/pf_string.h"
#include "script/public/script.h"
#include "game/public/game.h"
#include "navigation/public/nav.h"
#include "audio/public/audio.h"
#include "phys/public/phys.h"
#include "anim/public/anim.h"
#include "event.h"
#include "ui.h"
#include "pf_math.h"
#include "settings.h"
#include "session.h"
#include "perf.h"
#include "sched.h"

#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif


#define PF_VER_MAJOR 1
#define PF_VER_MINOR 0
#define PF_VER_PATCH 0

VEC_TYPE(event, SDL_Event)
VEC_IMPL(static inline, event, SDL_Event)

/*****************************************************************************/
/* GLOBAL VARIABLES                                                          */
/*****************************************************************************/

const char                      *g_basepath; /* write-once - path of the base directory */
unsigned long                    g_frame_idx = 0;

threadID_t                       g_main_thread_id;   /* write-once */
threadID_t                       g_render_thread_id; /* write-once */

/*****************************************************************************/
/* STATIC VARIABLES                                                          */
/*****************************************************************************/

static SDL_Window               *s_window;
static int                       s_window_width;
static int                       s_window_height;
static SDL_Surface              *s_loading_screen;

/* Flag to perform a single step of the simulation while the game is paused. 
 * Cleared at after performing the step. 
 */
static bool                      s_step_frame = false;
static bool                      s_quit = false; 
static vec_event_t               s_prev_tick_events;

static SDL_Thread               *s_render_thread;
static struct render_sync_state  s_rstate;

static int                       s_argc;
static char                    **s_argv;

/*****************************************************************************/
/* STATIC FUNCTIONS                                                          */
/*****************************************************************************/

static void process_sdl_events(void)
{
    PERF_ENTER();
    UI_InputBegin();

    vec_event_reset(&s_prev_tick_events);
    SDL_Event event;    
   
    while(SDL_PollEvent(&event)) {

        UI_HandleEvent(&event);
        vec_event_push(&s_prev_tick_events, event);

        switch(event.type) {

        case SDL_KEYDOWN:
            if(event.key.keysym.sym == SDLK_q && (event.key.keysym.mod & KMOD_LALT)) {
                s_quit = true; 
            }
	    else if (event.key.keysym.sym == SDLK_r && (event.key.keysym.mod & KMOD_LALT)) {
	      /* // TODO: Temp, "hot reload" by restarting python interpreter and current scene.. */
	      /* // Restart python */
	      /* S_Shutdown(); */
	      /* if(!S_Init(s_argv[0], g_basepath, UI_GetContext())) { */
	      /* 	fprintf(stderr, "Failed to reinitialize scripting subsystem\n"); */
	      /* 	exit(1); */
	      /* } */
	      /* // Restart scene by saving and loading */
	      /* /\* char buf[1024]; *\/ */
	      /* /\* sprintf(buf, "pf.save_session(%s)",  *\/ */
	      /* /\* if (!S_RunString( *\/ */

	      /* // Save sessions is broken... try it from the menus.. so I can't do this.. */
	      
	    }
            break;

        case SDL_USEREVENT:
            if(event.user.code == 0) {
                E_Global_Notify(EVENT_60HZ_TICK, NULL, ES_ENGINE); 
            }
            break;

        case SDL_WINDOWEVENT:
            if(event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                // Update stored size
                int out_w, out_h;
                SDL_GL_GetDrawableSize(s_window, &out_w, &out_h);
                
//                SDL_Rect rect;
//                SDL_GetDisplayUsableBounds(SDL_GetWindowDisplayIndex(SDL_GetWindowFromID(event.window.windowID)), &rect);
//                if (event.window.data1 > rect.w) {
//                    event.window.data1 = rect.w;
//                }
//                if (event.window.data2 > rect.h) {
//                    event.window.data2 = rect.h;
//                }
                
                s_window_width = event.window.data1;
                s_window_height = event.window.data2;
                
                printf("SDL_WINDOWEVENT_SIZE_CHANGED: SDL_GL_GetDrawableSize(): %d, %d; event.window: %d, %d\n", out_w, out_h, s_window_width, s_window_height);
            }
            break;
        default: 
            break;
        }
    }

    for(int i = 0; i < vec_size(&s_prev_tick_events); i++) {
        const SDL_Event *event = &vec_AT(&s_prev_tick_events, i);
        E_Global_Notify(event->type, (void*)event, ES_ENGINE);
    }

    UI_InputEnd();
    PERF_RETURN_VOID();
}

static void on_user_quit(void *user, void *event)
{
    s_quit = true;
}

static bool rstate_init(struct render_sync_state *rstate)
{
    rstate->start = false;

    rstate->sq_lock = SDL_CreateMutex();
    if(!rstate->sq_lock)
        goto fail_sq_lock;
        
    rstate->sq_cond = SDL_CreateCond();
    if(!rstate->sq_cond)
        goto fail_sq_cond;

    rstate->done = false;

    rstate->done_lock = SDL_CreateMutex();
    if(!rstate->done_lock)
        goto fail_done_lock;

    rstate->done_cond = SDL_CreateCond();
    if(!rstate->done_cond)
        goto fail_done_cond;

    rstate->swap_buffers = false;
    return true;

fail_done_cond:
    SDL_DestroyMutex(rstate->done_lock);
fail_done_lock:
    SDL_DestroyCond(rstate->sq_cond);
fail_sq_cond:
    SDL_DestroyMutex(rstate->sq_lock);
fail_sq_lock:
    return false;
}

static void rstate_destroy(struct render_sync_state *rstate)
{
    SDL_DestroyCond(rstate->done_cond);
    SDL_DestroyMutex(rstate->done_lock);
    SDL_DestroyCond(rstate->sq_cond);
    SDL_DestroyMutex(rstate->sq_lock);
}

static int render_thread_quit(void)
{
    SDL_LockMutex(s_rstate.sq_lock);
    s_rstate.quit = true;
    SDL_CondSignal(s_rstate.sq_cond);
    SDL_UnlockMutex(s_rstate.sq_lock);

    int ret;
    SDL_WaitThread(s_render_thread, &ret);
    return ret;
}

static void render_thread_start_work(void)
{
    SDL_LockMutex(s_rstate.done_lock);
    s_rstate.done = false;
    SDL_UnlockMutex(s_rstate.done_lock);

    SDL_LockMutex(s_rstate.sq_lock);
    s_rstate.start = true;
    SDL_CondSignal(s_rstate.sq_cond);
    SDL_UnlockMutex(s_rstate.sq_lock);
}

#include "glHacks.h"
void render_thread_wait_done(void)
{
    PERF_ENTER();

    SDL_LockMutex(s_rstate.done_lock);
    while(!s_rstate.done)
        SDL_CondWait(s_rstate.done_cond, s_rstate.done_lock);
    s_rstate.done = false;
    #ifdef __APPLE__
    if (s_rstate.swap_buffers) {
        // Finish the update part
        SDLOpenGLContext* nscontext = (SDLOpenGLContext*)SDL_GL_GetCurrentContext();
        CFStringRef aCFString;
        aCFString = CFStringCreateWithCString(NULL, "update", kCFStringEncodingUTF8);
        ((void(*)(void*, SEL))objc_msgSend)(nscontext, NSSelectorFromString(aCFString)); // Call the `update` method on the current SDLOpenGLContext object (which inherits from NSOpenGLContext : https://developer.apple.com/documentation/appkit/nsopenglcontext/1436135-update?language=objc )
        CFRelease(aCFString);
    }
    #endif
    SDL_UnlockMutex(s_rstate.done_lock);

    PERF_RETURN_VOID();
}

static void render_maybe_enable(void)
{
    /* Simulate a single frame after a session change without rendering 
     * it - this gives us a chance to handle this event without anyone 
     * noticing. */
    if(((uint64_t)g_frame_idx) - Session_ChangeTick() <= 1)
        return;
    s_rstate.swap_buffers = true;
}

static void fs_on_key_press(void *user, void *event)
{
    SDL_KeyboardEvent *key = &((SDL_Event*)event)->key;
    if(key->keysym.scancode != CONFIG_FRAME_STEP_HOTKEY)
        return;
    s_step_frame = true;
}

static bool frame_step_validate(const struct sval *new_val)
{
    return (new_val->type == ST_TYPE_BOOL);
}

static void frame_step_commit(const struct sval *new_val)
{
    if(new_val->as_bool) {
        E_Global_Register(SDL_KEYDOWN, fs_on_key_press, NULL, 
            G_PAUSED_FULL | G_PAUSED_UI_RUNNING);
    }else{
        E_Global_Unregister(SDL_KEYDOWN, fs_on_key_press); 
    }
}

static void engine_create_settings(void)
{
    ss_e status = Settings_Create((struct setting){
        .name = "pf.debug.paused_frame_step_enabled",
        .val = (struct sval) {
            .type = ST_TYPE_BOOL,
            .as_bool = false 
        },
        .prio = 0,
        .validate = frame_step_validate,
        .commit = frame_step_commit,
    });
    assert(status == SS_OKAY);
}

static SDL_Surface *engine_create_loading_screen(void)
{
    ASSERT_IN_MAIN_THREAD();
    SDL_Surface *ret = NULL;

    char fullpath[512];
    pf_snprintf(fullpath, sizeof(fullpath), "%s/%s", g_basepath, CONFIG_LOADING_SCREEN);

    int width, height, orig_format;
    unsigned char *image = stbi_load(fullpath, &width, &height, 
        &orig_format, STBI_rgb);

    if(!image) {
        fprintf(stderr, "Loading Screen: Failed to load image: %s\n", fullpath);
        goto fail_load_image;
    }

    ret = SDL_CreateRGBSurfaceWithFormat(0, width, height, 24, SDL_PIXELFORMAT_RGB24);

    if(!ret) {
        fprintf(stderr, "Loading Screen: Failed to create SDL surface: %s\n", SDL_GetError());    
        goto fail_surface;
    }

    memcpy(ret->pixels, image, width * height * 3);

fail_surface:
    stbi_image_free(image);
fail_load_image:
    return ret;
}

static void engine_set_icon(void)
{
    char iconpath[512];
    if(!Engine_GetArg("appicon", sizeof(iconpath), iconpath))
        return;

    char fullpath[512];
    pf_snprintf(fullpath, sizeof(fullpath), "%s/%s", g_basepath, iconpath);

    int width, height, orig_format;
    unsigned char *image = stbi_load(fullpath, &width, &height, 
        &orig_format, STBI_rgb_alpha);

    if(!image) {
        fprintf(stderr, "Failed to load client icon image: %s\n", fullpath);
        return;
    }

    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
    if(!surface) {
        fprintf(stderr, "Failed to create surface from client icon image: %s\n", fullpath);
        goto fail_surface;
    }

    memcpy(surface->pixels, image, width * height * 4);
    SDL_SetWindowIcon(s_window, surface);
    SDL_FreeSurface(surface);
fail_surface:
    free(image);
}

static bool engine_init(void)
{
    g_main_thread_id = thisThreadID();

    vec_event_init(&s_prev_tick_events);
    if(!vec_event_resize(&s_prev_tick_events, 8192))
        return false;

    if(!Perf_Init()) {
        fprintf(stderr, "Failed to initialize performance module.\n");
        goto fail_perf;
    }

    /* Initialize 'Settings' before any subsystem to allow all of them 
     * to register settings. */
    if(Settings_Init() != SS_OKAY) {
        fprintf(stderr, "Failed to initialize settings module.\n");
        goto fail_settings;
    }

    ss_e status;
    if((status = Settings_LoadFromFile()) != SS_OKAY) {
        fprintf(stderr, "Could not load settings from file: %s [status: %d]\n", 
            Settings_GetFile(), status);
    }

    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) {
        fprintf(stderr, "Failed to initialize SDL: %s\n", SDL_GetError());
        goto fail_sdl;
    }

    SDL_DisplayMode dm;
    SDL_GetDesktopDisplayMode(0, &dm);

    struct sval setting;
    int res[2] = {dm.w, dm.h};

    if(Settings_Get("pf.video.resolution", &setting) == SS_OKAY) {
        res[0] = (int)setting.as_vec2.x;
        res[1] = (int)setting.as_vec2.y;
    }

    enum pf_window_flags wf = PF_WF_BORDERLESS_WIN, extra_flags = 0;
    if(Settings_Get("pf.video.display_mode", &setting) == SS_OKAY) {
        wf = setting.as_int;
    }
    if(Settings_Get("pf.video.window_always_on_top", &setting) == SS_OKAY) {
        extra_flags = setting.as_bool ? SDL_WINDOW_ALWAYS_ON_TOP : 0;
    }

    R_InitAttributes();

    char appname[64] = "Permafrost Engine";
    Engine_GetArg("appname", sizeof(appname), appname);
    #ifdef __APPLE__
    SDL_Rect rect;
    SDL_GetDisplayUsableBounds(0, &rect);
    res[0] = rect.w;
    res[1] = rect.h;
    #endif
    s_window = SDL_CreateWindow(
        appname,
        #ifdef __APPLE__
        rect.x,
        rect.y,
        #else
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        #endif
        res[0], 
        res[1], 
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | wf | extra_flags);
    // Set initial window size
    s_window_width = res[0];
    s_window_height = res[1];

    s_loading_screen = engine_create_loading_screen();
    engine_set_icon();
    stbi_set_flip_vertically_on_load(true);

    // FIXME: This causes crash when trying to create a gl context for this window, so I commented it out temporarily (maybe related to https://discourse.libsdl.org/t/sdl-createwindowfrom-with-opengl/19737/4 ) :
    //Engine_LoadingScreen();

    if(!rstate_init(&s_rstate)) {
        fprintf(stderr, "Failed to initialize the render sync state.\n");
        goto fail_rstate;
    }

    struct render_init_arg rarg = (struct render_init_arg) {
        .in_window = s_window,
        .in_width = res[0],
        .in_height = res[1],
    };

    s_rstate.arg = &rarg;
    s_render_thread = R_Run(&s_rstate);

    if(!s_render_thread) {
        fprintf(stderr, "Failed to start the render thread.\n");
        goto fail_rthread;
    }
    //g_render_thread_id = SDL_GetThreadID(s_render_thread);
    g_render_thread_id = getThreadID(s_render_thread);
    printf("engine_init: g_render_thread_id was reported by " xstr(getThreadID()) "() as: %lu\n", g_render_thread_id);
    //assert(g_render_thread_id != 0); // https://wiki.libsdl.org/SDL_GetThreadID : "This thread identifier is as reported by the underlying operating system. If SDL is running on a platform that does not support threads the return value will always be zero." -- however, sometimes this works/doesn't work on macOS...

    render_thread_start_work();
    render_thread_wait_done();

    if(!rarg.out_success)
        goto fail_render_init;

    Perf_RegisterThread(g_main_thread_id, "main");
    Perf_RegisterThread(g_render_thread_id, "render");

    if(!Sched_Init()) {
        fprintf(stderr, "Failed to initialize scheduling module.\n");
        goto fail_sched;
    }

    if(!Session_Init()) {
        fprintf(stderr, "Failed to initialize session module.\n");
        goto fail_sesh;
    }

    if(!AL_Init()) {
        fprintf(stderr, "Failed to initialize asset-loading module.\n");
        goto fail_al;
    }

    if(!Cursor_InitDefault(g_basepath)) {
        fprintf(stderr, "Failed to initialize cursor module\n");
        goto fail_cursor;
    }
    Cursor_SetActive(CURSOR_POINTER);

    if(!E_Init()) {
        fprintf(stderr, "Failed to initialize event subsystem\n");
        goto fail_event;
    }

    if(!Entity_Init()) {
        fprintf(stderr, "Failed to initialize event subsystem\n");
        goto fail_entity;
    }

    if(!A_Init()) {
        fprintf(stderr, "Failed to initialize animation subsystem\n");
        goto fail_anim;
    }

    if(!G_Init()) {
        fprintf(stderr, "Failed to initialize game subsystem\n");
        goto fail_game;
    }

    if(!R_Init(g_basepath)) {
        fprintf(stderr, "Failed to intiaialize rendering subsystem\n");
        goto fail_render;
    }

    E_Global_Register(SDL_QUIT, on_user_quit, NULL, 
        G_RUNNING | G_PAUSED_UI_RUNNING | G_PAUSED_FULL);

    if(!UI_Init(g_basepath, s_window)) {
        fprintf(stderr, "Failed to initialize nuklear\n");
        goto fail_nuklear;
    }

    if(!S_Init(s_argv[0], g_basepath, UI_GetContext())) {
        fprintf(stderr, "Failed to initialize scripting subsystem\n");
        goto fail_script;
    }

    if(!N_Init()) {
        fprintf(stderr, "Failed to intialize navigation subsystem\n");
        goto fail_nav;
    }

    if(!Audio_Init()) {
        fprintf(stderr, "Failed to intialize audio subsystem\n");
        goto fail_audio;
    }

    if(!P_Projectile_Init()) {
        fprintf(stderr, "Failed to intialize physics subsystem\n");
        goto fail_phys;
    }

    engine_create_settings();
    s_rstate.swap_buffers = true;
    return true;

fail_phys:
    Audio_Shutdown();
fail_audio:
    N_Shutdown();
fail_nav:
    S_Shutdown();
fail_script:
    UI_Shutdown();
fail_nuklear:
    G_Shutdown();
fail_game:
    A_Shutdown();
fail_anim:
    Entity_Shutdown();
fail_entity:
    E_Shutdown();
fail_event:
fail_render:
    Cursor_FreeAll();
fail_cursor:
    AL_Shutdown();
fail_al:
    Session_Shutdown();
fail_sesh:
    Sched_Shutdown();
fail_sched:
fail_render_init:
    render_thread_quit();
fail_rthread:
    rstate_destroy(&s_rstate);
fail_rstate:
    if(s_loading_screen) {
        SDL_FreeSurface(s_loading_screen);
    }
    SDL_DestroyWindow(s_window);
    SDL_Quit();
fail_sdl:
    Settings_Shutdown();
fail_settings:
    Perf_Shutdown();
fail_perf:
    return false; 
}

static void engine_shutdown(void)
{
    P_Projectile_Shutdown();
    Audio_Shutdown();
    S_Shutdown();
    UI_Shutdown();

    /* Execute the last batch of commands that may have been queued by the 
     * shutdown routines. 
     */
    render_thread_start_work();
    render_thread_wait_done();
    render_thread_quit();

    /* 'Game' must shut down after 'Scripting'. There are still 
     * references to game entities in the Python interpreter that should get
     * their destructors called during 'S_Shutdown(), which will invoke the 
     * 'G_' API to remove them from the world.
     */
    G_Shutdown(); 
    A_Shutdown();
    Entity_Shutdown();
    N_Shutdown();

    Cursor_FreeAll();
    AL_Shutdown();
    E_Shutdown();
    Session_Shutdown();
    Sched_Shutdown();
    Perf_Shutdown();

    vec_event_destroy(&s_prev_tick_events);
    rstate_destroy(&s_rstate);

    if(s_loading_screen) {
        SDL_FreeSurface(s_loading_screen);
    }
    SDL_DestroyWindow(s_window); 
    SDL_Quit();

    Settings_Shutdown();
}

/*****************************************************************************/
/* EXTERN FUNCTIONS                                                          */
/*****************************************************************************/

/* Fills the framebuffer with the loading screen using SDL's software renderer. 
 * Used to set a loading screen immediately, even before the rendering subsystem 
 * is initialized, */
void Engine_LoadingScreen(void)
{
    ASSERT_IN_MAIN_THREAD();
    assert(s_window);

    /* Make sure the render therad doesn't overwrite the screen... */
    if(g_render_thread_id) {
        Engine_WaitRenderWorkDone();
    }

    SDL_Surface *win_surface = SDL_GetWindowSurface(s_window);
    SDL_Renderer *sw_renderer = SDL_CreateSoftwareRenderer(win_surface);
    assert(sw_renderer);

    SDL_SetRenderDrawColor(sw_renderer, 0x00, 0x00, 0x00, 0xff);
    SDL_RenderClear(sw_renderer);

    SDL_Texture *tex;
    if(s_loading_screen && (tex = SDL_CreateTextureFromSurface(sw_renderer, s_loading_screen))) {
        SDL_RenderCopy(sw_renderer, tex, NULL, NULL);
        SDL_DestroyTexture(tex);
    }

    SDL_UpdateWindowSurface(s_window);
    SDL_DestroyRenderer(sw_renderer);
}

int Engine_SetRes(int w, int h)
{
    SDL_DisplayMode dm = (SDL_DisplayMode) {
        .format = SDL_PIXELFORMAT_UNKNOWN,
        .w = w,
        .h = h,
        .refresh_rate = 0, /* Unspecified */
        .driverdata = NULL,
    };

//    SDL_SetWindowSize(s_window, w, h);
    SDL_SetWindowPosition(s_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    return SDL_SetWindowDisplayMode(s_window, &dm);
}

void Engine_SetDispMode(enum pf_window_flags wf)
{
    SDL_SetWindowFullscreen(s_window, wf & SDL_WINDOW_FULLSCREEN);
    SDL_SetWindowBordered(s_window, !(wf & (SDL_WINDOW_BORDERLESS | SDL_WINDOW_FULLSCREEN)));
    SDL_SetWindowPosition(s_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
}

void Engine_WinDrawableSize(int *out_w, int *out_h)
{
    //SDL_GL_GetDrawableSize(s_window, out_w, out_h); // Can't use UI on a thread other than the main thread on macOS, so don't do this line. Instead, return the stored values:
    *out_w = s_window_width;
    *out_h = s_window_height;
}

void Engine_FlushRenderWorkQueue(void)
{
    ASSERT_IN_MAIN_THREAD();

    /* Wait for the render thread to finish its' current batch */
    render_thread_start_work();
    render_thread_wait_done();
    G_SwapBuffers();

    /* Submit and run the queued batch to completion */
    render_thread_start_work();
    render_thread_wait_done();
    G_SwapBuffers();

    /* Kick off the empty batch such that we're in the same state that we started in */
    render_thread_start_work();
}

void Engine_EnableRendering(bool on)
{
    Engine_WaitRenderWorkDone();
    s_rstate.swap_buffers = on;
}

void Engine_WaitRenderWorkDone(void)
{
    PERF_ENTER();
    if(s_quit) {
        PERF_RETURN_VOID();
    }

    /* Wait for the render thread to finish, but don't yet clear/ack the 'done' flag */
    SDL_LockMutex(s_rstate.done_lock);
    while(!s_rstate.done) {
        SDL_CondWait(s_rstate.done_cond, s_rstate.done_lock);
    }
    SDL_UnlockMutex(s_rstate.done_lock);

    PERF_RETURN_VOID();
}

void Engine_ClearPendingEvents(void)
{
    SDL_FlushEvents(0, SDL_LASTEVENT);
    E_ClearPendingEvents();
}

bool Engine_GetArg(const char *name, size_t maxout, char out[static maxout])
{
    size_t namelen = strlen(name);
    for(int i = 2; i < s_argc; i++) {
        const char *curr = s_argv[i];
        if(strstr(curr, "--") != curr)
            continue;
        curr += 2;
        if(0 != strncmp(curr, name, namelen))
            continue;
        curr += namelen;
        if(*curr != '=')
            continue;
        pf_strlcpy(out, curr + 1, maxout);
        return true;
    }
    return false;
}

#if defined(_WIN32)
int CALLBACK pf_WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                     LPSTR lpCmdLine, int nCmdShow)
{
    int argc = __argc;
    char **argv = __argv;

#else
int pf_main(int argc, char **argv)
{
#endif

    int ret = EXIT_SUCCESS;

    if(argc < 3) {
        printf("Usage: %s [base directory path (containing 'assets', 'shaders' and 'scripts' folders)] [script path]\n", argv[0]);
        ret = EXIT_FAILURE;
        goto fail_args;
    }

    g_basepath = argv[1];
    s_argc = argc;
    s_argv = argv;

    if(!engine_init()) {
        ret = EXIT_FAILURE; 
        goto fail_init;
    }

    Audio_PlayMusicFirst();
    S_RunFile(argv[2], 0, NULL);

    /* Run the first frame of the simulation, and prepare the buffers for rendering. */
    E_ServiceQueue();
    G_Update();
    G_Render();
    G_SwapBuffers();
    Perf_FinishTick();

    while(!s_quit) {

        Perf_BeginTick();
        enum simstate curr_ss = G_GetSimState();
        bool prev_step_frame = s_step_frame;

        if(prev_step_frame) {
            assert(curr_ss != G_RUNNING); 
            G_SetSimState(G_RUNNING);
        }

        render_maybe_enable();
        render_thread_start_work();
        Sched_StartBackgroundTasks();

        process_sdl_events();
        E_ServiceQueue();
        Session_ServiceRequests();
        G_Update();
        G_Render();
        Sched_Tick();

	// Handle hot reloads
	// TODO: implement hot reload
	//S_HandleFileChanges();

        render_thread_wait_done();

        G_SwapBuffers();
        Perf_FinishTick();

        if(prev_step_frame) {
            G_SetSimState(curr_ss);
            s_step_frame = false;
        }

        ++g_frame_idx;
    }

    ss_e status;
    if((status = Settings_SaveToFile()) != SS_OKAY) {
        fprintf(stderr, "Could not save settings to file: %s [status: %d]\n", 
            Settings_GetFile(), status);
    }

    engine_shutdown();
fail_init:
fail_args:
    exit(ret);
}

