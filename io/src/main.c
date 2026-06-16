#include <commons/config.h>
#include <commons/log.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <utils/hello.h>
#include <utils/utils.h>

#include "io_funciones.h"

int main(int argc, char *argv[]) {
  saludar("io");

  if (argc < 3) {
    printf("Error: Faltan argumentos. Uso: ./bin/io [Archivo de Configuración] [Tipo]\n");
    return 1;
  }

  char *tipo_io = argv[2];

  t_log *logger = log_create("io.log", "IO", true, LOG_LEVEL_INFO);

  if (logger == NULL) {
    printf("Error al crear el logger\n");
    return 1;
  }

  t_config *config = config_create(argv[1]);
  if (config == NULL) {
    log_error(logger, "No se pudo encontrar el arhcivo io.config");
    log_destroy(logger);
    return 1;
  }

  // obtengo el ip y el puerto a conectar
  char *ip = config_get_string_value(config, "IP_SERVIDOR");
  char *puerto = config_get_string_value(config, "PUERTO_SERVIDOR");

  // conecto al ip y al mismo puerto que el kernel_memory
  int conexion_scheduler = crear_conexion(ip, puerto);
  if (conexion_scheduler != -1) {
    log_info(logger, "## Conectado a Kernel Scheduler");
    log_info(logger, "## Conectado en puerto: %s con IP: %s", puerto, ip);
    enviar_string(tipo_io, conexion_scheduler, IDENTIFICACION_IO);
    
    // bucle de IN/OUT
    iniciar_bucle_io(conexion_scheduler, tipo_io, logger);
  } else {
    log_error(logger, "Error al intentar conectarse al Kernel Scheduler");
  }

  
  liberar_conexion(conexion_scheduler);
  config_destroy(config);
  log_destroy(logger);

  return 0;
}
