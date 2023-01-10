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

#include <core/screen.h>
#include <core/screens.h>
#include <core/state.h>
#include <core/surface_allocation.h>

#include "gles2_gfxdriver.h"

D_DEBUG_DOMAIN( GLES2_2D, "GLES2/2D", "OpenGL ES 2.0 2D Acceleration" );

/**********************************************************************************************************************/

/*
 * State handling macros.
 */

#define GLES2_VALIDATE(flag)                                      \
     do {                                                         \
          dev->progs[dev->prog_index].flags |= flag;              \
     } while (0)

#define GLES2_INVALIDATE(flag)                                    \
     do {                                                         \
          int i;                                                  \
          for (i = 0; i < NUM_PROGRAMS; i++)                      \
               dev->progs[i].flags &= ~flag;                      \
     } while (0)

#define GLES2_CHECK_VALIDATE(flag)                                \
     do {                                                         \
          if ((dev->progs[dev->prog_index].flags & flag) != flag) \
               gles2_validate_##flag( drv, dev, state );          \
     } while (0)

/*
 * State validation functions.
 */

static inline void
gles2_validate_DESTINATION( GLES2DriverData *drv,
                            GLES2DeviceData *dev,
                            CardState       *state )
{
     GLint             w    = state->destination->config.size.w;
     GLint             h    = state->destination->config.size.h;
     GLES2ProgramInfo *prog = &dev->progs[dev->prog_index];
     GLfloat           m[9];
     int               width, height;

     D_DEBUG_AT( GLES2_2D, "%s()\n", __FUNCTION__ );
     D_DEBUG_AT( GLES2_2D, "  -> width %d, height %d\n", w, h );

     glViewport( 0, 0, w, h );

     memset( m, 0, sizeof(m) );
     m[8] = 1.0f;

     if (!drv->aspect) {
          /* Get the layer rotation. */
          if (dfb_config->layers[dfb_config->primary_layer].rotate_set)
               drv->rotation = dfb_config->layers[dfb_config->primary_layer].rotate;
          else
               dfb_screen_get_rotation( dfb_screen_at_translated( DSCID_PRIMARY ), &drv->rotation );

          if (drv->rotation == 90) {
               dfb_screen_get_screen_size( dfb_screen_at_translated( DSCID_PRIMARY ), &width, &height );
               drv->aspect = height <= width ? (float) height / width : (float) width / height;
          }
          else
               drv->aspect = 1.0f;
     }

     if (state->render_options & DSRO_MATRIX) {
          m[0] = 2.0f / w; m[4] = -2.0f / h; m[6] = -1.0f; m[7] = 1.0f;

          glUniformMatrix3fv( prog->dfbMVPMatrix, 1, GL_FALSE, m );
     }
     else {
          GLint fbo;

          glGetIntegerv( GL_FRAMEBUFFER_BINDING, &fbo );

          if (fbo) {
               glUniform3f( prog->dfbScale, 2.0f / w,  2.0f / h, -1.0f );

               m[0] = 1.0f; m[4] = 1.0f;

               glUniformMatrix3fv( prog->dfbRotMatrix, 1, GL_FALSE, m );
          }
          else {
               if (drv->rotation == 90)
                    glUniform3f( prog->dfbScale, drv->aspect * 2.0f / h, drv->aspect * -2.0f / w,  1.0f );
               else
                    glUniform3f( prog->dfbScale, 2.0f / w, -2.0f / h,  1.0f );

               switch (drv->rotation) {
                    case 180:
                         m[0] = -1.0f; m[4] = -1.0f;
                         break;
                    case 90:
                         m[1] = 1.0f; m[3] = -1.0f;
                         break;
                    default:
                         m[0] = 1.0f; m[4] = 1.0f;
                         break;
               }

               glUniformMatrix3fv( prog->dfbRotMatrix, 1, GL_FALSE, m );
          }
     }

     /* Set the flag. */
     GLES2_VALIDATE( DESTINATION );
}

static inline void
gles2_validate_CLIP( GLES2DriverData *drv,
                     GLES2DeviceData *dev,
                     CardState       *state )
{
     GLint fbo;

     D_DEBUG_AT( GLES2_2D, "%s()\n", __FUNCTION__ );

     glGetIntegerv( GL_FRAMEBUFFER_BINDING, &fbo );

     glEnable( GL_SCISSOR_TEST );

     glScissor( state->clip.x1, fbo ? state->clip.y1 : state->dst.allocation->config.size.h - state->clip.y2 - 1,
                state->clip.x2 - state->clip.x1 + 1, state->clip.y2 - state->clip.y1 + 1 );

     /* Set the flag. */
     GLES2_VALIDATE( CLIP );
}

