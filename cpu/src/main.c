#include "cpu_utils.h"
#include <commons/config.h>
#include <commons/log.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <utils/hello.h>
#include <utils/utils.h>

//prox sacar esto de aca y ponerlo en un .h aparte
void prueba_conexion_con_kernel_memory(int cliente_fd, t_log* logger);

int main(int argc, char *argv[]) {
  saludar("cpu");

  char *config_path = (argc > 1) ? argv[1] : "./cpu.config";
  char *id_cpu = (argc > 2) ? argv[2] : "1";

  inicializar_cpu(config_path, id_cpu);

  finalizar_cpu();

  return 0;
}

/*
ANOTACIONES....

  CONEXIONES
   Aca fd es File descriptor.
   exportar a inicializacion --> Hecho
   tengo que checkear que no haya conexiones ya hechas a memoria y scheduler y
   stick, pero tengo que dejar algo escuchando por si quiero conectar otro stick
   kernel memory me avisa que hay stick o lo tengo que detectar?

*/

/* DEJO ESTO X ACA PARA DESPUES...
 // Defino logica para "recibir cod_ops"
 // ACA TENGO QUE RECIBIR PID


while (1) {
   // La ejecución se frena acá hasta que llegue un mensaje
   int cod_op = recibir_operacion(fd_scheduler, logger_cpu);

   /*    switch (cod_op) {
       case EJECUTAR_PROCESO:
         log_info(logger_cpu,
                  "Me llegó un proceso. Iniciando Ciclo de Instrucción.");
         // ACA llamarías a tu ciclo: ejecutar_ciclo(fd_memory, fd_scheduler);
         break;

       case INTERRUPCION:
         log_warning(logger_cpu, "¡Interrupción recibida! Desalojando...");
         // Lógica para frenar el ciclo actual
         break;
    default:
      log_error(logger_cpu, "Operación desconocida: %d", cod_op);
      break;
    }
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
    prueba_conexion_con_kernel_memory(conexion_memory, logger);
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

       case -1:
         log_error(logger_cpu, "El Scheduler se desconectó. Terminando CPU.");
         return EXIT_FAILURE;

         default:
         log_error(logger_cpu, "Operación desconocida: %d", cod_op);
         break;
       }
       

  return 0;;
}

void prueba_conexion_con_kernel_memory(int conexion_memory, t_log* logger) {
  t_paquete* paquete_iniciar = crear_paquete();
    paquete_iniciar->cop = INICIAR_PROCESO;
    enviar_paquete(paquete_iniciar, conexion_memory);
    eliminar_paquete(paquete_iniciar);

    // Le damos un microsegundo para que termine de procesar el archivo
    usleep(1000); 

    // 2. Le pedimos la primera instrucción (nuestro código hardcodeado pide el PC 0)
    t_paquete* paquete_pedir = crear_paquete();
    paquete_pedir->cop = PEDIR_INSTRUCCION;
    enviar_paquete(paquete_pedir, conexion_memory);
    eliminar_paquete(paquete_pedir);

    // 3. Recibimos la respuesta de Kernel Memory (ahora nos va a mandar un paquete, no un int)
    int cod_op = recibir_operacion(conexion_memory);
    if (cod_op == MENSAJE) { // O el código que hayas usado
        // Recibimos el tamaño del string y luego el string
        int size;
        recv(conexion_memory, &size, sizeof(int), MSG_WAITALL);
        char* instruccion = malloc(size);
        recv(conexion_memory, instruccion, size, MSG_WAITALL);

        log_info(logger, "PRUEBA ÉXITO: Recibí la instrucción '%s'", instruccion);
        free(instruccion);
    }
}
*/