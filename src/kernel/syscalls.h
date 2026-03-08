#ifndef SYSCALLS_H
#define SYSCALLS_H

#include "idt.h" // <--- ABSOLUMENT NECESSAIRE pour définir struct regs ici

extern char global_cmd_buffer[128];

void syscall_handler(struct regs *r);
void set_current_command(const char* cmd);

#endif