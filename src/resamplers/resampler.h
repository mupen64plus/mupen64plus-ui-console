/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus-ui-console - resampler.h                                  *
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

#ifndef M64P_RESAMPLER_H
#define M64P_RESAMPLER_H

#include <stddef.h>

struct m64p_resampler
{
    void* resampler_data;
    size_t (*resample)(void* resampler_data,
                       const void* src, size_t src_size, unsigned int input_frequency,
                       void* dst, size_t dst_size, unsigned int output_frequency);
};

size_t resample(struct m64p_resampler* resampler,
                const void* src, size_t src_size, unsigned int input_frequency,
                void* dst, size_t dst_size, unsigned int output_frequency);

#endif
