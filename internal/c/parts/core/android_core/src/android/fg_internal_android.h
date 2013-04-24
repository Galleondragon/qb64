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

#ifndef  FREEGLUT_INTERNAL_ANDROID_H
#define  FREEGLUT_INTERNAL_ANDROID_H


/* -- PLATFORM-SPECIFIC INCLUDES ------------------------------------------- */
/* Android OpenGL ES is accessed through EGL */
#include "egl/fg_internal_egl.h"

/**
 * Virtual PAD (spots on touchscreen that simulate keys)
 */
struct vpad_state {
    bool on;
    bool left;
    bool right;
    bool up;
    bool down;
};
struct touchscreen {
    struct vpad_state vpad;
    bool in_mmotion;
};


/* -- JOYSTICK-SPECIFIC STRUCTURES AND TYPES ------------------------------- */
/*
 * Initial defines from "js.h" starting around line 33 with the existing "freeglut_joystick.c"
 * interspersed
 */

  /*
   * We'll put these values in and that should
   * allow the code to at least compile when there is
   * no support. The JS open routine should error out
   * and shut off all the code downstream anyway and if
   * the application doesn't use a joystick we'll be fine.
   */

  struct JS_DATA_TYPE
  {
    int buttons;
    int x;
    int y;
  };

#            define JS_RETURN (sizeof(struct JS_DATA_TYPE))

/* XXX It might be better to poll the operating system for the numbers of buttons and
 * XXX axes and then dynamically allocate the arrays.
 */
#    define _JS_MAX_AXES 16
typedef struct tagSFG_PlatformJoystick SFG_PlatformJoystick;
struct tagSFG_PlatformJoystick
{
       struct JS_DATA_TYPE js;

    char         fname [ 128 ];
    int          fd;
};

#endif  /* FREEGLUT_INTERNAL_ANDROID_H */
