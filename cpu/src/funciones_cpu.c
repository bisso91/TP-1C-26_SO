#include "funciones_cpu.h"
#include <stdbool.h>
#include <pthread.h>

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
int interrupt_op = 0;

t_list *cpu_sticks = NULL;
pthread_mutex_t mutex_cpu_sticks;

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

  cpu_sticks = list_create();
  pthread_mutex_init(&mutex_cpu_sticks, NULL);

  // 5-Conexiones
  // MEMORIA--->Aca soy cliente
  log_trace(logger_cpu, "Creando conexion a Memory...");
  fd_memory = crear_conexion(config_cpu.ip_memory, config_cpu.puerto_memory);
  if (fd_memory == -1) {
    log_error(logger_cpu, "No se pudo iniciar conexion con Memoria");
    exit(EXIT_FAILURE);
  }
  log_info(logger_cpu, "Conexion establecida con Memoria Central (FD: %d)",
           fd_memory);
  // No handshake is sent to Memory as Memory does not have handshake logic.

  // DISPATCH E INTERRUPT (Conexiones al Scheduler como cliente)
  log_info(logger_cpu, "Conectando a Scheduler Dispatch en %s:%s...", config_cpu.ip_scheduler, config_cpu.puerto_scheduler);
  fd_dispatch = crear_conexion(config_cpu.ip_scheduler, config_cpu.puerto_scheduler);
  if (fd_dispatch == -1) {
    log_error(logger_cpu, "No se pudo conectar a Scheduler Dispatch");
    exit(EXIT_FAILURE);
  }
  // Enviar identificación
  int cop_dispatch = IDENTIFICACION_CPU_DISPATCH;
  send(fd_dispatch, &cop_dispatch, sizeof(int), 0);
  log_info(logger_cpu, "Conexion establecida con Scheduler Dispatch (FD: %d)", fd_dispatch);

  log_info(logger_cpu, "Conectando a Scheduler Interrupt en %s:%s...", config_cpu.ip_scheduler, config_cpu.puerto_scheduler);
  fd_interrupt = crear_conexion(config_cpu.ip_scheduler, config_cpu.puerto_scheduler);
  if (fd_interrupt == -1) {
    log_error(logger_cpu, "No se pudo conectar a Scheduler Interrupt");
    exit(EXIT_FAILURE);
  }
  // Enviar identificación
  int cop_interrupt = IDENTIFICACION_CPU_INTERRUPT;
  send(fd_interrupt, &cop_interrupt, sizeof(int), 0);
  log_info(logger_cpu, "Conexion establecida con Scheduler Interrupt (FD: %d)", fd_interrupt);

  // Levantar hilo de interrupciones
  pthread_t thread_interrupt;
  extern void *interrupt_server(void *arg);
  pthread_create(&thread_interrupt, NULL, interrupt_server, NULL);
  pthread_detach(thread_interrupt);

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

