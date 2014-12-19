#ifndef __DEBUGGER_H__
#define __DEBUGGER_H__

int debugger_loop_wait;

//void dbg_frontend_init();
//void dbg_frontend_update(unsigned int pc);
//void dbg_frontend_vi();

int debugger_setup_callbacks();
int debugger_step();
int debugger_loop(void *arg);

#endif /* __DEBUGGER_H__ */