static inline void
gles2_validate_MATRIX( GLES2DriverData *drv,
                       GLES2DeviceData *dev,
                       CardState       *state )
{
     GLES2ProgramInfo *prog = &dev->progs[dev->prog_index];

     D_DEBUG_AT( GLES2_2D, "%s()\n", __FUNCTION__ );

     if (state->render_options & DSRO_MATRIX) {
          /* matrix consists of 3x3 fixed point 16.16 integer */
          GLfloat m[9];

          m[0] = state->matrix[0] / 65536.0f; m[3] = state->matrix[1] / 65536.0f; m[6] = state->matrix[6] / 65536.0f;
          m[1] = state->matrix[3] / 65536.0f; m[4] = state->matrix[4] / 65536.0f; m[7] = state->matrix[7] / 65536.0f;
          m[2] = state->matrix[6] / 65536.0f; m[5] = state->matrix[7] / 65536.0f; m[8] = state->matrix[8] / 65536.0f;

          glUniformMatrix3fv( prog->dfbROMatrix, 1, GL_FALSE, m );

          D_DEBUG_AT( GLES2_2D, "  -> loaded render options %f %f %f %f %f %f %f %f %f\n",
                      m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8] );
     }

     /* Set the flag. */
     GLES2_VALIDATE( MATRIX );
}

static inline void
gles2_validate_COLOR_DRAW( GLES2DriverData *drv,
                           GLES2DeviceData *dev,
                           CardState       *state )
{
     GLES2ProgramInfo *prog = &dev->progs[dev->prog_index];

     D_DEBUG_AT( GLES2_2D, "%s()\n", __FUNCTION__ );

     if (state->drawingflags & DSDRAW_SRC_PREMULTIPLY) {
          /* Pre-multiply source color by alpha: c/255.0 * a/255.0 = c * a/65025f */
          GLfloat a = state->color.a / 65025.0f;

          glUniform4f( prog->dfbColor,
                       state->color.r * a, state->color.g * a, state->color.b * a, state->color.a / 255.0f );

          D_DEBUG_AT( GLES2_2D, "  -> loaded premultiplied color %f %f %f %f\n",
                      state->color.r * a, state->color.g * a, state->color.b * a, state->color.a / 255.0f );
     }
     else {
          /* Scale source color in floating point range [0.0f - 1.0f] */
          GLfloat s = 1.0f / 255.0f;

          glUniform4f( prog->dfbColor, state->color.r * s, state->color.g * s, state->color.b * s, state->color.a * s );

          D_DEBUG_AT( GLES2_2D, "  -> loaded color %f %f %f %f\n",
                      state->color.r * s, state->color.g * s, state->color.b * s, state->color.a * s );
     }

     /* Set the flag. */
     GLES2_VALIDATE( COLOR_DRAW );
}

static inline void
gles2_validate_COLORKEY( GLES2DriverData *drv,
                         GLES2DeviceData *dev,
                         CardState       *state )
{
     GLint             r    = (state->src_colorkey & 0x00FF0000) >> 16;
     GLint             g    = (state->src_colorkey & 0x0000FF00) >>  8;
     GLint             b    = (state->src_colorkey & 0x000000FF);
     GLES2ProgramInfo *prog = &dev->progs[dev->prog_index];

     D_DEBUG_AT( GLES2_2D, "%s()\n", __FUNCTION__ );

     glUniform3i( prog->dfbColorkey, r, g, b );

     D_DEBUG_AT( GLES2_2D, "  -> loaded colorkey %d %d %d\n", r, g, b );

     /* Set the flag. */
     GLES2_VALIDATE( COLORKEY );
}

static inline void
gles2_validate_SOURCE( GLES2DriverData *drv,
                       GLES2DeviceData *dev,
                       CardState       *state )
{
     GLint             w    = state->source->config.size.w;
     GLint             h    = state->source->config.size.h;
     GLuint            tex  = (GLuint)(long) state->src.handle;
     GLES2ProgramInfo *prog = &dev->progs[dev->prog_index];

     D_DEBUG_AT( GLES2_2D, "%s()\n", __FUNCTION__ );
     D_DEBUG_AT( GLES2_2D, "  -> width %d, height %d, texture %u\n", w, h, tex );

     glBindTexture( GL_TEXTURE_2D, tex );

     glUniform2f( prog->dfbTexScale, 1.0f / w, 1.0f / h );

     /* Set the flag. */
     GLES2_VALIDATE( SOURCE );
}

