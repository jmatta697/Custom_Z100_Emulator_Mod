#include <signal.h>
#include "8088.h"

extern volatile sig_atomic_t breakpoint_active;

void bp_exec_check(unsigned int address);
void bp_read_check(unsigned int address);
void bp_write_check(unsigned int address);
void bp_in_check(unsigned int address);
void bp_out_check(unsigned int address);
void dbg_init(void);
void dbg_reg(P8088 *p8088);
int dbg_cmd(P8088 *p8088, const char *command);
void out_error(const char *format, ...);
