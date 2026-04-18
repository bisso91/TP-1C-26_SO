#include <commons/config.h>
#include <commons/log.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <utils/hello.h>
#include <utils/utils.h>

int main(int argc, char *argv[]) {
  saludar("cpu");

  // valido cantidad de argumentos
  if (argc < 3) {
    printf(
        "Error: Faltan argumentos. Uso: ./bin/cpu [Config] [Identificador]\n");
    return 1;
  }

  char *id_cpu = argv[2];

  // creo el logger
  char nombre_logger[20];
  sprintf(nombre_logger, "CPU_%s", id_cpu);
  t_log *logger = log_create("cpu.log", nombre_logger, true, LOG_LEVEL_INFO);

  if (logger == NULL) {
    printf("Error al crear el logger\n");
    return 1;
  }

  t_config *config = config_create("cpu.config");
  if (config == NULL) {
    log_error(logger, "No se pudo encontrar el arhcivo cpu.config");
    return 1;
  }

  char *ip_memory = config_get_string_value(config, "IP_MEMORY");
  char *puerto_memory = config_get_string_value(config, "PUERTO_MEMORY");
  char *ip_scheduler = config_get_string_value(config, "IP_SCHEDULER");
  char *puerto_scheduler = config_get_string_value(config, "PUERTO_SCHEDULER");
  char *ip_memory_stick = config_get_string_value(config, "IP_STICK");
  char *puerto_memory_stick = config_get_string_value(config, "PUERTO_STICK");

  //Conexion a Kernel Memory

  int conexion_memory = crear_conexion(ip_memory, puerto_memory);
  if (conexion_memory != -1) {
    log_info(logger, "## Conectado a Kernel Memory");
  } else {
    log_error(logger, "Error al conectar a Kernel Memory");
  }

  //Conexion a Planificado Kernel

  int conexion_scheduler = crear_conexion(ip_scheduler, puerto_scheduler);
  if (conexion_scheduler != -1) {
    log_info(logger, "## Conectado a Kernel Scheduler");
  } else {
    log_error(logger, "Error al conectar a Kernel Scheduler");
  }

  // Conexion a Memory Stick
  int conexion_stick = crear_conexion(ip_memory_stick, puerto_memory_stick);
  if (conexion_stick != -1) {
    log_info(logger, "## Conectado a Memory Stick");
  } else {
    log_error(logger, "Error al conectar a Memory Stick");
  }

  //libero conexiones
  liberar_conexion(conexion_memory);
  liberar_conexion(conexion_scheduler);
  liberar_conexion(conexion_stick);
  config_destroy(config);
  log_destroy(logger);

  return 0;;
}