static inline void
gles2_validate_COLOR_BLIT( GLES2DriverData *drv,
                           GLES2DeviceData *dev,
                           CardState       *state )
{
     GLfloat           s, r, g, b, a;
     GLES2ProgramInfo *prog = &dev->progs[dev->prog_index];

     D_DEBUG_AT( GLES2_2D, "%s()\n", __FUNCTION__ );

     /* Scale in floating point range [0.0f - 1.0f] */
     s = 1.0f / 255.0f;

     if (state->blittingflags & DSBLIT_COLORIZE) {
          D_DEBUG_AT( GLES2_2D, "  -> DSBLIT_COLORIZE\n" );

          r = state->color.r * s;
          g = state->color.g * s;
          b = state->color.b * s;
     }
     else {
          r = g = b = 1.0f;
     }

     if (state->blittingflags & DSBLIT_BLEND_COLORALPHA) {
          D_DEBUG_AT( GLES2_2D, "  -> DSBLIT_BLEND_COLORALPHA\n" );

          a = state->color.a * s;
     }
     else {
          a = 1.0f;
     }

     if (state->blittingflags & DSBLIT_SRC_PREMULTCOLOR) {
          D_DEBUG_AT( GLES2_2D, "  -> DSBLIT_SRC_PREMULTCOLOR\n" );

          r *= a;
          g *= a;
          b *= a;
     }

     glUniform4f( prog->dfbColor, r, g, b, a );

     D_DEBUG_AT( GLES2_2D, "  -> loaded color %f %f %f %f\n", r, g, b, a );

     /* Set the flag. */
     GLES2_VALIDATE( COLOR_BLIT );
}

static inline void
gles2_validate_BLENDING( GLES2DriverData *drv,
                         GLES2DeviceData *dev,
                         CardState       *state )
{
     GLenum src, dst;

     D_DEBUG_AT( GLES2_2D, "%s()\n", __FUNCTION__ );

     switch (state->src_blend) {
          case DSBF_ZERO:
               src = GL_ZERO;
               break;

          case DSBF_ONE:
               src = GL_ONE;
               break;

          case DSBF_SRCCOLOR:
               src = GL_SRC_COLOR;
               break;

          case DSBF_INVSRCCOLOR:
               src = GL_ONE_MINUS_SRC_COLOR;
               break;

          case DSBF_SRCALPHA:
               src = GL_SRC_ALPHA;
               break;

          case DSBF_INVSRCALPHA:
               src = GL_ONE_MINUS_SRC_ALPHA;
               break;

          case DSBF_DESTALPHA:
               src = GL_DST_ALPHA;
               break;

          case DSBF_INVDESTALPHA:
               src = GL_ONE_MINUS_DST_ALPHA;
               break;

          case DSBF_DESTCOLOR:
               src = GL_DST_COLOR;
               break;

          case DSBF_INVDESTCOLOR:
               src = GL_ONE_MINUS_DST_COLOR;
               break;

          case DSBF_SRCALPHASAT:
               src = GL_SRC_ALPHA_SATURATE;
               break;

          default:
               D_BUG( "unexpected src blend function %u", state->src_blend );
     }

     switch (state->dst_blend) {
          case DSBF_ZERO:
               dst = GL_ZERO;
               break;

          case DSBF_ONE:
               dst = GL_ONE;
               break;

          case DSBF_SRCCOLOR:
               dst = GL_SRC_COLOR;
               break;

          case DSBF_INVSRCCOLOR:
               dst = GL_ONE_MINUS_SRC_COLOR;
               break;

          case DSBF_SRCALPHA:
               dst = GL_SRC_ALPHA;
               break;

          case DSBF_INVSRCALPHA:
               dst = GL_ONE_MINUS_SRC_ALPHA;
               break;

          case DSBF_DESTALPHA:
               dst = GL_DST_ALPHA;
               break;

          case DSBF_INVDESTALPHA:
               dst = GL_ONE_MINUS_DST_ALPHA;
               break;

          case DSBF_DESTCOLOR:
               dst = GL_DST_COLOR;
               break;

          case DSBF_INVDESTCOLOR:
               dst = GL_ONE_MINUS_DST_COLOR;
               break;

          case DSBF_SRCALPHASAT:
               dst = GL_SRC_ALPHA_SATURATE;
               break;

          default:
               D_BUG( "unexpected dst blend function %u", state->dst_blend );
     }

     glBlendFunc( src, dst );

     /* Set the flag. */
     GLES2_VALIDATE( BLENDING );
}

