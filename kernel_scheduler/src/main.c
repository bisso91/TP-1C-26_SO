#include <commons/config.h>
#include <commons/log.h>
#include <commons/string.h>
#include <stdio.h>
#include <stdlib.h>
#include <utils/hello.h>
#include <utils/utils.h>
#include <commons/collections/queue.h>
#include <pthread.h> // manejo de semaforos
#include <scheduler-funciones.h>

// instancias de variables globales
char *algoritmo_de_planificacion;
int quantum;
t_dictionary *recursos_sistema;
extern sem_t sem_cpu_libre;

int socket_cpu_dispatch = -1;
int socket_cpu_interrupt = -1;
int socket_kernel_memory = -1;
int active_stdin_pid = -1;
int active_stdout_pid = -1;

pthread_mutex_t mutex_io_pending;
t_dictionary *io_pending_addresses;

void *atender_cliente(void *arg);
void crear_proceso_inicial(char *path_proceso);

int main(int argc, char *argv[]) {
  saludar("kernel_scheduler");

  if (argc < 3) {
    printf("ERROR: Faltan argumentos. Uso: %s [Config] [Path Proceso Inicial]\n", argv[0]);
    return 1;
  }

  char *config_path = argv[1];
  char *path_proceso_inicial = argv[2];

  //===================================================//
  //     INICIALIZACIÓN DE COLAS / MUTEX / SEMAFOROS    //
  //===================================================//
  cola_new = queue_create();
  cola_ready = queue_create();
  cola_block = queue_create();
  cola_exit = queue_create();
  interfaces_io = dictionary_create();
  io_pending_addresses = dictionary_create();

  prioridades_procesos = dictionary_create();
  pthread_mutex_init(&mutex_prioridades, NULL);
  nombres_recursos_global = list_create();

  pthread_mutex_init(&mutex_new, NULL);
  pthread_mutex_init(&mutex_ready, NULL);
  pthread_mutex_init(&mutex_block, NULL);
  pthread_mutex_init(&mutex_exit, NULL);
  pthread_mutex_init(&mutex_interfaces_io, NULL);
  pthread_mutex_init(&mutex_io_pending, NULL);

  sem_init(&sem_grado_multiprogramacion, 0, 8);
  sem_init(&sem_procesos_en_ready, 0, 0);
  sem_init(&sem_procesos_en_new, 0, 0);
  sem_init(&sem_cpu_libre, 0, 1);

  //===================================================//
  //     CONFIGURACION DEL PLANIFICADOR COMO SERVER/CLIENT //
  //===================================================//
  logger_server = log_create("kernel_scheduler.log", "KERNEL_SCHEDULER", true, LOG_LEVEL_INFO);
  if (logger_server == NULL) {
    printf("ERROR: No se pudo crear el logger\n");
    exit(1);
  }

  t_config *config_server = config_create(config_path);
  if (config_server == NULL) {
    log_error(logger_server, "No se pudo encontrar el archivo kernel_scheduler.config");
    return 1;
  }

  // --- lectura de archivo de config para RR y recursos --- //
  algoritmo_de_planificacion = config_get_string_value(config_server, "PLANIFICATION_ALGORITHM");
  if (algoritmo_de_planificacion == NULL) {
    algoritmo_de_planificacion = config_get_string_value(config_server, "ALGORITMO_PLANIFICIACION");
  }
  if (algoritmo_de_planificacion == NULL) {
    algoritmo_de_planificacion = "FIFO";
  }

  if (strcmp(algoritmo_de_planificacion, "CMN") == 0) {
    if (config_has_property(config_server, "QUEUES_ALGORITHMS")) {
      queues_algorithms = config_get_array_value(config_server, "QUEUES_ALGORITHMS");
      int cant = 0;
      while (queues_algorithms[cant] != NULL) {
        cant++;
      }
      cant_colas_multinivel = cant;
    } else {
      log_error(logger_server, "Falta la propiedad QUEUES_ALGORITHMS en config para CMN");
      return 1;
    }
    
    if (config_has_property(config_server, "QUEUE_PREEMPTION")) {
      char *preempt_str = config_get_string_value(config_server, "QUEUE_PREEMPTION");
      if (preempt_str != NULL && (strcmp(preempt_str, "TRUE") == 0 || strcmp(preempt_str, "true") == 0)) {
        queue_preemption = true;
      } else {
        queue_preemption = false;
      }
    } else {
      queue_preemption = false;
    }

    colas_multinivel = malloc(sizeof(t_queue*) * cant_colas_multinivel);
    for (int i = 0; i < cant_colas_multinivel; i++) {
      colas_multinivel[i] = queue_create();
    }
  }
  
  if (config_has_property(config_server, "RR_QUANTUM")) {
    quantum = config_get_int_value(config_server, "RR_QUANTUM");
  } else if (config_has_property(config_server, "QUANTUM")) {
    quantum = config_get_int_value(config_server, "QUANTUM");
  } else {
    quantum = 1000;
  }

  // Recursos de configuración (si existen)
  if (config_has_property(config_server, "RECURSOS") && config_has_property(config_server, "INSTANCIAS_RECURSOS")) {
    char** nombres_recursos = config_get_array_value(config_server, "RECURSOS");
    char** instancias_recursos = config_get_array_value(config_server, "INSTANCIAS_RECURSOS");
    inicializar_recursos(nombres_recursos, instancias_recursos);
    string_array_destroy(nombres_recursos);
    string_array_destroy(instancias_recursos);
  } else {
    recursos_sistema = dictionary_create();
  }

  // --- CONECTAR A KERNEL MEMORY --- //
  char *ip_memory = config_get_string_value(config_server, "IP_MEMORIA");
  char *puerto_memory = config_get_string_value(config_server, "PUERTO_SERVIDOR");
  if (puerto_memory == NULL) {
     puerto_memory = "8002"; // default Kernel Memory port
  }

  log_info(logger_server, "Conectando a Kernel Memory en %s:%s...", ip_memory, puerto_memory);
  int connection_km = crear_conexion(ip_memory, puerto_memory);
  if (connection_km != -1) {
    log_info(logger_server, "## Conectado a Kernel Memory");
    socket_kernel_memory = connection_km;
  } else {
    log_error(logger_server, "No se pudo conectar a Kernel Memory. Abortando.");
    return 1;
  }

  // --- CREAR PROCESO INICIAL --- //
  crear_proceso_inicial(path_proceso_inicial);

  // --- INICIALIZACIÓN DE HILOS --- //
  pthread_t hilo_plp;
  pthread_create(&hilo_plp, NULL, planificador_largo_plazo, NULL);
  pthread_detach(hilo_plp);

  pthread_t hilo_pcp;
  if (strcmp(algoritmo_de_planificacion, "FIFO") == 0){
    pthread_create(&hilo_pcp, NULL, planificador_corto_plazo_fifo, NULL);
  } else if (strcmp(algoritmo_de_planificacion, "RR") == 0){ 
    pthread_create(&hilo_pcp, NULL, planificador_corto_plazo_rr, NULL);
  } else if (strcmp(algoritmo_de_planificacion, "CMN") == 0){
    pthread_create(&hilo_pcp, NULL, planificador_corto_plazo_cmn, NULL);
  } else {
    log_error(logger_server, "Algoritmo de planificación desconocido: %s", algoritmo_de_planificacion);
    return 1;
  }
  pthread_detach(hilo_pcp);

  // --- INICIAR SERVIDOR SCHEDULER --- //
  char *ip_server = "0.0.0.0"; // listen on all interfaces
  char *puerto_server = config_get_string_value(config_server, "PUERTO_ESCUCHA");

  int server_fd = iniciar_servidor(ip_server, puerto_server);
  log_info(logger_server, "Kernel Scheduler iniciado en %s:%s. Esperando conexiones...", ip_server, puerto_server);

  while (1) {
    int cliente_fd = esperar_cliente(server_fd);
    if (cliente_fd != -1) {
      log_info(logger_server, "## Nuevo Cliente Conectado - FD del socket: %d", cliente_fd);

      int *fd_ptr = malloc(sizeof(int));
      *fd_ptr = cliente_fd;

      pthread_t hilo_cliente;
      pthread_create(&hilo_cliente, NULL, atender_cliente, fd_ptr);
      pthread_detach(hilo_cliente);
    }
  }

  config_destroy(config_server);
  log_destroy(logger_server);
  return 0;
}

