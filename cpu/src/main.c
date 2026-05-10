#include "cpu_utils.h"
#include <commons/config.h>
#include <commons/log.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <utils/hello.h>
#include <utils/utils.h>

int main(int argc, char *argv[]) {
  saludar("cpu");
  /*
    // valido cantidad de argumentos
    if (argc < 3) {
      printf(
          "Error: Faltan argumentos. Uso: ./bin/cpu [Config]
    [Identificador]\n"); return 1;
    }
*/
  // Saco validacion porque me rompe el debug
  char *id_cpu = argv[2];

  // creo el logger
  char nombre_logger[20];
  sprintf(nombre_logger, "CPU_%s", id_cpu);
  t_log *logger_cpu =
      log_create("cpu.log", nombre_logger, true, LOG_LEVEL_INFO);

  // Validacion archivo de log
  if (logger_cpu == NULL) {
    printf("Error al crear el logger\n");
    return 1;
  } else {
    printf("Archivo de log creado correctamente\n");
  }

  t_config *config_plana = config_create("cpu.config");
  t_config_cpu config_cpu;

  // Validacion de archivo de config
  if (config_plana == NULL) {
    log_error(logger_cpu, "No se pudo encontrar el archivo cpu.config");
    return 1;
  } else {
    printf("Archivo de cofiguracion creado correctamente\n");
  }

  t_registros registros;

  cargar_configuracion(&config_cpu, config_plana);

  if (!cargar_configuracion(&config_cpu, config_plana)) {
    log_error(logger_cpu, "Error al cargar configuración");
    return 1;
  }

  // CONEXIONES
  // Aca fd es File descriptor.

  int fd_memory = conectar_a_modulo("Kernel Memory", config_cpu.ip_memory,
                                    config_cpu.puerto_memory, logger_cpu);
  int fd_scheduler =
      conectar_a_modulo("Kernel Scheduler", config_cpu.ip_scheduler,
                        config_cpu.puerto_scheduler, logger_cpu);
  int fd_stick = conectar_a_modulo("Memory Stick", config_cpu.ip_memory_stick,
                                   config_cpu.puerto_memory_stick, logger_cpu);

  // Validacion
  if (fd_memory == -1 || fd_scheduler == -1 || fd_stick == -1) {
    log_error(logger_cpu, "No se pudieron establecer todas las conexiones "
                          "necesarias. Libero memoria y termino programa.");
    // ... liberar memoria y salir
    liberar_conexion(fd_memory);
    liberar_conexion(fd_scheduler);
    liberar_conexion(fd_stick);
    return 1;
  }

  // libero conexiones
  liberar_conexion(fd_memory);
  liberar_conexion(fd_scheduler);
  liberar_conexion(fd_stick);
  config_destroy(config_plana);
  log_destroy(logger_cpu);

  return 0;
  ;
}