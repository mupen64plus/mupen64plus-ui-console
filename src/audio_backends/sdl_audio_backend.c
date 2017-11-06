/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus-ui-console - sdl_audio_backend.c                          *
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

#include "sdl_audio_backend.h"

#include "circular_buffer.h"
#include "m64p_types.h"
#include "main.h"
#include "object_factory.h"
#include "resamplers/resampler.h"

#include <SDL.h>

#include <stdlib.h>
#include <string.h>


struct sdl_data
{
    struct circular_buffer primary_buffer;
    size_t target;
    size_t secondary_buffer_size;

    unsigned int last_cb_time;
    unsigned int input_frequency;
    unsigned int output_frequency;
    unsigned int speed_factor;

    struct m64p_resampler resampler;
    void (*release_resampler)(void* object);

    unsigned int error;
    unsigned int swap_channels;
};


static struct sdl_data* new_sdl_data(size_t primary_buffer_size,
                                     size_t target,
                                     size_t secondary_buffer_size,
                                     const struct object_factory* resampler_factory,
                                     unsigned int swap_channels)
{
    /* allocate sdl_data */
    struct sdl_data* sdl_data = malloc(sizeof(*sdl_data));
    if (sdl_data == NULL)
        return NULL;

    /* init primary bugffer */
    if (init_cbuff(&sdl_data->primary_buffer, primary_buffer_size) != 0)
        goto free_sdl_data;

    /* init resampler */
    if (resampler_factory->init(&sdl_data->resampler) != M64ERR_SUCCESS)
        goto release_primary_buffer;

    /* init other fields */
    sdl_data->target = target;
    sdl_data->secondary_buffer_size = secondary_buffer_size;
    sdl_data->last_cb_time = 0;
    sdl_data->input_frequency = 0;
    sdl_data->output_frequency = 0;
    sdl_data->speed_factor = 100;
    sdl_data->release_resampler = resampler_factory->release;
    sdl_data->error = 0;
    sdl_data->swap_channels = swap_channels;

    return sdl_data;

release_primary_buffer:
    release_cbuff(&sdl_data->primary_buffer);
free_sdl_data:
    free(sdl_data);
    return NULL;
}

static void delete_sdl_data(struct sdl_data* sdl_data)
{
    sdl_data->release_resampler(&sdl_data->resampler);
    release_cbuff(&sdl_data->primary_buffer);
    free(sdl_data);
}

/* SDL audio callback.
 * Pop and resample a certain amount of samples from the primary buffer
 */
static void audio_cb(void* userdata, unsigned char* stream, int len)
{
    unsigned int old_sample_rate;
    unsigned int new_sample_rate;
    size_t available;
    size_t needed;
    size_t consumed;
    const void* src;
    struct sdl_data* sdl_data = (struct sdl_data*)userdata;

    /* update last_cb_time */
    sdl_data->last_cb_time = SDL_GetTicks();

    /* compute samples rates, and needed number of bytes */
    old_sample_rate = sdl_data->input_frequency;
    new_sample_rate = sdl_data->output_frequency * 100 / sdl_data->speed_factor;
    needed = len * old_sample_rate / new_sample_rate;

    src = cbuff_tail(&sdl_data->primary_buffer, &available);
    if ((available > 0) && (available >= needed))
    {
        /* consume samples */
        consumed = resample(&sdl_data->resampler, src, available, old_sample_rate, stream, len, new_sample_rate);
        consume_cbuff_data(&sdl_data->primary_buffer, consumed);
    }
    else
    {
        /* buffer underflow */
        DebugMessage(M64MSG_VERBOSE, "buffer underflow");
        memset(stream, 0, len);
    }
}


/* Select output frequency depending on input_frequency */
static unsigned int select_output_frequency(unsigned int input_frequency)
{
    if (input_frequency <= 11025) { return 11025; }
    else if (input_frequency <= 22050) { return 22050; }
    else { return 44100; }
}


