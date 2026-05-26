// Dejo comentadas las lineas del main, por ahora defino la funcion general como en el stick
//void atender_cliente_mock(int cliente_fd, t_log* logger, t_dictionary* diccionario, char* basepath);
//void procesar_iniciar_proceso(int cliente_fd, t_log* logger, t_dictionary* diccionario, char* basepath);
//void procesar_pedir_instruccion(int cliente_fd, t_log* logger, t_dictionary* diccionario);

#ifndef KERNEL_UTILS_H_
#define KERNEL_UTILS_H|

#include <stdio.h>
#include <stdlib.h>
#include <commons/log.h>
#include <commons/config.h>
#include <utils/utils.h>

typedef struct {
    t_log* logger;
    t_config* config;
    int socket_servidor;
} t_kernel_memory_info;

// Procedimiento general del kernel memory
void iniciar_operacion_kernel_memory(char* config_path);

#endif /* KERNEL_UTILS_H_ */
