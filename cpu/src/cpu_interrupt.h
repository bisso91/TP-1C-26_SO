#ifndef CPU_INTERRUPT_H_
#define CPU_INTERRUPT_H_

#include <commons/config.h>
#include <commons/log.h>
#include <commons/string.h>
#include <pthread.h>
#include <readline/readline.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <utils/utils.h>

extern t_log *logger;
extern t_config *config;
extern int interrupt_fd;
extern int kernel_interrupt_fd;
extern uint32_t interrupted_pid;
void *interrupt_server(void *);

#endif /* CPU_INTERRUPT_H_ */
