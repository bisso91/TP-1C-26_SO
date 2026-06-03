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
       */