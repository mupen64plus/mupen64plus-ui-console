/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - eventloop.c                                             *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2008-2009 Richard Goedeken                              *
 *   Copyright (C) 2008 Ebenblues Nmn Okaygo Tillin9                       *
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
#include <SDL.h>

#include "eventloop.h"
#include "core_interface.h"
#include "main.h"

/* version number for CoreEvents config section */
#define CONFIG_PARAM_VERSION 1.00

/*********************************************************************************************************
* static variables and definitions for eventloop.c
*/

#define kbdFullscreen "Kbd Mapping Fullscreen"
#define kbdStop "Kbd Mapping Stop"
#define kbdPause "Kbd Mapping Pause"
#define kbdSave "Kbd Mapping Save State"
#define kbdLoad "Kbd Mapping Load State"
#define kbdIncrement "Kbd Mapping Increment Slot"
#define kbdReset "Kbd Mapping Reset"
#define kbdSpeeddown "Kbd Mapping Speed Down"
#define kbdSpeedup "Kbd Mapping Speed Up"
#define kbdScreenshot "Kbd Mapping Screenshot"
#define kbdMute "Kbd Mapping Mute"
#define kbdIncrease "Kbd Mapping Increase Volume"
#define kbdDecrease "Kbd Mapping Decrease Volume"
#define kbdForward "Kbd Mapping Fast Forward"
#define kbdAdvance "Kbd Mapping Frame Advance"
#define kbdGameshark "Kbd Mapping Gameshark"

typedef enum {joyFullscreen,
              joyStop,
              joyPause,
              joySave,
              joyLoad,
              joyIncrement,
              joyScreenshot,
              joyMute,
              joyIncrease,
              joyDecrease,
              joyForward,
              joyGameshark
} eJoyCommand;

static const char *JoyCmdName[] = { "Joy Mapping Fullscreen",
                                    "Joy Mapping Stop",
                                    "Joy Mapping Pause",
                                    "Joy Mapping Save State",
                                    "Joy Mapping Load State",
                                    "Joy Mapping Increment Slot",
                                    "Joy Mapping Screenshot",
                                    "Joy Mapping Mute",
                                    "Joy Mapping Increase Volume",
                                    "Joy Mapping Decrease Volume",
                                    "Joy Mapping Fast Forward",
                                    "Joy Mapping Gameshark"};

static const int NumJoyCommands = sizeof(JoyCmdName) / sizeof(const char *);

static int JoyCmdActive[16];  /* if extra joystick commands are added above, make sure there is enough room in this array */

/*********************************************************************************************************
* static functions for eventloop.c
*/

/** MatchJoyCommand
 *    This function processes an SDL event and updates the JoyCmdActive array if the
 *    event matches with the given command.
 *
 *    If the event activates a joystick command which was not previously active, then
 *    a 1 is returned.  If the event de-activates an command, a -1 is returned.  Otherwise
 *    (if the event does not match the command or active status didn't change), a 0 is returned.
 */
static int MatchJoyCommand(const SDL_Event *event, eJoyCommand cmd)
{
    const char *event_str = ConfigGetParamString(g_CoreEventsConfig, JoyCmdName[cmd]);
    int dev_number, input_number, input_value;
    char axis_direction;

    /* Empty string or non-joystick command */
    if (event_str == NULL || strlen(event_str) < 4 || event_str[0] != 'J')
        return 0;

    /* Evaluate event based on type of joystick input expected by the given command */
    switch (event_str[2])
    {
        /* Axis */
        case 'A':
            if (event->type != SDL_JOYAXISMOTION)
                return 0;
            if (sscanf(event_str, "J%dA%d%c", &dev_number, &input_number, &axis_direction) != 3)
                return 0;
            if (dev_number != event->jaxis.which || input_number != event->jaxis.axis)
                return 0;
            if (axis_direction == '+')
            {
                if (event->jaxis.value >= 15000 && JoyCmdActive[cmd] == 0)
                {
                    JoyCmdActive[cmd] = 1;
                    return 1;
                }
                else if (event->jaxis.value <= 8000 && JoyCmdActive[cmd] == 1)
                {
                    JoyCmdActive[cmd] = 0;
                    return -1;
                }
                return 0;
            }
            else if (axis_direction == '-')
            {
                if (event->jaxis.value <= -15000 && JoyCmdActive[cmd] == 0)
                {
                    JoyCmdActive[cmd] = 1;
                    return 1;
                }
                else if (event->jaxis.value >= -8000 && JoyCmdActive[cmd] == 1)
                {
                    JoyCmdActive[cmd] = 0;
                    return -1;
                }
                return 0;
            }
            else return 0; /* invalid axis direction in configuration parameter */
            break;
        /* Hat */
        case 'H':
            if (event->type != SDL_JOYHATMOTION)
                return 0;
            if (sscanf(event_str, "J%dH%dV%d", &dev_number, &input_number, &input_value) != 3)
                return 0;
            if (dev_number != event->jhat.which || input_number != event->jhat.hat)
                return 0;
            if ((event->jhat.value & input_value) == input_value && JoyCmdActive[cmd] == 0)
            {
                JoyCmdActive[cmd] = 1;
                return 1;
            }
            else if ((event->jhat.value & input_value) != input_value  && JoyCmdActive[cmd] == 1)
            {
                JoyCmdActive[cmd] = 0;
                return -1;
            }
            return 0;
            break;
        /* Button. */
        case 'B':
            if (event->type != SDL_JOYBUTTONDOWN && event->type != SDL_JOYBUTTONUP)
                return 0;
            if (sscanf(event_str, "J%dB%d", &dev_number, &input_number) != 2)
                return 0;
            if (dev_number != event->jbutton.which || input_number != event->jbutton.button)
                return 0;
            if (event->type == SDL_JOYBUTTONDOWN && JoyCmdActive[cmd] == 0)
            {
                JoyCmdActive[cmd] = 1;
                return 1;
            }
            else if (event->type == SDL_JOYBUTTONUP && JoyCmdActive[cmd] == 1)
            {
                JoyCmdActive[cmd] = 0;
                return -1;
            }
            return 0;
            break;
        default:
            /* bad configuration parameter */
            return 0;
    }

    /* impossible to reach this point */
    return 0;
}

