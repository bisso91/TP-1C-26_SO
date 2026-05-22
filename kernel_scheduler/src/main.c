#include <commons/config.h>
#include <commons/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <utils/hello.h>
#include <utils/utils.h>
#include <commons/collections/queue.h>
#include <pthread.h> // manejo de semaforos
#include <scheduler-funciones.h>

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
    log_info(logger_server, "## Nuevo Cliente Conectado - FD del socket: %d",
             cliente_fd);
    //===================================================
    //---------PRUEBA DE MENSAJE CON IO
    // recibo el cop
    int cod_op = recibir_operacion(cliente_fd);
    // si es un msg, lo leo y lo logueo
    if (cod_op == MENSAJE) {
      //recibir_mensaje(cliente_fd, logger_server);
      // --- INICIO PRUEBA ---
        log_info(logger_server, "Enviando orden de SLEEP de prueba a la IO...");
        
        // Armamos el paquete crudo tal cual lo espera tu IO
        t_paquete* paquete_prueba = malloc(sizeof(t_paquete));
        paquete_prueba->cop = IO_SLEEP; // Podés cambiarlo a IO_STDIN o IO_STDOUT para probar las otras
        paquete_prueba->buffer = malloc(sizeof(t_buffer));
        paquete_prueba->buffer->size = sizeof(int) * 2; // PID (4 bytes) + Tiempo (4 bytes)
        paquete_prueba->buffer->stream = malloc(paquete_prueba->buffer->size);
        
        int pid_prueba = 404;
        int parametro_prueba = 3000; // 3000 milisegundos (o 3000 caracteres a leer si pruebas STDIN)
        
        memcpy(paquete_prueba->buffer->stream, &pid_prueba, sizeof(int));
        memcpy(paquete_prueba->buffer->stream + sizeof(int), &parametro_prueba, sizeof(int));
        
        enviar_paquete(paquete_prueba, cliente_fd);
        eliminar_paquete(paquete_prueba);

        // Esperamos a que la IO nos conteste el "FIN_IO"
        int op_rta = recibir_operacion(cliente_fd);
        if(op_rta == MENSAJE) {
            recibir_mensaje(cliente_fd, logger_server);
        } else if (op_rta == IO_STDIN || op_rta == IO_STDOUT) {
            // Si probás STDIN/STDOUT, tu IO manda un paquete entero de vuelta, no solo un mensaje
            int size_rta;
            void* buffer_rta = recibir_buffer(&size_rta, cliente_fd);
            log_info(logger_server, "La IO terminó y respondió!");
            free(buffer_rta);
        }
        // --- FIN PRUEBA ---
    } else {
      log_warning(logger_server, "Operación no indentificada.");
    }
  }
  //===================================================//
  // libero memoria
  config_destroy(config_server);
  log_destroy(logger_server);
  return 0;
}
