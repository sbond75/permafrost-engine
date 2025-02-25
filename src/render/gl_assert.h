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

#ifndef GL_ASSERT_H
#define GL_ASSERT_H

#include <assert.h>
#include <stdio.h>

#ifndef NDEBUG

static inline void GL_ASSERT_OK() {
    // https://stackoverflow.com/questions/7159348/disable-single-warning-error
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wformat"
    GLenum error = glGetError();
    if(error != GL_NO_ERROR) {
        fprintf(stderr, "%s:%d OpenGL error: 0x%x (%ju): %s\n",
        __FILE__, __LINE__, error, error, gluErrorString(error));
        fflush(stderr);
    }
    //assert(error == GL_NO_ERROR);
    #pragma GCC diagnostic pop
}

#else

#define GL_ASSERT_OK() /* no-op */

#endif

#endif
