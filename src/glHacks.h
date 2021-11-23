//
//  glHacks.h
//  permafrost-engine
//
//  Created by sbond75 on 11/22/21.
//  Copyright © 2021 sbond75. All rights reserved.
//

#ifndef glHacks_h
#define glHacks_h

#ifdef __APPLE__
typedef void* SDLTranslatorResponder;
typedef long NSInteger; // https://developer.apple.com/documentation/objectivec/nsinteger?language=objc

//typedef bool BOOL; // https://developer.apple.com/documentation/objectivec/bool?language=objc
#import <objc/runtime.h>
#import <objc/message.h>

typedef uint32_t IOPMAssertionID; // https://developer.apple.com/documentation/iokit/iopmassertionid
typedef struct SDL_VideoData_impl // SDL/src/video/cocoa/SDL_cocoavideo.h
{
    int allow_spaces;
    unsigned int modifierFlags;
    void *key_layout;
    SDLTranslatorResponder *fieldEdit;
    NSInteger clipboard_count;
    Uint32 screensaver_activity;
    BOOL screensaver_use_iopm;
    IOPMAssertionID screensaver_assertion;
    SDL_mutex *swaplock;
} SDL_VideoData_impl;

typedef void* SDL_WindowShaper;
typedef void* SDL_WindowUserData;
// SDL/src/video/SDL_sysvideo.h from SDL version 2.0.14 (tag release-2.0.14 in https://github.com/libsdl-org/SDL )
/* Define the SDL window structure, corresponding to toplevel windows */
struct SDL_Window_impl
{
    const void *magic;
    Uint32 id;
    char *title;
    SDL_Surface *icon;
    int x, y;
    int w, h;
    int min_w, min_h;
    int max_w, max_h;
    Uint32 flags;
    Uint32 last_fullscreen_flags;
    //Uint32 display_index;

    /* Stored position and size for windowed mode */
    SDL_Rect windowed;

    SDL_DisplayMode fullscreen_mode;

    float opacity;

    float brightness;
    Uint16 *gamma;
    Uint16 *saved_gamma;        /* (just offset into gamma) */

    SDL_Surface *surface;
    SDL_bool surface_valid;

    SDL_bool is_hiding;
    SDL_bool is_destroying;
    SDL_bool is_dropping;       /* drag/drop in progress, expecting SDL_SendDropComplete(). */

    //SDL_Rect mouse_rect;

    SDL_WindowShaper *shaper;

    SDL_HitTest hit_test;
    void *hit_test_data;

    SDL_WindowUserData *data;

    void *driverdata;

    SDL_Window *prev;
    SDL_Window *next;
};