void crear_proceso_inicial(char *path_proceso) {
    int nuevo_pid = generador_pid++;
    log_info(logger_server, "Creando proceso inicial PID %d con script %s", nuevo_pid, path_proceso);

    if (socket_kernel_memory != -1) {
      char *mensaje_km = string_from_format("%d %s", nuevo_pid, path_proceso);
      enviar_string(mensaje_km, socket_kernel_memory, INICIAR_PROCESO);
      free(mensaje_km);

      int ok;
      recv(socket_kernel_memory, &ok, sizeof(int), MSG_WAITALL);
    }

    t_pcb *nuevo_pcb = malloc(sizeof(t_pcb));
    nuevo_pcb->pid = nuevo_pid;
    nuevo_pcb->program_counter = 0;
    nuevo_pcb->estado = ESTADO_NEW;
    nuevo_pcb->cantidad_segmentos = 0;
    nuevo_pcb->tabla_segmentos = NULL;
    nuevo_pcb->prioridad_actual = 0;
    nuevo_pcb->prioridad_original = 0;

    registrar_prioridad(nuevo_pid, 0);

    pthread_mutex_lock(&mutex_new);
    queue_push(cola_new, nuevo_pcb);
    pthread_mutex_unlock(&mutex_new);

    log_info(logger_server, "## (%d) Se crea el proceso - Estado: NEW", nuevo_pcb->pid);
    sem_post(&sem_procesos_en_new);
}