// 1 Para recibir solo el número del código de operación // <-- Renombrada
int recibir_operacion_cpu(int socket_cliente, t_log *logger) {
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

void actualizar_sticks_desde_memoria() {
    pthread_mutex_lock(&mutex_cpu_sticks);
    
    int size = list_size(cpu_sticks);
    for (int i = 0; i < size; i++) {
        t_cpu_stick_conn *conn = list_get(cpu_sticks, i);
        if (conn->socket != -1) {
            close(conn->socket);
        }
        free(conn->ip);
        free(conn);
    }
    list_clean(cpu_sticks);
    
    int cop = OBTENER_STICKS;
    send(fd_memory, &cop, sizeof(int), 0);
    
    int op = recibir_operacion(fd_memory);
    if (op == OBTENER_STICKS) {
        int size_payload;
        void *stream = recibir_buffer(&size_payload, fd_memory);
        
        int desplazamiento = 0;
        int cant_sticks;
        memcpy(&cant_sticks, stream + desplazamiento, sizeof(int));
        desplazamiento += sizeof(int);
        
        for (int i = 0; i < cant_sticks; i++) {
            t_cpu_stick_conn *conn = malloc(sizeof(t_cpu_stick_conn));
            memcpy(&(conn->base), stream + desplazamiento, sizeof(int));
            desplazamiento += sizeof(int);
            memcpy(&(conn->limite), stream + desplazamiento, sizeof(int));
            desplazamiento += sizeof(int);
            
            int ip_len;
            memcpy(&ip_len, stream + desplazamiento, sizeof(int));
            desplazamiento += sizeof(int);
            
            conn->ip = malloc(ip_len);
            memcpy(conn->ip, stream + desplazamiento, ip_len);
            desplazamiento += ip_len;
            
            memcpy(&(conn->puerto), stream + desplazamiento, sizeof(int));
            desplazamiento += sizeof(int);
            
            conn->socket = -1;
            list_add(cpu_sticks, conn);
        }
        free(stream);
    }
    pthread_mutex_unlock(&mutex_cpu_sticks);
}

int obtener_conexion_stick(int dir_fisica) {
    pthread_mutex_lock(&mutex_cpu_sticks);
    int size = list_size(cpu_sticks);
    t_cpu_stick_conn *matching_stick = NULL;
    
    for (int i = 0; i < size; i++) {
        t_cpu_stick_conn *conn = list_get(cpu_sticks, i);
        if (dir_fisica >= conn->base && dir_fisica <= conn->limite) {
            matching_stick = conn;
            break;
        }
    }
    
    if (matching_stick == NULL) {
        pthread_mutex_unlock(&mutex_cpu_sticks);
        actualizar_sticks_desde_memoria();
        
        pthread_mutex_lock(&mutex_cpu_sticks);
        size = list_size(cpu_sticks);
        for (int i = 0; i < size; i++) {
            t_cpu_stick_conn *conn = list_get(cpu_sticks, i);
            if (dir_fisica >= conn->base && dir_fisica <= conn->limite) {
                matching_stick = conn;
                break;
            }
        }
    }
    
    if (matching_stick == NULL) {
        pthread_mutex_unlock(&mutex_cpu_sticks);
        return -1;
    }
    
    if (matching_stick->socket == -1) {
        char port_str[16];
        sprintf(port_str, "%d", matching_stick->puerto);
        matching_stick->socket = crear_conexion(matching_stick->ip, port_str);
        if (matching_stick->socket != -1) {
            int cop = IDENTIFICACION_CPU;
            send(matching_stick->socket, &cop, sizeof(int), 0);
            enviar_string("1", matching_stick->socket, IDENTIFICACION_CPU);
        }
    }
    
    int socket_to_return = matching_stick->socket;
    pthread_mutex_unlock(&mutex_cpu_sticks);
    return socket_to_return;
}

bool cpu_leer_memoria_segmentado(uint32_t dir_fisica, int tamanio, void *dest_buffer) {
    int bytes_leidos = 0;
    while (bytes_leidos < tamanio) {
        uint32_t curr_dir = dir_fisica + bytes_leidos;
        int rest = tamanio - bytes_leidos;
        
        pthread_mutex_lock(&mutex_cpu_sticks);
        t_cpu_stick_conn *matching_stick = NULL;
        int size_list = list_size(cpu_sticks);
        for (int i = 0; i < size_list; i++) {
            t_cpu_stick_conn *conn = list_get(cpu_sticks, i);
            if (curr_dir >= conn->base && curr_dir <= conn->limite) {
                matching_stick = conn;
                break;
            }
        }
        pthread_mutex_unlock(&mutex_cpu_sticks);
        
        if (matching_stick == NULL) {
            actualizar_sticks_desde_memoria();
            pthread_mutex_lock(&mutex_cpu_sticks);
            size_list = list_size(cpu_sticks);
            for (int i = 0; i < size_list; i++) {
                t_cpu_stick_conn *conn = list_get(cpu_sticks, i);
                if (curr_dir >= conn->base && curr_dir <= conn->limite) {
                    matching_stick = conn;
                    break;
                }
            }
            pthread_mutex_unlock(&mutex_cpu_sticks);
        }
        
        if (matching_stick == NULL) {
            log_error(logger_cpu, "Dirección física %u fuera de rango de los Memory Sticks", curr_dir);
            return false;
        }
        
        int limit_in_stick = matching_stick->limite - curr_dir + 1;
        int chunk_size = (rest < limit_in_stick) ? rest : limit_in_stick;
        
        int socket_stick = obtener_conexion_stick(curr_dir);
        if (socket_stick == -1) {
            return false;
        }
        
        int op = LECTURA_MEMORIA;
        send(socket_stick, &op, sizeof(int), 0);
        send(socket_stick, &curr_dir, sizeof(uint32_t), 0);
        send(socket_stick, &chunk_size, sizeof(uint32_t), 0);
        
        int r = recv(socket_stick, dest_buffer + bytes_leidos, chunk_size, MSG_WAITALL);
        if (r != chunk_size) {
            return false;
        }
        bytes_leidos += chunk_size;
    }
    return true;
}

bool cpu_escribir_memoria_segmentado(uint32_t dir_fisica, int tamanio, void *src_buffer) {
    int bytes_escritos = 0;
    while (bytes_escritos < tamanio) {
        uint32_t curr_dir = dir_fisica + bytes_escritos;
        int rest = tamanio - bytes_escritos;
        
        pthread_mutex_lock(&mutex_cpu_sticks);
        t_cpu_stick_conn *matching_stick = NULL;
        int size_list = list_size(cpu_sticks);
        for (int i = 0; i < size_list; i++) {
            t_cpu_stick_conn *conn = list_get(cpu_sticks, i);
            if (curr_dir >= conn->base && curr_dir <= conn->limite) {
                matching_stick = conn;
                break;
            }
        }
        pthread_mutex_unlock(&mutex_cpu_sticks);
        
        if (matching_stick == NULL) {
            actualizar_sticks_desde_memoria();
            pthread_mutex_lock(&mutex_cpu_sticks);
            size_list = list_size(cpu_sticks);
            for (int i = 0; i < size_list; i++) {
                t_cpu_stick_conn *conn = list_get(cpu_sticks, i);
                if (curr_dir >= conn->base && curr_dir <= conn->limite) {
                    matching_stick = conn;
                    break;
                }
            }
            pthread_mutex_unlock(&mutex_cpu_sticks);
        }
        
        if (matching_stick == NULL) {
            log_error(logger_cpu, "Dirección física %u fuera de rango de los Memory Sticks", curr_dir);
            return false;
        }
        
        int limit_in_stick = matching_stick->limite - curr_dir + 1;
        int chunk_size = (rest < limit_in_stick) ? rest : limit_in_stick;
        
        int socket_stick = obtener_conexion_stick(curr_dir);
        if (socket_stick == -1) {
            return false;
        }
        
        int op = ESCRITURA_MEMORIA;
        send(socket_stick, &op, sizeof(int), 0);
        send(socket_stick, &curr_dir, sizeof(uint32_t), 0);
        send(socket_stick, &chunk_size, sizeof(uint32_t), 0);
        send(socket_stick, src_buffer + bytes_escritos, chunk_size, 0);
        
        int resultado;
        int r = recv(socket_stick, &resultado, sizeof(int), MSG_WAITALL);
        if (r != sizeof(int) || resultado != 1) {
            return false;
        }
        bytes_escritos += chunk_size;
    }
    return true;
}

