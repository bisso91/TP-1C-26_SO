#include <commons/config.h>
#include <commons/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <utils/hello.h>
#include <utils/utils.h>
#include <commons/collections/queue.h>
#include <pthread.h> // manejo de semaforos
#include <scheduler-funciones.h>

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
  t_log *logger_server = log_create("kernel_scheduler.log", "KERNEL_SCHEDULER",
                                    true, LOG_LEVEL_INFO);
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
    
    // --- EJEMPLO: LA CPU NOS DEVUELVE UN PROCESO QUE PIDIÓ SLEEP ---
            case IO_SLEEP: {
              // 1. Recibiríamos el PCB actualizado y el tiempo de sleep de la CPU
                //t_pcb* pcb_recibido = recibir_pcb(cliente_fd);
                //int tiempo = recibir_entero(cliente_fd);

                // 2. Usamos nuestra nueva función para bloquearlo
                //bloquear_proceso_por_io(pcb_recibido, "SLEEP");

                // 3. Le mandamos la orden de trabajo al socket del módulo IO correspondiente
                // enviar_orden_io_sleep(socket_io, pcb_recibido->pid, tiempo);
                //pthread_mutex_lock(&mutex_interfaces_io);
                //int *socket_destino  = dictionary_get(interfaces_io, "SLEEP");
                //pthread_mutex_unlock(&mutex_interfaces_io);

                // if (socket_destino != NULL){
                //  //enviar_orden_io_sleep(*socket_destino, pcb_recibido->pid, tiempo);
                //  log_info(logger_server, "Orden de ");
                //}

                log_info(logger_server, "Recibí una petición de IO_SLEEP desde la CPU");

                t_pcb *pcb_recibido = recibir_pcb(cliente_fd);

                bloquear_proceso_por_io(pcb_recibido, "SLEEP");
                
                break;

              }
            // --- EJEMPLO: LA IO NOS AVISA QUE TERMINÓ SU TRABAJO ---
            case FIN_IO:
                // 1. Recibiríamos el PID del proceso que terminó su IO
                // int pid_terminado = recibir_entero(cliente_fd);

                // 2. Lo buscamos en la cola_block y lo sacamos
                // t_pcb* pcb_a_despertar = sacar_de_cola_block(pid_terminado);

                // 3. Usamos nuestra nueva función para devolverlo a READY
                // desbloquear_proceso_de_io(pcb_a_despertar);
                break;

            case IDENTIFICACION_IO:
                //char *nombre_io = recibir_mensaje(cliente_fd, logger_server);
                char *nombre_io = "SLEEP"; //harcodeo temporal

                int *socket_io = malloc(sizeof(int));
                *socket_io = cliente_fd;

                pthread_mutex_lock(&mutex_interfaces_io);
                dictionary_put(interfaces_io, nombre_io, socket_io);
                pthread_mutex_unlock(&mutex_interfaces_io);

                log_info(logger_server, "Interfaz IO registrada: %s en el FD %d", nombre_io, cliente_fd);

            default:
                log_warning(logger_server, "Operación no identificada del FD %d.", cliente_fd);
                break;
        }
  }

  close(cliente_fd);
  return NULL;
}
