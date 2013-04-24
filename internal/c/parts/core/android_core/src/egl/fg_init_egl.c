/*
 * freeglut_init_android.c
 *
 * Various freeglut initialization functions.
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

#define FREEGLUT_BUILDING_LIB
#include <GL/freeglut.h>
#include "fg_internal.h"

#include <android/native_app_glue/android_native_app_glue.h>

/*
 * A call to this function should initialize all the display stuff...
 */
void fgPlatformInitialize( const char* displayName )
{
  fprintf(stderr, "fgPlatformInitialize\n");
  fgState.Initialised = GL_TRUE;

  /* CreateDisplay */
  /* Using EGL_DEFAULT_DISPLAY, or a specific native display */
  EGLNativeDisplayType nativeDisplay = EGL_DEFAULT_DISPLAY;
  fgDisplay.pDisplay.eglDisplay = eglGetDisplay(nativeDisplay);


  FREEGLUT_INTERNAL_ERROR_EXIT(fgDisplay.pDisplay.eglDisplay != EGL_NO_DISPLAY,
			       "No display available", "fgPlatformInitialize");



  if (!eglInitialize(fgDisplay.pDisplay.eglDisplay, NULL, NULL))
    fgError("eglInitialize: error %x\n", eglGetError());


  /* CreateContext */
  fghCreateContext();

  // fgDisplay.ScreenWidth = ...;
  // fgDisplay.ScreenHeight = ...;
  // fgDisplay.ScreenWidthMM = ...;
  // fgDisplay.ScreenHeightMM = ...;
}

void fgPlatformCloseDisplay ( void )
{
  eglMakeCurrent(fgDisplay.pDisplay.eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  if (fgDisplay.pDisplay.eglContext != EGL_NO_CONTEXT) {
    eglDestroyContext(fgDisplay.pDisplay.eglDisplay, fgDisplay.pDisplay.eglContext);
    fgDisplay.pDisplay.eglContext = EGL_NO_CONTEXT;
  }

  if (fgDisplay.pDisplay.eglDisplay != EGL_NO_DISPLAY) {
    eglTerminate(fgDisplay.pDisplay.eglDisplay);
    fgDisplay.pDisplay.eglDisplay = EGL_NO_DISPLAY;
  }
}

/**
 * Destroy a menu context
 */
void fgPlatformDestroyContext ( SFG_PlatformDisplay pDisplay, SFG_WindowContextType MContext )
{
  if (MContext != EGL_NO_CONTEXT)
    eglDestroyContext(pDisplay.eglDisplay, MContext);
}
