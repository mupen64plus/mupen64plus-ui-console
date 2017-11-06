/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus-ui-console - object_factory.c                             *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2015 Bobby Smiles                                       *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "object_factory.h"

#include "audio_backends/dummy_audio_backend.h"
#include "resamplers/trivial_resampler.h"

#ifdef HAVE_SDL_AUDIO_BACKEND
#include "audio_backends/sdl_audio_backend.h"
#endif

#include <string.h>


const struct object_factory* const audio_backend_factories[] =
{
    &dummy_audio_backend_factory,
#ifdef HAVE_SDL_AUDIO_BACKEND
    &sdl_audio_backend_factory,
#endif
    NULL /* end of array sentinel */
};

const struct object_factory* const resampler_factories[] =
{
    &trivial_resampler_factory,
    NULL /* end of array sentinel */
};


const struct object_factory* get_object_factory(const struct object_factory* const* factories, const char* name)
{
    if (factories != NULL && name != NULL)
    {
        while((*factories) != NULL)
        {
            if (strcmp(name, (*factories)->name) == 0)
            {
                return *factories;
            }

            ++factories;
        }
    }

    return NULL;
}

