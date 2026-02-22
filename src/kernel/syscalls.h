#ifndef SYSCALLS_H
#define SYSCALLS_H

#include "idt.h" // <--- ABSOLUMENT NECESSAIRE pour définir struct regs ici

void syscall_handler(struct regs *r);

#endif