/**********************************************************************************************************************/

static void
gles2CheckState( void                *driver_data,
                 void                *device_data,
                 CardState           *state,
                 DFBAccelerationMask  accel )
{
     GraphicsDeviceInfo device_info;

     D_DEBUG_AT( GLES2_2D, "%s( %p, 0x%08x )\n", __FUNCTION__, state, accel );

     dfb_gfxcard_get_device_info( &device_info );

     /* Check if function is accelerated. */
     if (accel & ~device_info.caps.accel) {
          D_DEBUG_AT(GLES2_2D, "  -> unsupported function\n");
          return;
     }

     /* Check if drawing or blitting flags are supported. */
     if (DFB_DRAWING_FUNCTION(accel)) {
          if (state->drawingflags & ~device_info.caps.drawing) {
               D_DEBUG_AT(GLES2_2D, "  -> unsupported drawing flags 0x%08x\n", state->drawingflags);
               return;
          }
     }
     else {
          if (state->blittingflags & ~device_info.caps.blitting) {
               D_DEBUG_AT(GLES2_2D, "  -> unsupported blitting flags 0x%08x\n", state->blittingflags);
               return;
          }
     }

     /* Enable acceleration of the function. */
     state->accel |= accel;
}

static void
gles2SetState( void                *driver_data,
               void                *device_data,
               GraphicsDeviceFuncs *funcs,
               CardState           *state,
               DFBAccelerationMask  accel )
{
     GLES2DriverData *drv = driver_data;
     GLES2DeviceData *dev = device_data;
     DFBBoolean       blend;

     D_DEBUG_AT(GLES2_2D, "%s( %p, 0x%08x ) <- mod_hw 0x%08x\n", __FUNCTION__, state, accel, state->mod_hw );

     drv->blittingflags = state->blittingflags;

     /*
      * 1) Invalidate hardware states
      *
      * Each modification to the hw independent state invalidates one or more hardware states.
      */

     if (state->mod_hw == SMF_ALL) {
          GLES2_INVALIDATE( ALL );
     }
     else if (state->mod_hw) {
          if (state->mod_hw & SMF_DESTINATION)
               GLES2_INVALIDATE( DESTINATION );

          if (state->mod_hw & SMF_CLIP)
               GLES2_INVALIDATE( CLIP );

          if (state->mod_hw & SMF_MATRIX || state->mod_hw & SMF_RENDER_OPTIONS)
               GLES2_INVALIDATE( MATRIX );

          if (state->mod_hw & SMF_COLOR || state->mod_hw & SMF_DRAWING_FLAGS)
               GLES2_INVALIDATE( COLOR_DRAW );

          if (state->mod_hw & SMF_COLOR || state->mod_hw & SMF_BLITTING_FLAGS)
               GLES2_INVALIDATE( COLOR_BLIT );

          if (state->mod_hw & SMF_SRC_COLORKEY)
               GLES2_INVALIDATE( COLORKEY );

          if (state->mod_hw & SMF_SOURCE)
               GLES2_INVALIDATE( SOURCE );

          if (state->mod_hw & (SMF_SRC_BLEND | SMF_DST_BLEND))
               GLES2_INVALIDATE( BLENDING );
     }

     /*
      * 2) Validate hardware states
      *
      * Each function has its own set of states that need to be validated.
      */

     switch (accel) {
          case DFXL_FILLRECTANGLE:
          case DFXL_DRAWRECTANGLE:
          case DFXL_DRAWLINE:
          case DFXL_FILLTRIANGLE:
               /* Use of alpha blending. */
               if (state->drawingflags & DSDRAW_BLEND)
                    blend = DFB_TRUE;
               else
                    blend = DFB_FALSE;

               /* Validate the current shader program to use and check the states to validate. */
               if (state->render_options & DSRO_MATRIX) {
                    if (dev->prog_index != DRAW_MAT) {
                         dev->prog_index = DRAW_MAT;
                         glUseProgram( dev->progs[dev->prog_index].obj );
                    }
               }
               else {
                    if (dev->prog_index != DRAW) {
                         dev->prog_index = DRAW;
                         glUseProgram( dev->progs[dev->prog_index].obj );
                    }
               }

               D_DEBUG_AT( GLES2_2D, "  -> using shader program \"%s\"\n", dev->progs[dev->prog_index].name );

               GLES2_CHECK_VALIDATE( DESTINATION );
               GLES2_CHECK_VALIDATE( CLIP );
               GLES2_CHECK_VALIDATE( MATRIX );
               GLES2_CHECK_VALIDATE( COLOR_DRAW );

               if (blend) {
                    GLES2_CHECK_VALIDATE( BLENDING );
                    glEnable( GL_BLEND );
               }
               else {
                    glDisable( GL_BLEND );
               }

               /* Enable vertex positions and disable texture coordinates. */
               glEnableVertexAttribArray( GLES2VA_POSITIONS );
               glDisableVertexAttribArray( GLES2VA_TEXCOORDS );

               /*
                * 3) Tell which functions can be called without further validation, i.e. SetState()
                *
                * When the hw independent state is changed, this collection is reset.
                */

               state->set = DFXL_FILLRECTANGLE | DFXL_DRAWRECTANGLE | DFXL_DRAWLINE | DFXL_FILLTRIANGLE;
               break;

          case DFXL_BLIT:
          case DFXL_STRETCHBLIT:
               /* Use of alpha blending. */
               if (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA))
                    blend = DFB_TRUE;
               else
                    blend = DFB_FALSE;

               /* Validate the current shader program to use and check the states to validate. */
               if (state->render_options & DSRO_MATRIX) {
                    if (state->blittingflags & DSBLIT_SRC_COLORKEY && !blend) {
                         if (dev->prog_index != BLIT_COLORKEY_MAT) {
                              dev->prog_index = BLIT_COLORKEY_MAT;
                              glUseProgram( dev->progs[dev->prog_index].obj );
                         }
                    }
                    else if (state->blittingflags & DSBLIT_SRC_PREMULTIPLY) {
                         if (dev->prog_index != BLIT_PREMULTIPLY_MAT) {
                              dev->prog_index = BLIT_PREMULTIPLY_MAT;
                              glUseProgram( dev->progs[dev->prog_index].obj );
                         }
                    }
                    else if (state->blittingflags & (DSBLIT_COLORIZE | DSBLIT_BLEND_COLORALPHA | DSBLIT_SRC_PREMULTCOLOR)) {
                         if (dev->prog_index != BLIT_COLOR_MAT) {
                              dev->prog_index = BLIT_COLOR_MAT;
                              glUseProgram( dev->progs[dev->prog_index].obj );
                         }
                    }
                    else {
                         if (dev->prog_index != BLIT_MAT) {
                              dev->prog_index = BLIT_MAT;
                              glUseProgram( dev->progs[dev->prog_index].obj );
                         }
                    }
               }
               else {
                    if (state->blittingflags & DSBLIT_SRC_COLORKEY && !blend) {
                         if (dev->prog_index != BLIT_COLORKEY) {
                              dev->prog_index = BLIT_COLORKEY;
                              glUseProgram( dev->progs[dev->prog_index].obj );
                         }
                    }
                    else if (state->blittingflags & DSBLIT_SRC_PREMULTIPLY) {
                         if (dev->prog_index != BLIT_PREMULTIPLY) {
                              dev->prog_index = BLIT_PREMULTIPLY;
                              glUseProgram( dev->progs[dev->prog_index].obj );
                         }
                    }
                    else if (state->blittingflags & (DSBLIT_COLORIZE | DSBLIT_BLEND_COLORALPHA | DSBLIT_SRC_PREMULTCOLOR)) {
                         if (dev->prog_index != BLIT_COLOR) {
                              dev->prog_index = BLIT_COLOR;
                              glUseProgram( dev->progs[dev->prog_index].obj );
                         }
                    }
                    else {
                         if (dev->prog_index != BLIT) {
                              dev->prog_index = BLIT;
                              glUseProgram( dev->progs[dev->prog_index].obj );
                         }
                    }
               }

               D_DEBUG_AT( GLES2_2D, "  -> using shader program \"%s\"\n", dev->progs[dev->prog_index].name );

               GLES2_CHECK_VALIDATE( DESTINATION );
               GLES2_CHECK_VALIDATE( CLIP );
               GLES2_CHECK_VALIDATE( MATRIX );
               GLES2_CHECK_VALIDATE( SOURCE );
               GLES2_CHECK_VALIDATE( COLOR_BLIT );

               if (blend) {
                    GLES2_CHECK_VALIDATE( BLENDING );
                    glEnable( GL_BLEND );
               }
               else {
                    if (state->blittingflags & DSBLIT_SRC_COLORKEY) {
                         GLES2_CHECK_VALIDATE( COLORKEY );
                         glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
                         glEnable( GL_BLEND );
                    }
                    else {
                         glDisable( GL_BLEND );
                    }
               }

               /* If normal blitting or color keying is used, don't use filtering. */
               if (accel == DFXL_BLIT || ((state->blittingflags & DSBLIT_SRC_COLORKEY) && !blend)) {
                    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
                    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
               }
               else {
                    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
                    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
               }

               /* Enable vertex positions and disable texture coordinates. */
               glEnableVertexAttribArray( GLES2VA_POSITIONS );
               glEnableVertexAttribArray( GLES2VA_TEXCOORDS );

               /*
                * 3) Tell which functions can be called without further validation, i.e. SetState()
                *
                * When the hw independent state is changed, this collection is reset.
                */

               state->set = accel;
               break;

          default:
               D_BUG( "unexpected drawing/blitting function" );
               break;
     }

     /*
      * 4) Clear modification flags
      *
      * All flags have been evaluated in 1) and remembered for further validation.
      * If the hw independent state is not modified, this function won't get called
      * for subsequent rendering functions, unless they aren't defined by 3).
      */

     state->mod_hw = SMF_NONE;
}

