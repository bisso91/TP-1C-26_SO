#include <commons/config.h>
#include <commons/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <utils/hello.h>
#include <utils/utils.h>

int main(int argc, char *argv[]) {
  saludar("kernel_memory");

  t_log *logger =
      log_create("kernel_memory.log", "KERNEL_MEMORY", true, LOG_LEVEL_INFO);
  if (logger == NULL) {
    printf("No se creo el logger");
    return 1;
  }

  t_config *config = config_create("kernel_memory.config");
  if (config == NULL) {
    log_error(logger, "No se pudo encontrar el archivo kernel_memory.config");
    return 1;
  }

  // extraer valores de ip y puerto
  char *ip = config_get_string_value(config, "IP_MEMORIA");
  char *puerto = config_get_string_value(config, "PUERTO_ESCUCHA");

  // iniciar server con ip y puerto
  int server_fd = iniciar_servidor(puerto, logger);
  log_info(logger, "Kernel Memory iniciado en %s:%s. Esperando conexiones...",
           ip, puerto);

  // espero clientes
  while (1) {
    int cliente_fd = esperar_cliente(server_fd);
    log_info(logger, "## Nuevo Cliente Conectado - FD del socket: %d",
             cliente_fd);
  }

  // libero memoria
  config_destroy(config);
  log_destroy(logger);

  return 0;
}