void *atender_cliente(void *arg){
  int cliente_fd = *(int*) arg;
  free(arg);

  while(1){
    int cod_op = recibir_operacion(cliente_fd);

    if (cod_op <= 0){
      log_warning(logger_server, "El cliente con FD %d se desconectó.", cliente_fd);
      break;
    }

    switch (cod_op){
    case MENSAJE:
      recibir_mensaje(cliente_fd, logger_server);
      break;

    case IDENTIFICACION_IO: {
      char *nombre_io = recibir_string(cliente_fd);
      int *socket_io = malloc(sizeof(int));
      *socket_io = cliente_fd;

      pthread_mutex_lock(&mutex_interfaces_io);
      dictionary_put(interfaces_io, nombre_io, socket_io);
      pthread_mutex_unlock(&mutex_interfaces_io);

      log_info(logger_server, "## Interfaz IO registrada: %s en el FD %d", nombre_io, cliente_fd);
      break;
    }

    case IO_SLEEP: {
      t_pcb *pcb = recibir_pcb(cliente_fd);
      actualizar_prioridades_pcb(pcb);
      int milisegundos = recibir_entero(cliente_fd);

      log_info(logger_server, "## (%d) Solicitó syscall: SLEEP", pcb->pid);
      log_info(logger_server, "## (%d) Pasa del estado EXECUTE al estado BLOCKED", pcb->pid);

      pcb->estado = ESTADO_BLOCK;
      pthread_mutex_lock(&mutex_block);
      queue_push(cola_block, pcb);
      pthread_mutex_unlock(&mutex_block);

      sem_post(&sem_cpu_libre);

      pthread_mutex_lock(&mutex_interfaces_io);
      int *socket_io = dictionary_get(interfaces_io, "SLEEP");
      pthread_mutex_unlock(&mutex_interfaces_io);

      if (socket_io != NULL) {
        t_paquete *paquete_io = crear_paquete();
        paquete_io->cop = IO_SLEEP;
        int size_payload = sizeof(int) * 2;
        void *payload = malloc(size_payload);
        memcpy(payload, &(pcb->pid), sizeof(int));
        memcpy(payload + sizeof(int), &milisegundos, sizeof(int));
        agregar_a_paquete(paquete_io, payload, size_payload);
        enviar_paquete(paquete_io, *socket_io);
        free(payload);
        eliminar_paquete(paquete_io);
      } else {
        log_error(logger_server, "Interfaz SLEEP no conectada. Desbloqueando.");
        desbloquear_proceso_de_io(pcb);
      }
      break;
    }

    case IO_STDIN: {
      pthread_mutex_lock(&mutex_interfaces_io);
      int *socket_io_stdin = dictionary_get(interfaces_io, "IO_STDIN");
      pthread_mutex_unlock(&mutex_interfaces_io);

      if (socket_io_stdin != NULL && cliente_fd == *socket_io_stdin) {
        // Response from STDIN device!
        char *texto_leido = recibir_string(cliente_fd);
        log_info(logger_server, "STDIN recibio texto: '%s' para el PID %d", texto_leido, active_stdin_pid);
        free(texto_leido);

        // MOCK write to memory
        if (socket_kernel_memory != -1) {
          int cop = ESCRIBIR_MEMORIA;
          send(socket_kernel_memory, &cop, sizeof(int), 0);
          int ok;
          recv(socket_kernel_memory, &ok, sizeof(int), MSG_WAITALL);
        }

        t_pcb *pcb_a_desbloquear = sacar_de_cola_block(active_stdin_pid);
        if(pcb_a_desbloquear != NULL){
          log_info(logger_server, "## (%d) finalizó IO y pasa a READY", pcb_a_desbloquear->pid);
          encolar_proceso_ready(pcb_a_desbloquear);
        }
        active_stdin_pid = -1;
      } else {
        // Request from CPU!
        t_pcb *pcb = recibir_pcb(cliente_fd);
        actualizar_prioridades_pcb(pcb);
        int direccion_logica = recibir_entero(cliente_fd);
        int size_to_read = recibir_entero(cliente_fd);

        log_info(logger_server, "## (%d) Solicitó syscall: STDIN", pcb->pid);
        log_info(logger_server, "## (%d) Pasa del estado EXECUTE al estado BLOCKED", pcb->pid);

        pcb->estado = ESTADO_BLOCK;
        pthread_mutex_lock(&mutex_block);
        queue_push(cola_block, pcb);
        pthread_mutex_unlock(&mutex_block);

        sem_post(&sem_cpu_libre);

        if (socket_io_stdin != NULL) {
          active_stdin_pid = pcb->pid;
          t_paquete *paquete_io = crear_paquete();
          paquete_io->cop = IO_STDIN;
          int size_payload = sizeof(int) * 2;
          void *payload = malloc(size_payload);
          memcpy(payload, &(pcb->pid), sizeof(int));
          memcpy(payload + sizeof(int), &size_to_read, sizeof(int));
          agregar_a_paquete(paquete_io, payload, size_payload);
          enviar_paquete(paquete_io, *socket_io_stdin);
          free(payload);
          eliminar_paquete(paquete_io);
        } else {
          log_error(logger_server, "Interfaz STDIN no conectada. Desbloqueando.");
          desbloquear_proceso_de_io(pcb);
        }
      }
      break;
    }

    case IO_STDOUT: {
      t_pcb *pcb = recibir_pcb(cliente_fd);
      actualizar_prioridades_pcb(pcb);
      int direccion_logica = recibir_entero(cliente_fd);
      int size_to_read = recibir_entero(cliente_fd);

      log_info(logger_server, "## (%d) Solicitó syscall: STDOUT", pcb->pid);
      log_info(logger_server, "## (%d) Pasa del estado EXECUTE al estado BLOCKED", pcb->pid);

      pcb->estado = ESTADO_BLOCK;
      pthread_mutex_lock(&mutex_block);
      queue_push(cola_block, pcb);
      pthread_mutex_unlock(&mutex_block);

      sem_post(&sem_cpu_libre);

      pthread_mutex_lock(&mutex_interfaces_io);
      int *socket_io = dictionary_get(interfaces_io, "IO_STDOUT");
      pthread_mutex_unlock(&mutex_interfaces_io);

      if (socket_io != NULL) {
        char *texto_mock = "Contenido de memoria mockeado";
        int size_of_text = strlen(texto_mock) + 1;
        t_paquete *paquete_io = crear_paquete();
        paquete_io->cop = IO_STDOUT;
        int size_payload = sizeof(int) * 2 + size_of_text;
        void *payload = malloc(size_payload);
        memcpy(payload, &(pcb->pid), sizeof(int));
        memcpy(payload + sizeof(int), &size_of_text, sizeof(int));
        memcpy(payload + sizeof(int) * 2, texto_mock, size_of_text);
        agregar_a_paquete(paquete_io, payload, size_payload);
        enviar_paquete(paquete_io, *socket_io);
        free(payload);
        eliminar_paquete(paquete_io);
      } else {
        log_error(logger_server, "Interfaz STDOUT no conectada. Desbloqueando.");
        desbloquear_proceso_de_io(pcb);
      }
      break;
    }

    case FIN_IO:{
      int pid_terminado = recibir_entero(cliente_fd);
      t_pcb *pcb_a_desbloquear = sacar_de_cola_block(pid_terminado);

      if(pcb_a_desbloquear != NULL){
        log_info(logger_server, "## (%d) finalizó IO y pasa a READY", pcb_a_desbloquear->pid);
        encolar_proceso_ready(pcb_a_desbloquear);
      } else {
        log_error(logger_server, "Se intentó debloquear PID %d pero no estaba en BLOCK", pid_terminado);
      }
      break;
    }

    case WAIT_RECURSO: {
      t_pcb *pcb = recibir_pcb(cliente_fd);
      actualizar_prioridades_pcb(pcb);
      int cop_string = recibir_operacion(cliente_fd);
      char *nombre_recurso = recibir_string(cliente_fd);
      solicitar_recurso_wait(pcb, nombre_recurso, cliente_fd);
      free(nombre_recurso);
      break;
    }
    case SIGNAL_RECURSO: {
      t_pcb *pcb = recibir_pcb(cliente_fd);
      actualizar_prioridades_pcb(pcb);
      int cop_string = recibir_operacion(cliente_fd);
      char *nombre_recurso = recibir_string(cliente_fd);
      liberar_recurso_signal(pcb, nombre_recurso, cliente_fd);
      free(nombre_recurso);
      break; 
    }
    case MEM_ALLOC: {
      t_pcb *pcb = recibir_pcb(cliente_fd);
      actualizar_prioridades_pcb(pcb);
      int id_segmento = recibir_entero(cliente_fd);
      int tamanio = recibir_entero(cliente_fd);
      
      log_info(logger_server, "## (%d) - Solicitó syscall: MEM_ALLOC - ID: %d, Tamaño: %d", pcb->pid, id_segmento, tamanio);
      
      // Enviar solicitud a Kernel Memory
      int cop = MEM_ALLOC;
      send(socket_kernel_memory, &cop, sizeof(int), 0);
      enviar_entero(socket_kernel_memory, pcb->pid);
      enviar_entero(socket_kernel_memory, id_segmento);
      enviar_entero(socket_kernel_memory, tamanio);
      
      // Recibir respuesta de Kernel Memory
      int response_km;
      recv(socket_kernel_memory, &response_km, sizeof(int), MSG_WAITALL);
      
      if (response_km == INICIAR_COMPACTACION) {
        log_info(logger_server, "## Inicio de compactación");
        compactacion_activa = true;
        
        // Desalojar CPUs. Como por ahora tenemos una sola CPU conectada en dispatch/interrupt,
        // le enviamos interrupción de desalojo al socket de interrupción.
        if (socket_cpu_interrupt != -1) {
            int cop_int = INTERRUPCION_DESALOJO;
            send(socket_cpu_interrupt, &cop_int, sizeof(int), 0);
            enviar_entero(socket_cpu_interrupt, pcb->pid); // El PID de la CPU actualmente corriendo
            
            // Esperar que la CPU devuelva el PCB desalojado
            // Al hacer esto de forma síncrona en este hilo, podemos pausar el flujo de la syscall
            // y procesar el desalojo cuando la CPU responda en el hilo correspondiente
        }
        
        // Confirmar desalojo a Kernel Memory
        int cop_conf = CONFIRMAR_DESALOJO;
        send(socket_kernel_memory, &cop_conf, sizeof(int), 0);
        
        // Esperar fin de compactación
        int status_comp;
        recv(socket_kernel_memory, &status_comp, sizeof(int), MSG_WAITALL);
        
        log_info(logger_server, "## Fin de compactación");
        compactacion_activa = false;
        response_km = status_comp;
      }
      
      if (response_km == 1) {
        // Actualizar la tabla de segmentos en el PCB
        int cant_seg;
        recv(socket_kernel_memory, &cant_seg, sizeof(int), MSG_WAITALL);
        pcb->cantidad_segmentos = cant_seg;
        free(pcb->tabla_segmentos);
        if (cant_seg > 0) {
            pcb->tabla_segmentos = malloc(sizeof(t_segmento) * cant_seg);
            for (int i = 0; i < cant_seg; i++) {
                recv(socket_kernel_memory, &(pcb->tabla_segmentos[i].id), sizeof(int), MSG_WAITALL);
                recv(socket_kernel_memory, &(pcb->tabla_segmentos[i].base), sizeof(int), MSG_WAITALL);
                recv(socket_kernel_memory, &(pcb->tabla_segmentos[i].limite), sizeof(int), MSG_WAITALL);
            }
        } else {
            pcb->tabla_segmentos = NULL;
        }
        
        // Syscalls de memoria vuelven a enviar el PCB al CPU
        encolar_proceso_ready(pcb);
      } else {
        log_error(logger_server, "Out of memory o error al crear segmento %d para PID %d", id_segmento, pcb->pid);
        finalizar_proceso(pcb, "OUT_OF_MEMORY");
      }
      sem_post(&sem_cpu_libre);
      break;
    }
    
    case MEM_FREE: {
      t_pcb *pcb = recibir_pcb(cliente_fd);
      actualizar_prioridades_pcb(pcb);
      int id_segmento = recibir_entero(cliente_fd);
      
      log_info(logger_server, "## (%d) - Solicitó syscall: MEM_FREE - ID: %d", pcb->pid, id_segmento);
      
      // Enviar solicitud a Kernel Memory
      int cop = MEM_FREE;
      send(socket_kernel_memory, &cop, sizeof(int), 0);
      enviar_entero(socket_kernel_memory, pcb->pid);
      enviar_entero(socket_kernel_memory, id_segmento);
      
      int response_km;
      recv(socket_kernel_memory, &response_km, sizeof(int), MSG_WAITALL);
      
      if (response_km == 1) {
        int cant_seg;
        recv(socket_kernel_memory, &cant_seg, sizeof(int), MSG_WAITALL);
        pcb->cantidad_segmentos = cant_seg;
        free(pcb->tabla_segmentos);
        if (cant_seg > 0) {
            pcb->tabla_segmentos = malloc(sizeof(t_segmento) * cant_seg);
            for (int i = 0; i < cant_seg; i++) {
                recv(socket_kernel_memory, &(pcb->tabla_segmentos[i].id), sizeof(int), MSG_WAITALL);
                recv(socket_kernel_memory, &(pcb->tabla_segmentos[i].base), sizeof(int), MSG_WAITALL);
                recv(socket_kernel_memory, &(pcb->tabla_segmentos[i].limite), sizeof(int), MSG_WAITALL);
            }
        } else {
            pcb->tabla_segmentos = NULL;
        }
        
        encolar_proceso_ready(pcb);
      } else {
        log_error(logger_server, "Error al liberar segmento %d para PID %d", id_segmento, pcb->pid);
        finalizar_proceso(pcb, "SEG_FAULT");
      }
      sem_post(&sem_cpu_libre);
      break;
    }

    case INTERRUPCION_DESALOJO: {
      t_pcb *pcb_desalojado = recibir_pcb(cliente_fd);
      actualizar_prioridades_pcb(pcb_desalojado);
      
      if (!compactacion_activa) {
        log_info(logger_server, "## (%d) Prioridad: %d - Desalojado por cola más prioritaria", pcb_desalojado->pid, pcb_desalojado->prioridad_actual);
      }
      encolar_proceso_ready(pcb_desalojado);
      sem_post(&sem_cpu_libre);
      break;
    }
    
    case INIT_PROC: {
      t_pcb *pcb_creador = recibir_pcb(cliente_fd);
      actualizar_prioridades_pcb(pcb_creador);
      int cop_string = recibir_operacion(cliente_fd);
      char *path_proceso = recibir_string(cliente_fd);
      int prioridad = recibir_entero(cliente_fd);

      log_info(logger_server, "## (%d) Solicitó syscall: INIT_PROC para '%s'", pcb_creador->pid, path_proceso);

      int nuevo_pid = generador_pid++;
      
      if (socket_kernel_memory != -1) {
        char *mensaje_km = string_from_format("%d %s", nuevo_pid, path_proceso);
        enviar_string(mensaje_km, socket_kernel_memory, INICIAR_PROCESO);
        free(mensaje_km);

        int ok;
        recv(socket_kernel_memory, &ok, sizeof(int), MSG_WAITALL);
      }

      t_pcb *nuevo_pcb = malloc(sizeof(t_pcb));
      nuevo_pcb->pid = nuevo_pid;
      nuevo_pcb->program_counter = 0;
      nuevo_pcb->estado = ESTADO_NEW;
      nuevo_pcb->cantidad_segmentos = 0;
      nuevo_pcb->tabla_segmentos = NULL;
      nuevo_pcb->prioridad_actual = prioridad;
      nuevo_pcb->prioridad_original = prioridad;

      registrar_prioridad(nuevo_pid, prioridad);

      pthread_mutex_lock(&mutex_new);
      queue_push(cola_new, nuevo_pcb);
      pthread_mutex_unlock(&mutex_new);

      log_info(logger_server, "## (%d) Se crea el proceso - Estado: NEW", nuevo_pcb->pid);
      sem_post(&sem_procesos_en_new);

      encolar_proceso_ready(pcb_creador);
      sem_post(&sem_cpu_libre);

      free(path_proceso);
      break;
    }

    case FIN_QUANTUM: {
      t_pcb *pcb_desalojado = recibir_pcb(cliente_fd);
      actualizar_prioridades_pcb(pcb_desalojado);
      log_info(logger_server, "## (%d) - Desalojado por fin de quantum", pcb_desalojado->pid);

      encolar_proceso_ready(pcb_desalojado);
      sem_post(&sem_cpu_libre);
      break;
    }
    case FIN_PROCESO: {
      t_pcb *pcb_finalizado = recibir_pcb(cliente_fd);
      actualizar_prioridades_pcb(pcb_finalizado);
      finalizar_proceso(pcb_finalizado, "SUCCESS");
      break;
    }
    case SEGMENTATION_FAULT: {
      t_pcb *pcb_error = recibir_pcb(cliente_fd);
      actualizar_prioridades_pcb(pcb_error);
      finalizar_proceso(pcb_error, "SEG_FAULT");
      break;
    }
    case IDENTIFICACION_CPU_DISPATCH: {
      socket_cpu_dispatch = cliente_fd;
      log_info(logger_server, "CPU Dispatch conectada con FD %d", socket_cpu_dispatch);
      break;
    }
    case IDENTIFICACION_CPU_INTERRUPT: {
      socket_cpu_interrupt = cliente_fd;
      log_info(logger_server, "CPU Interrupt conectada con FD %d", socket_cpu_interrupt);
      break;
    }                
    default:
        log_warning(logger_server, "Operación no identificada del FD %d.", cliente_fd);
        break;
    }
  }
  close(cliente_fd);
  return NULL;
}
