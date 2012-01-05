/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - screenshot.c                                            *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2008 Richard42                                          *
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "screenshot.h"
#include "plugin.h"
#include "version.h"
#include "core_interface.h"
#include "osal_files.h"

#include <png.h>

/*********************************************************************************************************
* PNG support functions for writing screenshot files
*/

static void mupen_png_error(png_structp png_write, const char *message)
{
    fprintf(stderr, "UI-console: PNG Error: %s\n", message);
}

static void mupen_png_warn(png_structp png_write, const char *message)
{
    fprintf(stderr, "UI-console: PNG Warning: %s\n", message);
}

static void user_write_data(png_structp png_write, png_bytep data, png_size_t length)
{
    FILE *fPtr = (FILE *) png_get_io_ptr(png_write);
    if (fwrite(data, 1, length, fPtr) != length)
        fprintf(stderr, "UI-console: Failed to write %zi bytes to screenshot file.\n", length);
}

static void user_flush_data(png_structp png_write)
{
    FILE *fPtr = (FILE *) png_get_io_ptr(png_write);
    fflush(fPtr);
}

/*********************************************************************************************************
* Other Local (static) functions
*/

static void SaveRGBBufferToFile(char *filename, unsigned char *buf, int width, int height, int pitch)
{
    int i;
    png_structp png_write = NULL;
    png_infop png_info = NULL;
    png_byte **row_pointers = NULL;
    FILE *savefile = NULL;

    // allocate PNG structures
    png_write = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, mupen_png_error, mupen_png_warn);
    if (!png_write)
    {
        fprintf(stderr, "UI-console: Error creating PNG write struct.\n");
        goto cleanup;
    }
    png_info = png_create_info_struct(png_write);
    if (!png_info)
    {
        fprintf(stderr, "UI-console: Error creating PNG info struct.\n");
        goto cleanup;
    }
    // Set the jumpback
    if (setjmp(png_jmpbuf(png_write)))
    {
        fprintf(stderr, "UI-console: Error calling setjmp()\n");
        goto cleanup;
    }
    // open the file to write
    savefile = fopen(filename, "wb");
    if (savefile == NULL)
    {
        fprintf(stderr, "UI-console: Error opening '%s' to save screenshot.\n", filename);
        goto cleanup;
    }

    // set function pointers in the PNG library, for write callbacks
    png_set_write_fn(png_write, (png_voidp) savefile, user_write_data, user_flush_data);

    // set the info
    png_set_IHDR(png_write, png_info, width, height, 8, PNG_COLOR_TYPE_RGB,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    // allocate row pointers and scale each row to 24-bit color
    row_pointers = (png_byte **) malloc(height * sizeof(png_bytep));
    if (row_pointers == NULL)
    {
        fprintf(stderr, "UI-console: Error allocating PNG row pointers.\n");
        goto cleanup;
    }

    for (i = 0; i < height; i++)
        row_pointers[i] = (png_byte *) (buf + (height - 1 - i) * pitch);

    // set the row pointers
    png_set_rows(png_write, png_info, row_pointers);

    // write the picture to disk
    png_write_png(png_write, png_info, 0, NULL);

    cleanup:
        if (savefile != NULL)
            fclose(savefile);
        free(row_pointers);
        png_destroy_write_struct(&png_write, &png_info);
}

static int CurrentShotIndex = 0;

char *GetNextScreenshotFileName()
{
    // TODO XXX check alloc failure
    int i;
    m64p_rom_header hdr;
    const char *SshotDir;
    char *BaseDir, *filename;
    FILE *file;

    // get the game's name, convert to lowercase, convert spaces to underscores
    (*CoreDoCommand)(M64CMD_ROM_GET_HEADER, sizeof(hdr), &hdr);

    for (i = 0; i < sizeof(hdr.Name); i++) {
        char ch = hdr.Name[i];
        if (ch == ' ')
            ch = '_';
        else
            ch = tolower(hdr.Name[i]);
        hdr.Name[i] = ch;
    }

    // get the default screenshot path
    SshotDir = ConfigGetParamString(g_ConfigUI, "ScreenshotPath");
    if (strlen(SshotDir) == 0)
    {
        const char *UserDataPath = ConfigGetUserDataPath();
        BaseDir = (char *) malloc(strlen(UserDataPath) + strlen("screenshot/") + 1);
        sprintf(BaseDir, "%sscreenshot%c", UserDataPath, OSAL_DIR_SEPARATOR);
    }
    else
    {
        size_t len = strlen(SshotDir);
        BaseDir = (char *) malloc(len + 1 + 1);
        strcpy(BaseDir, SshotDir);

        // make sure there is a slash on the end of the pathname
        if (SshotDir[len-1] != OSAL_DIR_SEPARATOR)
        {
            BaseDir[len] = OSAL_DIR_SEPARATOR;
            BaseDir[len+1] = '\0';
        }
    }

    osal_mkdirp(BaseDir, 0700);

    // look for an unused screenshot filename
    filename = (char *) malloc(strlen(BaseDir) + 20 + 1 + 3 + 4 + 1);
    for (; CurrentShotIndex < 1000; CurrentShotIndex++) {
        sprintf(filename, "%s%.20s-%03i.png", BaseDir, hdr.Name, CurrentShotIndex);
        file = fopen(filename, "r");
        if (file == NULL)
            break;
        fclose(file);
    }

    if (CurrentShotIndex >= 1000)
    {
        fprintf(stderr, "UI-console: Can't save screenshot; folder already contains 1000 screenshots for this ROM.\n");
        return NULL;
    }

    free(BaseDir);

    return filename;
}

/*********************************************************************************************************
* Global screenshot functions
*/

void ScreenshotRomOpen()
{
    CurrentShotIndex = 0;
}

void TakeScreenshot(int iFrameNumber)
{
    char *filename = NULL;
    int width, height;
    unsigned char *pucFrame = NULL;

    // get the name of the screenshot file
    filename = GetNextScreenshotFileName();
    if (filename == NULL)
        goto cleanup;

    // get the width and height
    if ((*CoreDoCommand)(M64CMD_GET_SCREEN_WIDTH, 0, &width) != M64ERR_SUCCESS ||
        (*CoreDoCommand)(M64CMD_GET_SCREEN_HEIGHT, 0, &height) != M64ERR_SUCCESS)
    {
        fprintf(stderr, "UI-console: Can't get the dimensions of the screenshot.\n");
        goto cleanup;
    }

    // allocate memory for the image
    pucFrame = (unsigned char *) malloc(width * height * 3);
    if (pucFrame == NULL)
    {
        fprintf(stderr, "UI-console: Can't allocate memory for the screenshot.\n");
        goto cleanup;
    }

    // grab the back image from OpenGL by calling the video plugin
    if ((*CoreDoCommand)(M64CMD_READ_SCREEN, 0, pucFrame) != M64ERR_SUCCESS)
    {
        fprintf(stderr, "UI-console: Can't retrieve the emulator screen.\n");
        goto cleanup;
    }

    // write the image to a PNG
    SaveRGBBufferToFile(filename, pucFrame, width, height, width * 3);

    // print message -- this allows developers to capture frames and use them in the regression test
    printf("Captured screenshot for frame %i.\n", iFrameNumber); // TODO XXX print on OSD

    return;

cleanup:
    free(pucFrame);
    free(filename);
}
