#ifndef __DEBUGGER_H__
#define __DEBUGGER_H__

int debugger_loop_wait;

typedef struct {
  unsigned int addr;
  unsigned char enabled;
} breakpoint_t;

int debugger_setup_callbacks();
int debugger_step();
int debugger_loop(void *arg);

#endif /* __DEBUGGER_H__ */