typedef void* NSWindow;
typedef void* NSView;
typedef void* NSMutableArray;
typedef void* Cocoa_WindowListener;
typedef void *EGLSurface; // https://www.khronos.org/registry/EGL/api/EGL/egl.h
struct SDL_WindowData_impl // SDL/src/video/cocoa/SDL_cocoawindow.h from SDL version 2.0.14 (tag release-2.0.14 in https://github.com/libsdl-org/SDL )
{
    SDL_Window *window;
    NSWindow *nswindow;
    NSView *sdlContentView;
    NSMutableArray *nscontexts;
    SDL_bool created;
    SDL_bool inWindowFullscreenTransition;
    //NSInteger flash_request;
    Cocoa_WindowListener *listener;
    struct SDL_VideoData_impl *videodata;
#if SDL_VIDEO_OPENGL_EGL
    EGLSurface egl_surface;
#endif
};
//
//
//// https://gist.github.com/mmassaki/3892543
//void Swizzle(Class c, SEL orig, SEL new)
//{
//    Method origMethod = class_getInstanceMethod(c, orig);
//    Method newMethod = class_getInstanceMethod(c, new);
//    if(class_addMethod(c, orig, method_getImplementation(newMethod), method_getTypeEncoding(newMethod))) // Returns: "YES if the method was added successfully, otherwise NO (for example, the class already contains a method implementation with that name)." ( https://developer.apple.com/documentation/objectivec/1418901-class_addmethod?language=objc )
//        class_replaceMethod(c, new, method_getImplementation(origMethod), method_getTypeEncoding(origMethod));
//    else
//        method_exchangeImplementations(origMethod, newMethod);
//}
//
//// More hacks //
//
//void Replace(Class c, SEL orig, SEL new)
//{
//    Method origMethod = class_getInstanceMethod(c, orig);
//    Method newMethod = class_getInstanceMethod(c, new);
//    if(class_addMethod(c, orig, method_getImplementation(newMethod), method_getTypeEncoding(newMethod))) // Returns: "YES if the method was added successfully, otherwise NO (for example, the class already contains a method implementation with that name)." ( https://developer.apple.com/documentation/objectivec/1418901-class_addmethod?language=objc )
//        class_replaceMethod(c, new, method_getImplementation(origMethod), method_getTypeEncoding(origMethod));
//    else
//        method_setImplementation(origMethod, method_getImplementation(newMethod));
//}
//void Replace_withImp(Class c, SEL orig, IMP new, const char* newImpTypeEncoding)
//{
//    Method origMethod = class_getInstanceMethod(c, orig);
//    if(origMethod == NULL)
//        class_addMethod(c, orig, new, newImpTypeEncoding);
//    else
//        method_setImplementation(origMethod, new);
//}
//
#include <CoreFoundation/CoreFoundation.h> // "CFString is “toll-free bridged” with its Cocoa Foundation counterpart, NSString. This means that the Core Foundation type is interchangeable in function or method calls with the bridged Foundation object. Therefore, in a method where you see an NSString * parameter, you can pass in a CFStringRef, and in a function where you see a CFStringRef parameter, you can pass in an NSString instance. This also applies to concrete subclasses of NSString. See Toll-Free Bridged Types for more information on toll-free bridging." ( https://developer.apple.com/documentation/corefoundation/cfstring?language=objc )

// https://developer.apple.com/documentation/foundation/1395135-nsclassfromstring?language=objc
Class NSClassFromString(CFStringRef /*NSString **/aClassName);

// https://developer.apple.com/documentation/foundation/1395294-nsselectorfromstring?language=objc
SEL NSSelectorFromString(CFStringRef /*NSString **/aSelectorName);

//// //
//
//// https://www.mikeash.com/pyblog/objc_msgsends-new-prototype.html
//void newExplicitUpdate(id self, SEL _cmd) {
//    // Don't run on a dispatch queue! Original code from SDL/src/video/cocoa/SDL_cocoaopengl.m :
//    /*
//     - (void)explicitUpdate
//     {
//         if ([NSThread isMainThread]) {
//             [super update];
//         } else {
//             dispatch_sync(dispatch_get_main_queue(), ^{ [super update]; });
//         }
//     }
//     */
//
//    CFStringRef aCFString;
//    aCFString = CFStringCreateWithCString(NULL, "SDLOpenGLContext", kCFStringEncodingUTF8);
//    CFStringRef aCFString2;
//    aCFString2 = CFStringCreateWithCString(NULL, "update", kCFStringEncodingUTF8);
//    Class cls = NSClassFromString(aCFString);
//
//    // https://www.programmerall.com/article/7493171948/
//    Class superCls = class_getSuperclass(cls);
//    // See /usr/include/objc/message.h :
//    struct objc_super obj_super_class = {
//        .receiver = self,
//        #if !defined(__cplusplus)  &&  !__OBJC2__
//            .class = superCls
//        #else
//            .super_class = superCls
//        #endif
//
//    };
//
//    ((void (*)(void*, SEL))objc_msgSendSuper)(&obj_super_class, NSSelectorFromString(aCFString2));
//
//    CFRelease(aCFString);
//    CFRelease(aCFString2);
//}

typedef void* SDLOpenGLContext;
#endif

#endif /* glHacks_h */