/*********************************************************************************************************
* sdl event filter
*/
static int event_sdl_filter(const SDL_Event *event)
{
    int cmd, action;

    switch(event->type)
    {
        // user clicked on window close button
        case SDL_QUIT:
            stop_emu();
            break;

        case SDL_KEYDOWN:
            event_sdl_keydown(event->key.keysym.sym, event->key.keysym.mod);
            return 0;
        case SDL_KEYUP:
            event_sdl_keyup(event->key.keysym.sym, event->key.keysym.mod);
            return 0;

        // if joystick action is detected, check if it's mapped to a special function
        case SDL_JOYAXISMOTION:
        case SDL_JOYBUTTONDOWN:
        case SDL_JOYBUTTONUP:
        case SDL_JOYHATMOTION:
            for (cmd = 0; cmd < NumJoyCommands; cmd++)
            {
                action = MatchJoyCommand(event, (eJoyCommand) cmd);
                if (action == 1) /* command was just activated (button down, etc) */
                {
                    if (cmd == joyFullscreen)
                        toggle_fullscreen();
                    else if (cmd == joyStop)
                        stop_emu();
                    else if (cmd == joyPause)
                        toggle_pause();
                    else if (cmd == joySave)
                        savestate_save(1, NULL); /* save in mupen64plus format using current slot */
                    else if (cmd == joyLoad)
                        savestate_load(NULL); /* load using current slot */
                    else if (cmd == joyIncrement)
                        savestate_inc_slot();
                    else if (cmd == joyScreenshot)
                        main_take_next_screenshot();
                    else if (cmd == joyMute)
                        volume_mute();
                    else if (cmd == joyDecrease)
                        volume_down();
                    else if (cmd == joyIncrease)
                        volume_up();
                    else if (cmd == joyForward)
                        set_fastforward(1);
                }
                else if (action == -1) /* command was just de-activated (button up, etc) */
                {
                    if (cmd == joyForward)
                        set_fastforward(0);
                }
            }

            return 0;
            break;
    }

    return 1;
}

/*********************************************************************************************************
* global functions
*/

void event_initialize(void)
{
    const char *event_str = NULL;
    int i;

    // initializing the video system is required to recieve event notifications
    if (!SDL_WasInit(SDL_INIT_VIDEO))
        SDL_InitSubSystem(SDL_INIT_VIDEO);

    /* set initial state of all joystick commands to 'off' */
    for (i = 0; i < NumJoyCommands; i++)
        JoyCmdActive[i] = 0;

    /* activate any joysticks which are referenced in the joystick event command strings */
    for (i = 0; i < NumJoyCommands; i++)
    {
        event_str = ConfigGetParamString(g_CoreEventsConfig, JoyCmdName[i]);
        if (event_str != NULL && strlen(event_str) >= 4 && event_str[0] == 'J' && event_str[1] >= '0' && event_str[1] <= '9')
        {
            int device = event_str[1] - '0';
            if (!SDL_WasInit(SDL_INIT_JOYSTICK))
                SDL_InitSubSystem(SDL_INIT_JOYSTICK);
            if (!SDL_JoystickOpened(device))
                SDL_JoystickOpen(device);
        }
    }

    /* set up SDL event filter and disable key repeat */
    SDL_EnableKeyRepeat(0, 0);
    SDL_SetEventFilter(event_sdl_filter);
}

