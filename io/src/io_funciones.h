#ifndef IO_FUNCIONES_H_
#define IO_FUNCIONES_H_

#include <commons/log.h>
#include <stdbool.h>

// fn principal
void iniciar_bucle_io(int conexion_scheduler, char *tipo_io, t_log *logger);

// fn's especificas

void ejecutar_sleep(int pid, int conexion_shceduler, t_log *logger);
void ejecutar_stdin(int pid, int conexion_shceduler, t_log *logger);
void ejecutar_stdout(int pid, int conexion_shceduler, t_log *logger);

#endif
