/*
 * freeglut_internal_android.h
 *
 * The freeglut library private include file.
 *
 * Copyright (C) 2012  Sylvain Beucler
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * PAWEL W. OLSZTA BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef  FREEGLUT_INTERNAL_EGL_H
#define  FREEGLUT_INTERNAL_EGL_H

#include <EGL/egl.h>

/* -- GLOBAL TYPE DEFINITIONS ---------------------------------------------- */
/* The structure used by display initialization in freeglut_init.c */
typedef struct tagSFG_PlatformDisplay SFG_PlatformDisplay;
struct tagSFG_Window;
struct tagSFG_PlatformDisplay
{
  /* Used to initialize and deinitialize EGL */
  EGLDisplay          eglDisplay;
  EGLContext          eglContext;
  EGLConfig           eglContextConfig;
  EGLint              eglContextFormat;
  struct tagSFG_Window* single_window;
};


/*
 * Make "freeglut" window handle and context types so that we don't need so
 * much conditionally-compiled code later in the library.
 */
typedef EGLNativeWindowType SFG_WindowHandleType ;
typedef EGLContext SFG_WindowContextType ;
typedef struct tagSFG_PlatformContext SFG_PlatformContext;
/* SFG_PlatformContext is used for SFG_Window.Window */
struct tagSFG_PlatformContext
{
  EGLSurface          eglSurface;
};


/* Window's state description. This structure should be kept portable. */
typedef struct tagSFG_PlatformWindowState SFG_PlatformWindowState;
struct tagSFG_PlatformWindowState
{
    int             OldWidth;           /* Window width from before a resize */
    int             OldHeight;          /*   "    height  "    "    "   "    */
};

#endif