void event_set_core_defaults(void)
{
    // TODO XXX should move this to UI config section
    float fConfigParamsVersion;
    int bSaveConfig = 0;

    if (ConfigGetParameter(g_CoreEventsConfig, "Version", M64TYPE_FLOAT, &fConfigParamsVersion, sizeof(float)) != M64ERR_SUCCESS)
    {
        DebugMessage(M64MSG_WARNING, "No version number in 'CoreEvents' config section. Setting defaults.");
        ConfigDeleteSection("CoreEvents");
        ConfigOpenSection("CoreEvents", &g_CoreEventsConfig);
        bSaveConfig = 1;
    }
    else if (((int) fConfigParamsVersion) != ((int) CONFIG_PARAM_VERSION))
    {
        DebugMessage(M64MSG_WARNING, "Incompatible version %.2f in 'CoreEvents' config section: current is %.2f. Setting defaults.", fConfigParamsVersion, (float) CONFIG_PARAM_VERSION);
        ConfigDeleteSection("CoreEvents");
        ConfigOpenSection("CoreEvents", &g_CoreEventsConfig);
        bSaveConfig = 1;
    }
    else if ((CONFIG_PARAM_VERSION - fConfigParamsVersion) >= 0.0001f)
    {
        /* handle upgrades */
        float fVersion = CONFIG_PARAM_VERSION;
        ConfigSetParameter(g_CoreEventsConfig, "Version", M64TYPE_FLOAT, &fVersion);
        DebugMessage(M64MSG_INFO, "Updating parameter set version in 'CoreEvents' config section to %.2f", fVersion);
        bSaveConfig = 1;
    }

    ConfigSetDefaultFloat(g_CoreEventsConfig, "Version", CONFIG_PARAM_VERSION,  "Mupen64Plus CoreEvents config parameter set version number.  Please don't change this version number.");

    /* Keyboard presses mapped to core functions */
    ConfigSetDefaultInt(g_CoreEventsConfig, kbdStop, SDLK_ESCAPE,          "SDL keysym for stopping the emulator");
    ConfigSetDefaultInt(g_CoreEventsConfig, kbdFullscreen, SDLK_LAST,      "SDL keysym for switching between fullscreen/windowed modes");
    ConfigSetDefaultInt(g_CoreEventsConfig, kbdSave, SDLK_F5,              "SDL keysym for saving the emulator state");
    ConfigSetDefaultInt(g_CoreEventsConfig, kbdLoad, SDLK_F7,              "SDL keysym for loading the emulator state");
    ConfigSetDefaultInt(g_CoreEventsConfig, kbdIncrement, 0,               "SDL keysym for advancing the save state slot");
    ConfigSetDefaultInt(g_CoreEventsConfig, kbdReset, SDLK_F9,             "SDL keysym for resetting the emulator");
    ConfigSetDefaultInt(g_CoreEventsConfig, kbdSpeeddown, SDLK_F10,        "SDL keysym for slowing down the emulator");
    ConfigSetDefaultInt(g_CoreEventsConfig, kbdSpeedup, SDLK_F11,          "SDL keysym for speeding up the emulator");
    ConfigSetDefaultInt(g_CoreEventsConfig, kbdScreenshot, SDLK_F12,       "SDL keysym for taking a screenshot");
    ConfigSetDefaultInt(g_CoreEventsConfig, kbdPause, SDLK_p,              "SDL keysym for pausing the emulator");
    ConfigSetDefaultInt(g_CoreEventsConfig, kbdMute, SDLK_m,               "SDL keysym for muting/unmuting the sound");
    ConfigSetDefaultInt(g_CoreEventsConfig, kbdIncrease, SDLK_RIGHTBRACKET,"SDL keysym for increasing the volume");
    ConfigSetDefaultInt(g_CoreEventsConfig, kbdDecrease, SDLK_LEFTBRACKET, "SDL keysym for decreasing the volume");
    ConfigSetDefaultInt(g_CoreEventsConfig, kbdForward, SDLK_f,            "SDL keysym for temporarily going really fast");
    ConfigSetDefaultInt(g_CoreEventsConfig, kbdAdvance, SDLK_SLASH,        "SDL keysym for advancing by one frame when paused");
    ConfigSetDefaultInt(g_CoreEventsConfig, kbdGameshark, SDLK_g,          "SDL keysym for pressing the game shark button");
    /* Joystick events mapped to core functions */
    ConfigSetDefaultString(g_CoreEventsConfig, JoyCmdName[joyStop], "",       "Joystick event string for stopping the emulator");
    ConfigSetDefaultString(g_CoreEventsConfig, JoyCmdName[joyFullscreen], "", "Joystick event string for switching between fullscreen/windowed modes");
    ConfigSetDefaultString(g_CoreEventsConfig, JoyCmdName[joySave], "",       "Joystick event string for saving the emulator state");
    ConfigSetDefaultString(g_CoreEventsConfig, JoyCmdName[joyLoad], "",       "Joystick event string for loading the emulator state");
    ConfigSetDefaultString(g_CoreEventsConfig, JoyCmdName[joyIncrement], "",  "Joystick event string for advancing the save state slot");
    ConfigSetDefaultString(g_CoreEventsConfig, JoyCmdName[joyScreenshot], "", "Joystick event string for taking a screenshot");
    ConfigSetDefaultString(g_CoreEventsConfig, JoyCmdName[joyPause], "",      "Joystick event string for pausing the emulator");
    ConfigSetDefaultString(g_CoreEventsConfig, JoyCmdName[joyMute], "",       "Joystick event string for muting/unmuting the sound");
    ConfigSetDefaultString(g_CoreEventsConfig, JoyCmdName[joyIncrease], "",   "Joystick event string for increasing the volume");
    ConfigSetDefaultString(g_CoreEventsConfig, JoyCmdName[joyDecrease], "",   "Joystick event string for decreasing the volume");
    ConfigSetDefaultString(g_CoreEventsConfig, JoyCmdName[joyForward], "",    "Joystick event string for fast-forward");
    ConfigSetDefaultString(g_CoreEventsConfig, JoyCmdName[joyGameshark], "",  "Joystick event string for pressing the game shark button");

    if (bSaveConfig)
        ConfigSaveSection("CoreEvents");
}

