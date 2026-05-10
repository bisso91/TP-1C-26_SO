#include "cpu_utils.h"
#include <stdbool.h>

bool inicializar_registros(t_registros *registros, t_log *logger) {
  if (registros == NULL) {
    log_error(logger,
              "Error: Se intentó inicializar un puntero a registros nulo.");
    return false;
  }
  registros->PC = 0;
  registros->AX = 0;
  registros->BX = 0;
  registros->CX = 0;
  registros->DX = 0;
  registros->EAX = 0;
  registros->EBX = 0;
  registros->ECX = 0;
  registros->EDX = 0;
  registros->SI = 0;
  registros->DI = 0;

  log_info(logger, "## Registros inicializados correctamente (PC en 0)");
  return true;
}

bool cargar_configuracion(t_config_cpu *config_cpu, t_config *config_raw,
                          t_log *logger) {

  // 1. Validamos TODAS las claves necesarias para que no rompa
  if (!config_has_property(config_raw, "IP_MEMORY") ||
      !config_has_property(config_raw, "PUERTO_MEMORY") ||
      !config_has_property(config_raw, "IP_SCHEDULER") ||
      !config_has_property(config_raw, "PUERTO_SCHEDULER") ||
      !config_has_property(config_raw, "IP_STICK") ||
      !config_has_property(config_raw, "PUERTO_STICK") ||
      !config_has_property(config_raw, "LOG_LEVEL")) {
    return false;
  }

  // 2. Extraemos los valores de Memoria y Scheduler
  config_cpu->ip_memory = config_get_string_value(config_raw, "IP_MEMORY");
  config_cpu->puerto_memory =
      config_get_string_value(config_raw, "PUERTO_MEMORY");

  config_cpu->ip_scheduler =
      config_get_string_value(config_raw, "IP_SCHEDULER");
  config_cpu->puerto_scheduler =
      config_get_string_value(config_raw, "PUERTO_SCHEDULER");

  // 3. Extraemos los valores del Memory Stick
  config_cpu->ip_memory_stick = config_get_string_value(config_raw, "IP_STICK");
  config_cpu->puerto_memory_stick =
      config_get_string_value(config_raw, "PUERTO_STICK");

  // 4. Manejo del Log Level
  char *level_str = config_get_string_value(config_raw, "LOG_LEVEL");
  config_cpu->log_level = log_level_from_string(level_str);

  log_info(logger, "## Configuracion Cargada correctamente...");

  return true;
}
// CONECTAR LOS MODULOS....
int conectar_a_modulo(char *nombre, char *ip, char *puerto, t_log *logger) {
  int conexion = crear_conexion(ip, puerto);
  if (conexion != -1) {
    log_info(logger, "## Conectado a %s", nombre);
  } else {
    log_error(logger, "Error al conectar a %s en %s:%s", nombre, ip, puerto);
  }
  return conexion;
}