static bool
gles2FillRectangle( void         *driver_data,
                    void         *device_data,
                    DFBRectangle *rect )
{
     float   x1    = rect->x;
     float   y1    = rect->y;
     float   x2    = rect->w + x1;
     float   y2    = rect->h + y1;
     GLfloat pos[] = {
          x1, y1,
          x2, y1,
          x2, y2,
          x1, y2
     };

     D_DEBUG_AT( GLES2_2D, "%s( %4d,%4d-%4dx%4d )\n", __FUNCTION__, DFB_RECTANGLE_VALS( rect ) );

     glVertexAttribPointer( GLES2VA_POSITIONS, 2, GL_FLOAT, GL_FALSE, 0, pos );

     glDrawArrays( GL_TRIANGLE_FAN, 0, 4 );

     return true;
}

static bool
gles2DrawRectangle( void         *driver_data,
                    void         *device_data,
                    DFBRectangle *rect )
{
     float   x1    = rect->x + 1;
     float   y1    = rect->y + 1;
     float   x2    = rect->x + rect->w;
     float   y2    = rect->y + rect->h;
     GLfloat pos[] = {
          x1, y1,
          x2, y1,
          x2, y2,
          x1, y2
     };

     D_DEBUG_AT( GLES2_2D, "%s( %4d,%4d-%4dx%4d )\n", __FUNCTION__, DFB_RECTANGLE_VALS( rect ) );

     glVertexAttribPointer( GLES2VA_POSITIONS, 2, GL_FLOAT, GL_FALSE, 0, pos );

     glDrawArrays( GL_LINE_LOOP, 0, 4 );

     return true;
}