/*********************************************************************************************************
* sdl keyup/keydown handlers
*/

void event_sdl_keydown(int keysym, int keymod)
{
    /* check for the only 2 hard-coded key commands: Alt-enter for fullscreen and 0-9 for save state slot */
    if (keysym == SDLK_RETURN && keymod & (KMOD_LALT | KMOD_RALT))
        toggle_fullscreen();
    else if (keysym >= SDLK_0 && keysym <= SDLK_9)
        savestates_set_slot(keysym - SDLK_0);
    /* check all of the configurable commands */
    else if (keysym == ConfigGetParamInt(g_CoreEventsConfig, kbdStop))
        stop_emu();
    else if (keysym == ConfigGetParamInt(g_CoreEventsConfig, kbdFullscreen))
        toggle_fullscreen();
    else if (keysym == ConfigGetParamInt(g_CoreEventsConfig, kbdSave))
        savestate_save(1, NULL); /* save in mupen64plus format using current slot */
    else if (keysym == ConfigGetParamInt(g_CoreEventsConfig, kbdLoad))
        savestate_load(NULL); /* load using current slot */
    else if (keysym == ConfigGetParamInt(g_CoreEventsConfig, kbdIncrement))
        savestate_inc_slot();
    else if (keysym == ConfigGetParamInt(g_CoreEventsConfig, kbdReset))
        soft_reset();
    else if (keysym == ConfigGetParamInt(g_CoreEventsConfig, kbdSpeeddown))
        speed_delta(-5);
    else if (keysym == ConfigGetParamInt(g_CoreEventsConfig, kbdSpeedup))
        speed_delta(5);
    else if (keysym == ConfigGetParamInt(g_CoreEventsConfig, kbdScreenshot))
        main_take_next_screenshot();
    else if (keysym == ConfigGetParamInt(g_CoreEventsConfig, kbdPause))
        toggle_pause();
    else if (keysym == ConfigGetParamInt(g_CoreEventsConfig, kbdMute))
        volume_mute();
    else if (keysym == ConfigGetParamInt(g_CoreEventsConfig, kbdIncrease))
        volume_up();
    else if (keysym == ConfigGetParamInt(g_CoreEventsConfig, kbdDecrease))
        volume_down();
    else if (keysym == ConfigGetParamInt(g_CoreEventsConfig, kbdForward))
        set_fastforward(1);
    else if (keysym == ConfigGetParamInt(g_CoreEventsConfig, kbdAdvance))
        frame_advance();
    else if (keysym == ConfigGetParamInt(g_CoreEventsConfig, kbdGameshark))
        set_gameshark_button(1);
    else /* pass all other keypresses to the input plugin */
        send_key_down(keymod, keysym);
}

void event_sdl_keyup(int keysym, int keymod)
{
    if (keysym == ConfigGetParamInt(g_CoreEventsConfig, kbdStop))
        return;
    else if (keysym == ConfigGetParamInt(g_CoreEventsConfig, kbdForward))
        set_fastforward(0);
    else if (keysym == ConfigGetParamInt(g_CoreEventsConfig, kbdGameshark))
        set_gameshark_button(0);
    else
        send_key_up(keymod, keysym);
}


