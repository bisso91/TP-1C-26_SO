#include "main.h"
#include <commons/log.h>
#include <utils/hello.h>
#include <utils/utils.h>

int main(int argc, char *argv[]) {
  saludar("cpu");

  // Levanto el logger
  t_log *logger_Cpu;
  t_config *config_Cpu

      logger_Cpu = iniciar_logger();

  log_info(logger_Cpu, "A VER SI ANDA");
  // Paso archivo de config
  config_Cpu = iniciar_config();

  return 0;
}

// Funciones

t_log *iniciar_logger(void) {
  t_log *logger_Cpu;
  // creo el nuevo logger
  logger_Cpu = log_create("CPU.log", "CPU", true, LOG_LEVEL_INFO);
  return logger_Cpu;
}

t_config *iniciar_config(void) {
  t_config *config_Cpu;

  // le digo de donde levantar la config
  config_Cpu = config_create("CPU.config");
  // oh la la, estoy manejando errores
  if (config_Cpu == NULL) {
    perror("Hubo un problema con el config");
    abort();
  }

  return config_Cpu;
}