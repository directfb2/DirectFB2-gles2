#  This file is part of DirectFB.
#
#  This library is free software; you can redistribute it and/or
#  modify it under the terms of the GNU Lesser General Public
#  License as published by the Free Software Foundation; either
#  version 2.1 of the License, or (at your option) any later version.
#
#  This library is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  Lesser General Public License for more details.
#
#  You should have received a copy of the GNU Lesser General Public
#  License along with this library; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA

project('DirectFB2-gles2', 'c',
        version: '2.0.0')

directfb_dep = dependency('directfb-internal')

moduledir = directfb_dep.get_pkgconfig_variable('moduledir')

gles2_dep = dependency('glesv2')

pkgconfig = import('pkgconfig')

gles2_sources = [
  'gles2_2d.c',
  'gles2_gfxdriver.c'
]

library('directfb_gles2',
        gles2_sources,
        dependencies: [directfb_dep, gles2_dep],
        install: true,
        install_dir: join_paths(moduledir, 'gfxdrivers'))

pkgconfig.generate(filebase: 'directfb-gfxdriver-gles2',
                   variables: 'moduledir=' + moduledir,
                   name: 'DirectFB-gfxdriver-gles2',
                   description: 'OpenGL ES 2.0 GFX driver',
                   requires_private: 'glesv2',
                   libraries_private: ['-L${moduledir}/gfxdrivers',
                                       '-Wl,--whole-archive -ldirectfb_gles2 -Wl,--no-whole-archive'])

if get_option('default_library') != 'shared'
  meson.add_install_script('dfb-update-pkgconfig', skip_if_destdir: true)
endif
