#include <commons/config.h>
#include <commons/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <utils/hello.h>
#include <utils/utils.h>

int main(int argc, char *argv[]) {
  saludar("kernel_scheduler");

  //===================================================
  //     CONFIGURACION DEL PLANIFICADOR COMO CLIENTE   
  //===================================================
  t_log *logger_cliente = log_create("scheduler.log", "KERNEL_SCHEDULER", true, LOG_LEVEL_INFO);

  if (logger_cliente == NULL) {
    printf("Error al crear el logger de cliente\n");
    return 1;
  }

  t_config *config_cliente = config_create("kernel_scheduler.config");
  if (config_cliente == NULL) {
    log_error(logger_cliente, "No se pudo encontrar el arhcivo scheduler.config");
    return 1;
  }

  // obtengo el ip y el puerto a conectar
  char *ip_cliente = config_get_string_value(config_cliente, "IP_SERVIDOR");
  char *puerto_cliente =
      config_get_string_value(config_cliente, "PUERTO_SERVIDOR");

  // conecto al ip y al mismo puerto que el kernel_memory
  int conexion = crear_conexion(ip_cliente, puerto_cliente);
  if (conexion != 1) {
    log_info(logger_cliente, "## Conectado exitosamente al servidor en %s:%s",
             ip_cliente, puerto_cliente);
  } else {
    log_error(logger_cliente, "Error al intentar conectarse al servidor");
  }

  liberar_conexion(conexion);
  config_destroy(config_cliente);
  log_destroy(logger_cliente);
  //===================================================
  //     CONFIGURACION DEL PLANIFICADOR COMO SERVER
  //===================================================
  t_log *logger_server = log_create("kernel_scheduler.log", "KERNEL_SCHEDULER",
                                    true, LOG_LEVEL_INFO);
  if (logger_server == NULL) {
    printf("No se creo el logger");
    return 1;
  }

  t_config *config_server = config_create("kernel_scheduler.config");
  if (config_server == NULL) {
    log_error(logger_server,
              "No se pudo encontrar el archivo kernel_scheduler.config");
    return 1;
  }

  // extraer valores de ip y puerto
  char *ip_server = config_get_string_value(config_server, "IP_MEMORIA");
  char *puerto_server =
      config_get_string_value(config_server, "PUERTO_ESCUCHA");

  // iniciar server con ip y puerto
  int server_fd = iniciar_servidor(ip_server, puerto_server);
  log_info(logger_server,
           "Kernel Scheduler iniciado en %s:%s. Esperando conexiones...",
           ip_server, puerto_server);

  // espero clientes
  while (1) {
    int cliente_fd = esperar_cliente(server_fd);
    log_info(logger_server, "## Nuevo Cliente Conectado - FD del socket: %d",
             cliente_fd);
  }

  // libero memoria
  config_destroy(config_server);
  log_destroy(logger_server);
  return 0;
}
