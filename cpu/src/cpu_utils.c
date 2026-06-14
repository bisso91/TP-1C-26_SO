#include "cpu_utils.h"
#include <stdbool.h>

t_log *logger_cpu = NULL;
t_config *config_plana = NULL;
t_config_cpu config_cpu;
t_registros registros;

int fd_memory = -1;
int fd_scheduler = -1;
int fd_dispatch = -1;
int fd_interrupt = -1;
int kernel_interrupt_fd = -1;
uint32_t interrupted_pid = 0;

// UNIFICO TODA LA PRIMERA PARTE EN FUNCION...
void inicializar_cpu(char *config_path, char *id_cpu) {

  // 1-Logger
  char nombre_logger[20];
  sprintf(nombre_logger, "CPU_%s", id_cpu);
  logger_cpu = log_create("cpu.log", nombre_logger, true, LOG_LEVEL_INFO);

  // Validacion archivo de log
  if (logger_cpu == NULL) {
    printf("Error al crear el logger\n");
  } else {
    printf("Archivo de log creado correctamente\n");
  }

  // 2-Archivo_Config
  config_plana = config_create("cpu.config");

  // Validacion de archivo de config
  if (config_plana == NULL) {
    log_error(logger_cpu, "No se pudo encontrar el archivo cpu.config");
  } else {
    printf("Archivo de configuracion creado correctamente\n");
  }

  // 3-Cargo_Config

  // Aca cuando se hace el if, al evaluar ya ejecuta la funcion y me carga la
  // config
  if (!cargar_configuracion(&config_cpu, config_plana, logger_cpu)) {
    log_error(logger_cpu, "Error al cargar configuración");
  }

  // 4-Inicializo_registros
  if (!inicializar_registros(&registros, logger_cpu)) {
    log_error(logger_cpu, "Falla crítica al inicializar estructuras de CPU");
  }

  // 5-Conexiones
  // MEMORIA--->Aca soy cliente
  log_trace(logger_cpu, "Creando conexion a Memory_Scheduler...");
  fd_memory = crear_conexion(config_cpu.ip_memory, config_cpu.puerto_memory);
  if (fd_memory == -1) {
    log_error(logger_cpu, "No se pudo iniciar conexion con Memoria");
    exit(EXIT_FAILURE);
  }
  log_info(logger_cpu, "Conexion establecida con Memoria Central (FD: %d)",
           fd_memory);
  // Chiche, le mando un handshake
  uint8_t id_numerico = (uint8_t)atoi(id_cpu); // Aca standarizo el id
  // Si entendi bien con esto deberia andar
  ssize_t bytes_enviados =
      send(fd_memory, &id_numerico, sizeof(uint8_t), MSG_NOSIGNAL);
  if (bytes_enviados <= 0) {
    log_error(logger_cpu, "Fallo al enviar el Handshake (ID: %d) a Memoria",
              id_numerico);
    exit(EXIT_FAILURE);
  } else {
    log_trace(logger_cpu,
              "Handshake enviado exitosamente a Memoria (ID de CPU: %d)",
              id_numerico);
  }
  // DISPATCH E INTERRUPT

  fd_dispatch =
      iniciar_servidor(config_cpu.puerto_escucha_dispatch, logger_cpu);
  fd_interrupt =
      iniciar_servidor(config_cpu.puerto_escucha_interrupt, logger_cpu);

  log_info(logger_cpu, "### CPU Iniciada con ID: %s", id_cpu);

  return;
}

void finalizar_cpu() {
  log_trace(logger_cpu, "Terminando programa...");
  log_destroy(logger_cpu);
  config_destroy(config_plana);
  close(fd_interrupt);
  close(fd_dispatch);
  close(fd_memory);
  close(fd_scheduler);
}

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
      !config_has_property(config_raw, "LOG_LEVEL") ||
      !config_has_property(config_raw, "PUERTO_ESCUCHA_DISPATCH") ||
      !config_has_property(config_raw, "PUERTO_ESCUCHA_INTERRUPT")) {
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

  // 3. Manejo del Log Level
  char *level_str = config_get_string_value(config_raw, "LOG_LEVEL");
  config_cpu->log_level = log_level_from_string(level_str);

  // 4. Cargo los puertos de escucha del CPU
  config_cpu->puerto_escucha_dispatch =
      config_get_string_value(config_raw, "PUERTO_ESCUCHA_DISPATCH");
  config_cpu->puerto_escucha_interrupt =
      config_get_string_value(config_raw, "PUERTO_ESCUCHA_INTERRUPT");

  log_info(logger, "## Configuracion Cargada correctamente...");

  return true;
}

/* CONECTAR LOS MODULOS....
int conectar_a_modulo(char *nombre, char *ip, char *puerto, t_log *logger) {
  int conexion = crear_conexion(ip, puerto);
  if (conexion != -1) {
    log_info(logger, "## Conectado a %s", nombre);
  } else {
    log_error(logger, "Error al conectar a %s en %s:%s", nombre, ip, puerto);
  }
  return conexion;
}
VIENDO SI SIGO USANDOLA*/

// FUNCIONES PARA RECIBIR DATOS...

// 1 Para recibir solo el número del código de operación
int recibir_operacion(int socket_cliente, t_log *logger) {
  int cod_op;

  // Intentamos recibir el código de operación
  ssize_t bytes_recibidos =
      recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL);

  if (bytes_recibidos > 0) {
    log_debug(logger, "Operación recibida: %d", cod_op);
    return cod_op;
  } else if (bytes_recibidos == 0) {
    // El cliente cerró la conexión de forma ordenada (graceful shutdown)
    log_warning(logger, "El cliente se desconectó (socket: %d)",
                socket_cliente);
    close(socket_cliente);
    return -1;
  } else {
    // Error real en el socket
    log_error(logger, "Error al recibir operación del socket %d: %s",
              socket_cliente, strerror(errno));
    close(socket_cliente);
    return -1;
  }
}

// 2 Para recibir el "cuerpo" del mensaje...si viene con datos
void *recibir_buffer(int *size, int socket_cliente) {
  void *buffer;
  recv(socket_cliente, size, sizeof(int), MSG_WAITALL);
  buffer = malloc(*size);
  recv(socket_cliente, buffer, *size, MSG_WAITALL);
  return buffer;
}