/* Initialize the audio device. */
static void init_audio_device(struct sdl_data* sdl_data)
{
    SDL_AudioSpec desired, obtained;

    if (SDL_WasInit(SDL_INIT_AUDIO | SDL_INIT_TIMER) == (SDL_INIT_AUDIO | SDL_INIT_TIMER))
    {
        SDL_PauseAudio(1);
        SDL_CloseAudio();
    }
    else
    {
        /* initilize sdl subsystems */
        if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0)
        {
            DebugMessage(M64MSG_ERROR, "Failed to initialize SDL audio subsystem");
            sdl_data->error = 1;
            return;
        }
    }

    memset(&desired, 0, sizeof(desired));
    desired.freq = select_output_frequency(sdl_data->input_frequency);
    desired.format = AUDIO_S16SYS;
    desired.channels = 2;
    desired.samples = sdl_data->secondary_buffer_size / (2 * 2);
    desired.callback = audio_cb;
    desired.userdata = sdl_data;

    if (SDL_OpenAudio(&desired, &obtained) < 0)
    {
        DebugMessage(M64MSG_ERROR, "Failed to open the audio device: %s", SDL_GetError());
        sdl_data->error = 1;
        return;
    }

    sdl_data->output_frequency = obtained.freq;

    if (sdl_data->last_cb_time == 0)
        sdl_data->last_cb_time = SDL_GetTicks();

    sdl_data->error = 0;

    DebugMessage(M64MSG_INFO, "Audio device initialized: freq=%dHz (input freq=%dHz)",
                 sdl_data->output_frequency, sdl_data->input_frequency);
}


static void memcpy_swap_LR16(void* dst, const void* src, size_t size)
{
    /* assume a size multiple of 4 */
    size_t i;
    uint32_t v;
    const uint32_t* src_ = src;
    uint32_t* dst_ = dst;

    for(i = 0; i < size; i += 4)
    {
        v = *src_;
        *dst_ = (v << 16) | (v >> 16);

        ++src_;
        ++dst_;
    }
}

static void push_incoming_samples(struct sdl_data* sdl_data, const void* buffer, size_t size)
{
    size_t available;
    void* dst = cbuff_head(&sdl_data->primary_buffer, &available);

    if (size > available)
    {
        DebugMessage(M64MSG_VERBOSE, "Audio buffer overflow. Requested %d Available %d", size, available);
    }
    else
    {
        SDL_LockAudio();

        if (sdl_data->swap_channels == 0)
        {
            memcpy(dst, buffer, size);
        }
        else
        {
            memcpy_swap_LR16(dst, buffer, size);
        }

        SDL_UnlockAudio();

        produce_cbuff_data(&sdl_data->primary_buffer, size);
    }
}

static size_t estimate_consumable_size_at_next_audio_cb(const struct sdl_data* sdl_data)
{
    size_t available;
    size_t current_size, expected_size;
    unsigned int current_time, expected_cb_time; /* in ms */


    cbuff_tail(&sdl_data->primary_buffer, &available);


    current_size = (available * sdl_data->output_frequency * 100)
                 / (sdl_data->input_frequency * sdl_data->speed_factor);

    current_time = SDL_GetTicks();

    /* estimate time to next audio_cb */
    expected_cb_time = sdl_data->last_cb_time
        + (1000 * sdl_data->secondary_buffer_size / 4) / sdl_data->output_frequency;

    /* estimate consumable size at next audio_cb */
    expected_size = current_size;
    if (current_time < expected_cb_time)
    {
        expected_size += (expected_cb_time - current_time) * sdl_data->output_frequency * 4 / 1000;
    }

    return expected_size;
}

