#include <stdio.h>
#include <string.h>

#include "core_interface.h"

// For usleep
#include <unistd.h>

/*
 * Debugger vars
 */

// Used to wait for core response before requesting next command.
int debugger_loop_wait = 1;

// Counter indicating the number of DebugStep() calls we need to make yet.
static int debugger_steps_pending = 0;

// Keep track of the run state.
static int cur_run_state = 0;

/*
 * Debugger callbacks.
 */
void dbg_frontend_init() {
    printf("Debugger initialized.\n");
}

void dbg_frontend_update(unsigned int pc) {
    if (!debugger_steps_pending) {
        printf("PC at 0x%x.\n", pc);
        debugger_loop_wait = 0;
    }
    else {
        --debugger_steps_pending;
        debugger_step();
    }
}

void dbg_frontend_vi() {
    //printf("Debugger vertical int.\n");
}

/*
 * Debugger methods.
 */
int debugger_setup_callbacks() {
    m64p_error rval = (*DebugSetCallbacks)(dbg_frontend_init,
                                           dbg_frontend_update,
                                           dbg_frontend_vi);
    return rval != M64ERR_SUCCESS;
}

int debugger_set_run_state(int state) {
    m64p_error rval = (*DebugSetRunState)(state);
    return rval != M64ERR_SUCCESS;
}

int debugger_step() {
    m64p_error rval = (*DebugStep)();
    return rval != M64ERR_SUCCESS;
}

/*
 * Debugger main loop
 */
void *debugger_loop(void *arg) {
    char input[256];
    while (1) {
        if (debugger_loop_wait) {
            usleep(100);
            continue;
        }

        printf("(dbg) ");
        fgets(input, 256, stdin);
        input[strlen(input) - 1] = 0;

        if (strcmp(input, "run") == 0) {
            cur_run_state = 2;
            if (debugger_set_run_state(cur_run_state))
                printf("Error setting run_state: run\n");
            else {
                debugger_step(); // Hack to kick-start the emulation.
            }
        }
        else if (strcmp(input, "pause") == 0) {
            cur_run_state = 0;
            if (debugger_set_run_state(cur_run_state))
                printf("Error setting run_state: pause\n");
        }
        else if (strncmp(input, "step", 4) == 0) {
            if (cur_run_state == 2) {
              printf("Cannot step while running. Type `pause' first.\n");
              continue;
            }

            debugger_loop_wait = 1;
            debugger_steps_pending = 1;
            sscanf(input, "step %d", &debugger_steps_pending);
            if (debugger_steps_pending < 1)
                debugger_steps_pending = 1;
            --debugger_steps_pending;
            debugger_step();
        }
        else if (strlen(input) == 0)
            continue;
        else
            printf("Unrecognized: %s\n", input);
    }
}

