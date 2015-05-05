/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus-ui-console - trivial_resampler.c                          *
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

#include "trivial_resampler.h"
#include "resampler.h"

#include "m64p_types.h"

#include <stdint.h>

static size_t trivial_resample(void* resampler_data,
                               const void* src, size_t src_size, unsigned int input_frequency,
                               void* dst, size_t dst_size, unsigned int output_frequency)
{
    size_t i, j;

    if (output_frequency >= input_frequency)
    {
        int sldf = input_frequency;
        int const2 = 2*sldf;
        int dldf = output_frequency;
        int const1 = const2 - 2*dldf;
        int criteria = const2 - dldf;

        for (i = 0; i < dst_size/4; ++i)
        {
            ((uint32_t*)dst)[i] = ((uint32_t*)src)[j];

            if (criteria >= 0)
            {
                ++j;
                criteria += const1;
            }
            else
            {
                criteria += const2;
            }
        }
    }
    else
    {
        for (i = 0; i < dst_size/4; ++i)
        {
            j = i * input_frequency / output_frequency;
            ((uint32_t*)dst)[i] = ((uint32_t*)src)[j];
        }
    }

    return j * 4;
}


static m64p_error init(void* object)
{
    struct m64p_resampler* resampler = (struct m64p_resampler*)object;

    /* fill resampler structure */
    resampler->resampler_data = NULL;
    resampler->resample = trivial_resample;

    return M64ERR_SUCCESS;
}

static void release(void* object)
{
}

const struct object_factory trivial_resampler_factory = { "trivial", init, release };

