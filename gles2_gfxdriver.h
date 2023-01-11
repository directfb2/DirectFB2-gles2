/*
   This file is part of DirectFB.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
*/

#ifndef __GLES2_GFXDRIVER_H__
#define __GLES2_GFXDRIVER_H__

#include <GLES2/gl2.h>

/**********************************************************************************************************************/

typedef enum {
     GLES2VA_POSITIONS = 0,
     GLES2VA_TEXCOORDS = 1
} GLES2VertexAttribs;

typedef enum {
     NONE        = 0x00000000,

     DESTINATION = 0x00000001,
     CLIP        = 0x00000002,
     MATRIX      = 0x00000004,

     COLOR_DRAW  = 0x00000010,
     COLORKEY    = 0x00000020,

     SOURCE      = 0x00000100,
     COLOR_BLIT  = 0x00000200,

     BLENDING    = 0x00010000,

     ALL         = 0x00010337
} GLES2ValidationFlags;

typedef struct {
     GLuint                obj;          /* the program object */
     GLint                 dfbScale;     /* location of scale factors for clipped coordinates */
     GLint                 dfbRotMatrix; /* location of layer rotation matrix */
     GLint                 dfbROMatrix;  /* location of render options matrix */
     GLint                 dfbMVPMatrix; /* location of model-view-projection matrix */
     GLint                 dfbColor;     /* location of global RGBA color */
     GLint                 dfbColorkey;  /* location of colorkey RGB color */
     GLint                 dfbTexScale;  /* location of scale factors for normalized tex coordinates */
     char                 *name;         /* program object name for debugging */
     GLES2ValidationFlags  flags;        /* validation flags */
} GLES2ProgramInfo;

typedef enum {
     DRAW,
     DRAW_MAT,
     BLIT,
     BLIT_MAT,
     BLIT_COLOR,
     BLIT_COLOR_MAT,
     BLIT_COLORKEY,
     BLIT_COLORKEY_MAT,
     BLIT_PREMULTIPLY,
     BLIT_PREMULTIPLY_MAT,
     NUM_PROGRAMS,
     INVALID_PROGRAM
} GLES2ProgramIndex;

typedef struct {
     DFBSurfaceBlittingFlags blittingflags; /* blitting flags */
     float                   aspect;        /* layer aspect scaling */
     int                     rotation;      /* layer rotation */
} GLES2DriverData;

typedef struct {
     GLES2ProgramInfo  progs[NUM_PROGRAMS]; /* program info */
     GLES2ProgramIndex prog_index;          /* current program in use */
} GLES2DeviceData;

#endif