static bool
gles2DrawLine( void      *driver_data,
               void      *device_data,
               DFBRegion *line )
{
     GLfloat pos[] = {
          line->x1, line->y1,
          line->x2, line->y2
     };

     D_DEBUG_AT( GLES2_2D, "%s( %4d,%4d-%4d,%4d )\n", __FUNCTION__, DFB_REGION_VALS( line ) );

     glVertexAttribPointer( GLES2VA_POSITIONS, 2, GL_FLOAT, GL_FALSE, 0, pos );

     glDrawArrays( GL_LINES, 0, 2 );

     return true;
}

static bool
gles2FillTriangle( void        *driver_data,
                   void        *device_data,
                   DFBTriangle *tri )
{
     GLfloat pos[] = {
          tri->x1, tri->y1,
          tri->x2, tri->y2,
          tri->x3, tri->y3
     };

     D_DEBUG_AT( GLES2_2D, "%s( %4d,%4d-%4d,%4d-%4d,%4d )\n", __FUNCTION__, DFB_TRIANGLE_VALS( tri ) );

     glVertexAttribPointer( GLES2VA_POSITIONS, 2, GL_FLOAT, GL_FALSE, 0, pos );

     glDrawArrays( GL_TRIANGLES, 0, 3 );

     return true;
}

