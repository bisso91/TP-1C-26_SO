#include <commons/config.h>
#include <commons/log.h>
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
t_dictionary recursos_sistema;

int socket_cpu_dispatch = -1;
int socket_cpu_interrupt = -1;

void *atender_cliente(void *arg);

int main(int argc, char *argv[]) {
  saludar("kernel_scheduler");

  //===================================================//
  //     CONFIGURACION DEL PLANIFICADOR COMO CLIENTE   //
  //===================================================//
  t_log *logger_cliente = log_create("scheduler.log", "KERNEL_SCHEDULER", true, LOG_LEVEL_INFO);

  if (logger_cliente == NULL) {
    printf("Error al crear el logger de cliente\n");
    return 1;
  }

  t_config *config_cliente = config_create("kernel_scheduler.config");
  if (config_cliente == NULL) {
    log_error(logger_cliente,
              "No se pudo encontrar el arhcivo scheduler.config");
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
  //===================================================//

  //===================================================//
  //     CONFIGURACION DEL PLANIFICADOR COMO SERVER    //
  //===================================================//
  logger_server = log_create("kernel_scheduler.log", "KERNEL_SCHEDULER",true, LOG_LEVEL_INFO);
  if (logger_server == NULL) {
    printf("ERROR: No se pudo crear el logger\n");
    exit(1);
  }   
  // -------------------------------------------------- //

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
    log_info(logger_server, "## Nuevo Cliente Conectado - FD del socket: %d", cliente_fd);

    int *fd_ptr = malloc(sizeof(int));
    *fd_ptr = cliente_fd;

    pthread_t hilo_cliente;
    pthread_create(&hilo_cliente, NULL, atender_cliente, fd_ptr);
    pthread_detach(hilo_cliente);
    
  }

  // --- lectura de archivo de config para RR y recursos --- //
  algoritmo_de_planificacion = config_get_string_value(config_server, "ALGORITMO_PLANIFICIACION");
  
  if(config_has_property(config_server, "QUANTUM")){
    quantum = config_get_int_value(config_server, "QUANTUM");
  }

  char* *nombres_recursos = config_get_array_value(config_server, "RECURSOS");
  char* *instancias_recursos = config_get_array_value(config_server, "INSTANCIAS_RECURSOS");

  // inicialización de diccionario de mutex
  inicializar_recursos(nombres_recursos, instancias_recursos);

  // libero memoria de los recursos
  string_array_destroy(nombres_recursos);
  string_array_destroy(instancias_recursos);
  // ------------------------------------------------------ //

  // --- INICIALIZACIÓN DE COLAS / MUTEX / SEMAFOROS --- //
              // --- inicialización de colas --- //
  cola_new = queue_create();
  cola_ready = queue_create();
  cola_block = queue_create();
  cola_exit = queue_create();
  interfaces_io = dictionary_create();
              // ------------------------------- //

              // --- inicialización de mutex --- //
  pthread_mutex_init(&mutex_new, NULL);
  pthread_mutex_init(&mutex_ready, NULL);
  pthread_mutex_init(&mutex_block, NULL);
  pthread_mutex_init(&mutex_exit, NULL);
              // ------------------------------- //
              // --- semaforos --- //
  sem_init(&sem_grado_multiprogramacion, 0, 3);
  sem_init(&sem_procesos_en_ready, 0, 0);
  sem_init(&sem_procesos_en_new, 0, 0);

              // --- inicialización de hilos --- //
  pthread_t hilo_plp;
  pthread_create(&hilo_plp, NULL, planificador_largo_plazo, NULL);
  pthread_detach(hilo_plp);


  pthread_t hilo_pcp;
  pthread_create(&hilo_pcp, NULL, planificador_corto_plazo_fifo, NULL);
  pthread_detach(hilo_pcp);
  
  // -------------------------------------------------- // 

  // --- Elección del hilo de corto plazo (PCP) --- //
  pthread_t hilo_pcp;
  if (strcmp(algoritmo_de_planificacion, "FIFO") == 0){
    pthread_create(&hilo_pcp, NULL, planificador_corto_plazo_fifo, NULL);
  } else if (strcmp(algoritmo_de_planificacion, "RR") == 0){ 
    pthread_create(&hilo_pcp, NULL, planificador_corto_plazo_rr, NULL);
  } else {
    log_error(logger_server, "Algoritmo desconocido");
    return 1;
  }
  pthread_detach(hilo_pcp);  

  // libero memoria
  config_destroy(config_server);
  log_destroy(logger_server);
  return 0;
}

