#include "io_funciones.h"
#include <commons/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void iniciar_bucle_io(int conexion_scheduler, char *tipo_io, t_log *logger) {
  while (1) {
    // Aca deberías usar una función de utils para recibir (ej:
    // recibir_operacion(conexion_scheduler)) int cod_op =
    // recibir_operacion(conexion_scheduler);

    // Simulemos que recibimos un PID para operar (esto te lo enviará el
    // Scheduler)
    int pid_actual = 0;

    // logs obligatorios de inicio

    log_info(logger, "## PID: %d - Inicio de IO", pid_actual);

    if (strcmp(tipo_io, "SLEEP") == 0) {
      ejecutar_sleep(pid_actual, conexion_scheduler, logger);
    } else if (strcmp(tipo_io, "STDOUT") == 0) {
      ejecutar_stdout(pid_actual, conexion_scheduler, logger);
    } else if (strcmp(tipo_io, "STDIN") == 0) {
      ejecutar_stdin(pid_actual, conexion_scheduler, logger);
    } else {
      log_error(logger, "Tipo de IO no reconocido. Abortando bucle...");
      break;
    }

    // logs obligatorios de fin

    log_info(logger, "## PID: %d - Fin de IO", pid_actual);
    // Aca deberías avisarle al Kernel Scheduler que ya terminaste enviando un
    // paquete de respuesta enviar_mensaje("OK", conexion_scheduler);

    // Rompemos el ciclo por ahora para que no te llene la consola de logs en
    // esta simulación
    break;
  }
}

void ejecutar_sleep(int pid, int conexion_shceduler, t_log *logger) {
  // int milisegundos = recibir_datos_de(conexion_scheduler);
  int milisegundos = 20; // simu

  log_info(logger, "PID: %d - Haciendo sleep por %d milisegundos.", pid,
           milisegundos);

  usleep(milisegundos * 1000);
}

void ejecutar_stdin(int pid, int conexion_shceduler, t_log *logger) {
  // int tamaño_a_leer = recibir_datos_de(conexion_scheduler);
  int size_to_read = 15; // simu

  log_info(logger, "## PID: %d - Ingrese %d caracteres: ", pid, size_to_read);

  // Acá iría la lógica de readline() o fgets()
}

void ejecutar_stdout(int pid, int conexion_shceduler, t_log *logger) {
  // char *texto = recibir_datos_de(conexion_scheduler);
  char *texto = "Hola desde STDOUT simu"; // simu

  log_info(logger, "## PID: %d - %s", pid, texto);
}