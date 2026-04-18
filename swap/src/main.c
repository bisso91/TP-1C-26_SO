#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>
#include <utils/utils.h>
#include <commons/log.h>
#include <commons/config.h>


int main(int argc, char* argv[]) {
    saludar("io");

    t_log *logger = log_create("swap.log", "SWAP", true, LOG_LEVEL_INFO);

    if(logger == NULL){
        printf("Error al crear el logger\n");
        return 1;
    }

    t_config *config = config_create("swap.config");
    if (config == NULL){
        log_error(logger, "No se pudo encontrar el arhcivo swap.config");
        return 1;
    }

    //obtengo el ip y el puerto a conectar
    char *ip = config_get_string_value(config, "IP_SERVIDOR");
    char *puerto = config_get_string_value(config, "PUERTO_SERVIDOR");

    //conecto al ip y al mismo puerto que el kernel_memory
    int conexion = crear_conexion(ip, puerto);
    if (conexion != 1) {
        log_info(logger, "## Conectado exitosamente al servidor en %s:%s", ip, puerto);
    } else {
        log_error(logger, "Error al intentar conectarse al servidor");
    }

    liberar_conexion(conexion);
    config_destroy(config);
    log_destroy(logger);
    
    return 0;
}