void *atender_cliente(void *arg){
  int cliente_fd = *(int*) arg; // q p*nga es ese puntero???
  free(arg);

  while(1){
    int cod_op = recibir_operacion(cliente_fd);

    if (cod_op <= 0){
      log_warning(logger_server, "El cliente con FD %d se desconectó.", cliente_fd);
      // ver si agregar logica para identificar quien tiro la conexion
      break;
    }

    switch (cod_op){
    case MENSAJE:
      recibir_mensaje(cliente_fd, logger_server);
      break;
    case IO_GENERICA: {
      t_pcb *pcb_recibido = recibir_pcb(cliente_fd);
      char *nombre_interfaz = recibir_mensaje(cliente_fd, logger_server);

      log_info(logger_server, "## (PID %d) Pasa de EXEC a BLOCKED (Esperando a %s )", pcb_recibido->pid, nombre_interfaz);
      bloquear_proceso_por_io(pcb_recibido, nombre_interfaz);

      pthread_mutex_lock(&mutex_interfaces_io);
      int *socket_destino = dictionary_get(interfaces_io, nombre_interfaz);
      pthread_mutex_unlock(&mutex_interfaces_io);

      if(socket_destino != NULL){
        //enviar_peticion_io(...);
      } else {
        log_error(logger_server, "Interfaz %s no conectada.", nombre_interfaz);
      }
      free(nombre_interfaz);
      break;
    }
    case FIN_IO:{
              // recibo el PID del proceso que terminó
              int pid_terminado = recibir_entero(cliente_fd);

              // lo saco de la cola de BLOCKED
              t_pcb *pcb_a_desbloquear = sacar_de_cola_block(pid_terminado);

              if(pcb_a_desbloquear != NULL){
                log_info(logger_server, "## (PID: %d) Desbloqueado. Pasando a READY", pcb_a_desbloquear->pid);

                // cambio el estado
                pcb_a_desbloquear->estado = ESTADO_READY;

                // lo paso a la cola de READY
                pthread_mutex_lock(&mutex_ready);
                queue_push(cola_ready, pcb_a_desbloquear);
                pthread_mutex_unlock(&mutex_ready);

                // notifico al planificador de corto plazo
                sem_post(&sem_procesos_en_ready);
                } else {
                  log_error(logger_server, "Se intentó debloquear PID %d pero no estaba en BLOCK", pid_terminado);
                }
                break;
            }
            /*  
            case IDENTIFICACION_IO:
                //char *nombre_io = recibir_mensaje(cliente_fd, logger_server);
                char *nombre_io = "SLEEP"; //harcodeo temporal

                int *socket_io = malloc(sizeof(int));
                *socket_io = cliente_fd;

                pthread_mutex_lock(&mutex_interfaces_io);
                dictionary_put(interfaces_io, nombre_io, socket_io);
                pthread_mutex_unlock(&mutex_interfaces_io);

                log_info(logger_server, "Interfaz IO registrada: %s en el FD %d", nombre_io, cliente_fd);
                */
    // --- CASOS PARA RECURSOS --- //
    case WAIT_RECURSO: {
      t_pcb *pcb = recibir_pcb(cliente_fd);
      char *nombre_recurso = recibir_mensaje(cliente_fd, logger_server);
      solicitar_recurso_wait(pcb, nombre_recurso, cliente_fd);
      free(nombre_recurso);
      break;
    }
    case SIGNAL_RECURSO: {
      t_pcb *pcb = recibir_pcb(cliente_fd);
      char *nombre_recurso = recibir_mensaje(cliente_fd, logger_server);
      liberar_recurso_signal(pcb, nombre_recurso, cliente_fd);
      free(nombre_recurso);
      break; 
    }
    // ---------------------------- //
    // --- DESALOJO POR RR --- //
    case FIN_QUANTUM: {
      t_pcb *pcb_desalojado = recibir_pcb(cliente_fd);
      log_info(logger_server, "## (PID %d) Desalojado por fin de Quantum. Pasando a READY.", pcb_desalojado->pid);

      pcb_desalojado->estado = ESTADO_READY;

      pthread_mutex_lock(&mutex_ready);
      queue_push(cola_ready, pcb_desalojado);
      pthread_mutex_unlock(&mutex_ready);

      sem_post(&sem_procesos_en_ready);
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