static bool
gles2Blit( void         *driver_data,
           void         *device_data,
           DFBRectangle *rect,
           int           dx,
           int           dy )
{
     GLES2DriverData *drv   = driver_data;
     float            x1    = dx;
     float            y1    = dy;
     float            x2    = rect->w + x1;
     float            y2    = rect->h + y1;
     GLfloat          pos[] = {
          x1, y1,
          x2, y1,
          x2, y2,
          x1, y2
     };
     float            tx1   = rect->x;
     float            ty1   = rect->y;
     float            tx2   = rect->w + tx1;
     float            ty2   = rect->h + ty1;
     GLfloat          tex[8];

     D_DEBUG_AT( GLES2_2D, "%s( [%2d], %4d,%4d-%4dx%4d <- %4d,%4d )\n", __FUNCTION__, 0,
                 dx, dy, rect->w, rect->h, rect->x, rect->y );

     if (drv->blittingflags & DSBLIT_ROTATE180) {
          tex[0] = tx2; tex[1] = ty2;
          tex[2] = tx1; tex[3] = ty2;
          tex[4] = tx1; tex[5] = ty1;
          tex[6] = tx2; tex[7] = ty1;
     }
     else if (drv->blittingflags & DSBLIT_ROTATE90) {
          tex[0] = tx2; tex[1] = ty1;
          tex[2] = tx2; tex[3] = ty2;
          tex[4] = tx1; tex[5] = ty2;
          tex[6] = tx1; tex[7] = ty1;
     }
     else if (drv->blittingflags & DSBLIT_ROTATE270) {
          tex[0] = tx1; tex[1] = ty2;
          tex[2] = tx1; tex[3] = ty1;
          tex[4] = tx2; tex[5] = ty1;
          tex[6] = tx2; tex[7] = ty2;
     }
     else {
          tex[0] = tx1; tex[1] = ty1;
          tex[2] = tx2; tex[3] = ty1;
          tex[4] = tx2; tex[5] = ty2;
          tex[6] = tx1; tex[7] = ty2;
     }

     glVertexAttribPointer( GLES2VA_POSITIONS, 2, GL_FLOAT, GL_FALSE, 0, pos );
     glVertexAttribPointer( GLES2VA_TEXCOORDS, 2, GL_FLOAT, GL_FALSE, 0, tex );

     glDrawArrays( GL_TRIANGLE_FAN, 0, 4 );

     return true;
}

static bool
gles2StretchBlit( void         *driver_data,
                  void         *device_data,
                  DFBRectangle *srect,
                  DFBRectangle *drect )
{
     GLES2DriverData *drv   = driver_data;
     float            x1    = drect->x;
     float            y1    = drect->y;
     float            x2    = drect->w + x1;
     float            y2    = drect->h + y1;
     GLfloat          pos[] = {
          x1, y1,
          x2, y1,
          x2, y2,
          x1, y2
     };
     float            tx1   = srect->x;
     float            ty1   = srect->y;
     float            tx2   = srect->w + tx1;
     float            ty2   = srect->h + ty1;
     GLfloat          tex[8];

     D_DEBUG_AT( GLES2_2D, "%s( [%2d], %4d,%4d-%4dx%4d <- %4d,%4d-%4dx%4d )\n", __FUNCTION__, 0,
                 DFB_RECTANGLE_VALS( drect ), DFB_RECTANGLE_VALS( srect ) );

     if (drv->blittingflags & DSBLIT_ROTATE180) {
          tex[0] = tx2; tex[1] = ty2;
          tex[2] = tx1; tex[3] = ty2;
          tex[4] = tx1; tex[5] = ty1;
          tex[6] = tx2; tex[7] = ty1;
     }
     else if (drv->blittingflags & DSBLIT_ROTATE90) {
          tex[0] = tx2; tex[1] = ty1;
          tex[2] = tx2; tex[3] = ty2;
          tex[4] = tx1; tex[5] = ty2;
          tex[6] = tx1; tex[7] = ty1;
     }
     else if (drv->blittingflags & DSBLIT_ROTATE270) {
          tex[0] = tx1; tex[1] = ty2;
          tex[2] = tx1; tex[3] = ty1;
          tex[4] = tx2; tex[5] = ty1;
          tex[6] = tx2; tex[7] = ty2;
     }
     else {
          tex[0] = tx1; tex[1] = ty1;
          tex[2] = tx2; tex[3] = ty1;
          tex[4] = tx2; tex[5] = ty2;
          tex[6] = tx1; tex[7] = ty2;
     }

     glVertexAttribPointer( GLES2VA_POSITIONS, 2, GL_FLOAT, GL_FALSE, 0, pos );
     glVertexAttribPointer( GLES2VA_TEXCOORDS, 2, GL_FLOAT, GL_FALSE, 0, tex );

     glDrawArrays( GL_TRIANGLE_FAN, 0, 4 );

     return true;
}

