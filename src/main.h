/*
 *  This file is part of Permafrost Engine. 
 *  Copyright (C) 2019-2020 Eduard Permyakov 
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

#ifndef MAIN_H
#define MAIN_H

#include <SDL.h>
#include <assert.h>
#include <stdbool.h>

#ifdef __APPLE__
#include <pthread.h>
static inline uint64_t thisThreadID() {
    uint64_t thread_id;
    // https://www.manpagez.com/man/3/pthread_threadid_np/ : for macOS
    pthread_threadid_np(pthread_self(), &thread_id);
    return thread_id;
}
// Based on https://github.com/libsdl-org/SDL : SDL/src/thread/SDL_thread_c.h
typedef pthread_t SYS_ThreadHandle; // SDL/src/thread/pthread/SDL_systhread_c.h
#define ERR_MAX_STRLEN  128 // SDL/src/SDL_error_c.h
typedef struct SDL_error // SDL/src/SDL_error_c.h
{
    int error; /* This is a numeric value corresponding to the current error */
    char str[ERR_MAX_STRLEN];
} SDL_error;
struct SDL_Thread_impl
{
    SDL_threadID threadid;
    SYS_ThreadHandle handle;
    int status;
    SDL_atomic_t state;  /* SDL_THREAD_STATE_* */
    SDL_error errbuf;
    char *name;
    size_t stacksize;  /* 0 for default, >0 for user-specified stack size. */
    int (SDLCALL * userfunc) (void *);
    void *userdata;
    void *data;
    void *endfunc;  /* only used on some platforms. */
};
static inline uint64_t getThreadID_impl(SDL_Thread* th) {
    pthread_t pth = ((struct SDL_Thread_impl*)th)->handle;
    
    uint64_t thread_id;
    // https://www.manpagez.com/man/3/pthread_threadid_np/ : for macOS
    pthread_threadid_np(pth, &thread_id);
    return thread_id;
}
#define getThreadID(x) getThreadID_impl(x)
//#define thisThreadID() (long long)pthread_self() //((struct _opaque_pthread_t *)pthread_self())->__sig //pthread_self()
#define threadID_t uint64_t //long long //long //pthread_t
#else
#define thisThreadID() SDL_ThreadID()
#define threadID_t SDL_threadID
#define getThreadID(x) SDL_GetThreadID(x)
#endif
#define xstr(a) str(a) // https://stackoverflow.com/questions/2653214/stringification-of-a-macro-value
#define str(a) #a

extern const char    *g_basepath;      /* readonly */
extern unsigned       g_last_frame_ms; /* readonly */
extern unsigned long  g_frame_idx;     /* readonly */
extern threadID_t     g_main_thread_id;   /* readonly */
extern threadID_t     g_render_thread_id; /* readonly */


#define ASSERT_IN_RENDER_THREAD() \
    assert(thisThreadID() == g_render_thread_id)

#define ASSERT_IN_MAIN_THREAD() \
    assert(thisThreadID() == g_main_thread_id)


enum pf_window_flags {

    PF_WF_FULLSCREEN     = SDL_WINDOW_FULLSCREEN 
                         | SDL_WINDOW_INPUT_GRABBED,
    PF_WF_BORDERLESS_WIN = SDL_WINDOW_BORDERLESS 
                         | SDL_WINDOW_INPUT_GRABBED,
    PF_WF_WINDOW         = SDL_WINDOW_INPUT_GRABBED,
};


int  Engine_SetRes(int w, int h);
void Engine_SetDispMode(enum pf_window_flags wf);
void Engine_WinDrawableSize(int *out_w, int *out_h);
void Engine_LoadingScreen(void);
void Engine_EnableRendering(bool on);

/* Execute all the currently queued render commands on the render thread. 
 * Block until it completes. This is used during initialization only to 
 * execute rendering code serially.
 */
void Engine_FlushRenderWorkQueue(void);
/* Wait for the current batch of render command to finish */
void Engine_WaitRenderWorkDone(void);
void Engine_ClearPendingEvents(void);
bool Engine_GetArg(const char *name, size_t maxout, char out[static maxout]);

#ifdef EXTERNAL_DRIVER // https://stackoverflow.com/questions/31519449/c99-call-main-function-from-another-main
#define pf_WinMain pf_WinMain
#define pf_main pf_main
#else
#define pf_WinMain WinMain
#define pf_main main
#endif

#if defined(_WIN32)
int CALLBACK pf_WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                     LPSTR lpCmdLine, int nCmdShow);
#else
int pf_main(int argc, char **argv);
#endif

#endif