/* handle SDL Audio Thread / Core synchronization to avoid {over,under}flows */
static void synchronize_audio(struct sdl_data* sdl_data)
{
    size_t expected_size = estimate_consumable_size_at_next_audio_cb(sdl_data);

    if (expected_size >= sdl_data->target + sdl_data->output_frequency / 100)
    {
        /* Core is ahead of SDL audio thread,
         * delay emulation to allow the SDL audio thread to catch up */
        unsigned int wait_time = (expected_size - sdl_data->target) * 1000 / (4*sdl_data->output_frequency);

        SDL_PauseAudio(0);
        SDL_Delay(wait_time);
    }
    else if (expected_size < sdl_data->secondary_buffer_size)
    {
        /* Core is behind SDL audio thread (predicting an underflow),
         * pause the audio to let the Core catch up */
        SDL_PauseAudio(1);
    }
    else
    {
        /* expected size is within tolerance, everything is okay */
        SDL_PauseAudio(0);
    }
}


static void set_audio_format(void* user_data, unsigned int frequency, unsigned int bits)
{
    struct sdl_data* sdl_data = (struct sdl_data*)user_data;

    //DebugMessage(M64MSG_WARNING, "SDL audio backend set format: %dHz %d bits", frequency, bits);

    /* XXX: assume 16bit samples whatever */
    if (bits != 16)
    {
        DebugMessage(M64MSG_WARNING, "Incoming samples are not 16 bits (%d)", bits);
    }

    sdl_data->input_frequency = frequency;
    init_audio_device(sdl_data);
}

static void push_audio_samples(void* user_data, const void* buffer, size_t size)
{
    struct sdl_data* sdl_data = (struct sdl_data*)user_data;

    //DebugMessage(M64MSG_WARNING, "SDL audio backend push samples: @%p  0x%x bytes", buffer, size);

    if (sdl_data->error != 0 || sdl_data->input_frequency == 0 || sdl_data->output_frequency == 0)
        return;

    push_incoming_samples(sdl_data, buffer, size);
    synchronize_audio(sdl_data);
}


static m64p_error init(void* object)
{
    const struct object_factory* resampler_factory;
    struct sdl_data* sdl_data;

    /* XXX: parse config file to retrieve the values */
    size_t primary_buffer_size = 16384 * 4;
    size_t target = 10240 * 4;
    size_t secondary_buffer_size = 2048 * 4;
    unsigned int swap_channels = 1;
    const char* resampler_name = "trivial";

    struct m64p_audio_backend* backend = (struct m64p_audio_backend*)object;

    /* get resampler factory */
    resampler_factory = get_object_factory(resampler_factories, resampler_name);
    if (resampler_factory == NULL)
    {
        DebugMessage(M64MSG_ERROR, "Couldn't find resampler factory: %s", resampler_name);
        return M64ERR_INPUT_INVALID;
    }

    /* allocate and initialize sdl_data */
    sdl_data = new_sdl_data(primary_buffer_size,
                            target,
                            secondary_buffer_size,
                            resampler_factory,
                            swap_channels);
    if (sdl_data == NULL)
    {
        DebugMessage(M64MSG_ERROR, "Couldn't create new sdl_data !");
        return M64ERR_NO_MEMORY;
    }

    /* fill backend structure */
    backend->user_data = sdl_data;
    backend->set_audio_format = set_audio_format;
    backend->push_audio_samples = push_audio_samples;

    return M64ERR_SUCCESS;
}

static void release(void* object)
{
    struct m64p_audio_backend* backend = (struct m64p_audio_backend*)object;
    struct sdl_data* sdl_data = (struct sdl_data*)backend->user_data;

    if (SDL_WasInit(SDL_INIT_AUDIO) != 0)
    {
        SDL_PauseAudio(1);
        SDL_CloseAudio();
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }

    if (SDL_WasInit(SDL_INIT_TIMER) != 0)
        SDL_QuitSubSystem(SDL_INIT_TIMER);

    delete_sdl_data(sdl_data);
    memset(backend, 0, sizeof(*backend));
}

const struct object_factory sdl_audio_backend_factory = { "sdl", init, release };