static bool
gles2BatchBlit( void               *driver_data,
                void               *device_data,
                const DFBRectangle *rects,
                const DFBPoint     *points,
                unsigned int        num,
                unsigned int       *ret_num )
{
     GLES2DriverData *drv = driver_data;
     float            x1, y1, x2, y2;
     GLfloat          pos[num*12];
     float            tx1, ty1, tx2, ty2;
     GLfloat          tex[num*12];
     int              i;

     for (i = 0; i < num; i++) {
          D_DEBUG_AT( GLES2_2D, "%s( [%2d] %4d,%4d-%4dx%4d <- %4d,%4d )\n", __FUNCTION__, i,
                      points[i].x, points[i].y, rects[i].w, rects[i].h, rects[i].x, rects[i].y );

          x1 = points[i].x;
          y1 = points[i].y;
          x2 = rects[i].w + x1;
          y2 = rects[i].h + y1;

          pos[i*12+0]  = x1; pos[i*12+1]  = y1;
          pos[i*12+2]  = x2; pos[i*12+3]  = y1;
          pos[i*12+4]  = x2; pos[i*12+5]  = y2;

          pos[i*12+6]  = x2; pos[i*12+7]  = y2;
          pos[i*12+8]  = x1; pos[i*12+9]  = y1;
          pos[i*12+10] = x1; pos[i*12+11] = y2;

          tx1 = rects[i].x;
          ty1 = rects[i].y;
          tx2 = rects[i].w + tx1;
          ty2 = rects[i].h + ty1;

          if (drv->blittingflags & DSBLIT_ROTATE180) {
               tex[i*12+0]  = tx2; tex[i*12+1]  = ty2;
               tex[i*12+2]  = tx1; tex[i*12+3]  = ty2;
               tex[i*12+4]  = tx1; tex[i*12+5]  = ty1;

               tex[i*12+6]  = tx1; tex[i*12+7]  = ty1;
               tex[i*12+8]  = tx2; tex[i*12+9]  = ty2;
               tex[i*12+10] = tx2; tex[i*12+11] = ty1;
          }
          else if (drv->blittingflags & DSBLIT_ROTATE90) {
               tex[i*12+0]  = tx2; tex[i*12+1]  = ty1;
               tex[i*12+2]  = tx2; tex[i*12+3]  = ty2;
               tex[i*12+4]  = tx1; tex[i*12+5]  = ty2;

               tex[i*12+6]  = tx1; tex[i*12+7]  = ty2;
               tex[i*12+8]  = tx2; tex[i*12+9]  = ty1;
               tex[i*12+10] = tx1; tex[i*12+11] = ty1;
          }
          else if (drv->blittingflags & DSBLIT_ROTATE270) {
               tex[i*12+0]  = tx1; tex[i*12+1]  = ty2;
               tex[i*12+2]  = tx1; tex[i*12+3]  = ty1;
               tex[i*12+4]  = tx2; tex[i*12+5]  = ty1;

               tex[i*12+6]  = tx2; tex[i*12+7]  = ty1;
               tex[i*12+8]  = tx1; tex[i*12+9]  = ty2;
               tex[i*12+10] = tx2; tex[i*12+11] = ty2;
          }
          else {
               tex[i*12+0]  = tx1; tex[i*12+1]  = ty1;
               tex[i*12+2]  = tx2; tex[i*12+3]  = ty1;
               tex[i*12+4]  = tx2; tex[i*12+5]  = ty2;

               tex[i*12+6]  = tx2; tex[i*12+7]  = ty2;
               tex[i*12+8]  = tx1; tex[i*12+9]  = ty1;
               tex[i*12+10] = tx1; tex[i*12+11] = ty2;
          }
     }

     glVertexAttribPointer( GLES2VA_POSITIONS, 2, GL_FLOAT, GL_FALSE, 0, pos );
     glVertexAttribPointer( GLES2VA_TEXCOORDS, 2, GL_FLOAT, GL_FALSE, 0, tex );

     glDrawArrays( GL_TRIANGLES, 0, num * 6 );

     return true;
}

const GraphicsDeviceFuncs gles2GraphicsDeviceFuncs = {
     .CheckState    = gles2CheckState,
     .SetState      = gles2SetState,
     .FillRectangle = gles2FillRectangle,
     .DrawRectangle = gles2DrawRectangle,
     .DrawLine      = gles2DrawLine,
     .FillTriangle  = gles2FillTriangle,
     .Blit          = gles2Blit,
     .StretchBlit   = gles2StretchBlit,
     .BatchBlit     = gles2BatchBlit
};
