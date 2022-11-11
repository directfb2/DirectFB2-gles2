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

/* Transform input 2D positions "dfbPos" by scale and offset to get GLES clip coordinates. */
static const char *draw_vert_src = "                                     \
attribute vec2 dfbPos;                                                   \
uniform   vec3 dfbScale;                                                 \
                                                                         \
void main(void)                                                          \
{                                                                        \
     gl_Position.x = dfbScale.x * dfbPos.x - 1.0;                        \
     gl_Position.y = dfbScale.y * dfbPos.y + dfbScale.z;                 \
     gl_Position.z = 0.0;                                                \
     gl_Position.w = 1.0;                                                \
}";

/* Transform input 2D positions "dfbPos" by the render options matrix before transforming to GLES clip coordinates. */
static const char *draw_mat_vert_src = "                                 \
attribute vec2 dfbPos;                                                   \
uniform   mat3 dfbMVPMatrix;                                             \
uniform   mat3 dfbROMatrix;                                              \
                                                                         \
void main(void)                                                          \
{                                                                        \
     vec3 pos = dfbMVPMatrix * dfbROMatrix * vec3(dfbPos, 1.0);          \
     gl_Position = vec4(pos.x, pos.y, 0.0, pos.z);                       \
}";

/* Draw fragment in a constant color. */
static const char *draw_frag_src = "                                     \
precision mediump float;                                                 \
                                                                         \
uniform vec4 dfbColor;                                                   \
                                                                         \
void main(void)                                                          \
{                                                                        \
     gl_FragColor = dfbColor;                                            \
}";

/* This is the same as draw_vert_src with the addition of texture coords "dfbUV". */
static const char *blit_vert_src = "                                     \
attribute vec2 dfbPos;                                                   \
attribute vec2 dfbUV;                                                    \
uniform   vec3 dfbScale;                                                 \
uniform   vec2 dfbTexScale;                                              \
varying   vec2 varTexCoord;                                              \
                                                                         \
void main(void)                                                          \
{                                                                        \
     gl_Position.x = dfbScale.x * dfbPos.x - 1.0;                        \
     gl_Position.y = dfbScale.y * dfbPos.y + dfbScale.z;                 \
     gl_Position.z = 0.0;                                                \
     gl_Position.w = 1.0;                                                \
     varTexCoord.s = dfbTexScale.x * dfbUV.x;                            \
     varTexCoord.t = dfbTexScale.y * dfbUV.y;                            \
}";

/* This is the same as draw_mat_vert_src with the addition of texture coords "dfbUV". */
static const char *blit_mat_vert_src = "                                 \
attribute vec2 dfbPos;                                                   \
attribute vec2 dfbUV;                                                    \
uniform   mat3 dfbMVPMatrix;                                             \
uniform   mat3 dfbROMatrix;                                              \
uniform   vec2 dfbTexScale;                                              \
varying   vec2 varTexCoord;                                              \
                                                                         \
void main(void)                                                          \
{                                                                        \
     vec3 pos = dfbMVPMatrix * dfbROMatrix * vec3(dfbPos, 1.0);          \
     gl_Position = vec4(pos.x, pos.y, 0.0, pos.z);                       \
     varTexCoord.s = dfbTexScale.x * dfbUV.x;                            \
     varTexCoord.t = dfbTexScale.y * dfbUV.y;                            \
}";

/* Sample texture. */
static const char *blit_frag_src = "                                     \
precision mediump float;                                                 \
                                                                         \
uniform sampler2D dfbSampler;                                            \
varying vec2      varTexCoord;                                           \
                                                                         \
void main(void)                                                          \
{                                                                        \
     gl_FragColor = texture2D(dfbSampler, varTexCoord);                  \
}";

/* Sample texture and modulate by static color. */
static const char *blit_color_frag_src = "                               \
precision mediump float;                                                 \
                                                                         \
uniform sampler2D dfbSampler;                                            \
uniform vec4      dfbColor;                                              \
varying vec2      varTexCoord;                                           \
                                                                         \
void main(void)                                                          \
{                                                                        \
     gl_FragColor = texture2D(dfbSampler, varTexCoord) * dfbColor;       \
}";

/* Apply source color keying. */
static const char *blit_colorkey_frag_src = "                            \
precision mediump float;                                                 \
                                                                         \
uniform sampler2D dfbSampler;                                            \
uniform vec4      dfbColor;                                              \
uniform ivec3     dfbColorkey;                                           \
varying vec2      varTexCoord;                                           \
                                                                         \
void main(void)                                                          \
{                                                                        \
     vec4 c = texture2D(dfbSampler, varTexCoord);                        \
     int  r = int(c.r * 255.0 + 0.5);                                    \
     int  g = int(c.g * 255.0 + 0.5);                                    \
     int  b = int(c.b * 255.0 + 0.5);                                    \
     if (r == dfbColorkey.x && g == dfbColorkey.y && b == dfbColorkey.z) \
          discard;                                                       \
     gl_FragColor = c * dfbColor;                                        \
}";

/* Perform an alpha pre-multiply of source frag color with source frag alpha after sampling and modulation. */
static const char *blit_premultiply_frag_src = "                         \
precision mediump float;                                                 \
                                                                         \
uniform sampler2D dfbSampler;                                            \
uniform vec4      dfbColor;                                              \
varying vec2      varTexCoord;                                           \
                                                                         \
void main(void)                                                          \
{                                                                        \
     gl_FragColor  = texture2D(dfbSampler, varTexCoord);                 \
     gl_FragColor *= dfbColor;                                           \
     gl_FragColor.rgb *= gl_FragColor.a;                                 \
}";
