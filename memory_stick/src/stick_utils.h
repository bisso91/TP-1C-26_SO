#ifndef STICK_UTILS_H_
#define STICK_UTILS_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <commons/log.h>
#include <commons/config.h>
#include <utils/utils.h>

// Defino el struct 
typedef struct {
    t_log* logger;
    t_config* config;
    void* espacio_usuario;
    int tamanio_memoria;
    int socket_servidor;
} t_memory_stick_info;

// Defino el procedimiento general del stick
void iniciar_operacion_stick(char* config_path, int tamanio);
void ejecutar_lectura_stick(int cliente_fd, void* espacio_usuario, int tamanio_memoria, int retardo, t_log* logger);
void ejecutar_escritura_stick(int cliente_fd, void* espacio_usuario, int tamanio_memoria, int retardo, t_log* logger);

#endif /* STICK_UTILS_H_ */
