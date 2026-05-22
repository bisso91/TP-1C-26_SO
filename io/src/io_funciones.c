#include "io_funciones.h"
#include "io_funciones.h"
#include <commons/log.h>
#include <readline/readline.h> // para funcion ejecutar stdin
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utils/utils.h>

void iniciar_bucle_io(int conexion_scheduler, char *tipo_io, t_log *logger) {
  while (1) {

    log_info(logger, "Esperando peticiones del Kernel Scheduler...");
    int cod_op = recibir_operacion(conexion_scheduler);

    if (cod_op == -1) {
      log_error(logger,
                "El Kernel Scheduler se desconecto. Terminando modulo IO");
      break; // interrumpo el bucle
    }

    switch (cod_op) {
    case IO_SLEEP:
      if (strcmp(tipo_io, "SLEEP") == 0) {
        ejecutar_sleep(conexion_scheduler, logger);
      } else {
        log_error(logger, "ERROR - tipo esperado: SLEEP. Tipo actual: %s",
                  tipo_io);
      }
      break;
    case IO_STDOUT:
      if (strcmp(tipo_io, "IO_STDOUT") == 0) {
        ejecutar_stdout(conexion_scheduler, logger);
      } else {
        log_error(logger, "ERROR - tipo esperado: STDOUT. Tipo actual: %s",
                  tipo_io);
      }
      break;
    case IO_STDIN:
      if (strcmp(tipo_io, "IO_STDIN") == 0) {
        ejecutar_stdin(conexion_scheduler, logger);
      } else {
        log_error(logger, "ERROR - tipo esperado: STDIN. Tipo actual: %s",
                  tipo_io);
      }
      break;
    default:
      log_warning(logger, "Operación desconocida...");
      break;
    }

    // Aca deberías avisarle al Kernel Scheduler que ya terminaste enviando un
    // paquete de respuesta enviar_mensaje("OK", conexion_scheduler);
    break;
  }
}

void ejecutar_sleep(int conexion_shceduler, t_log *logger) {
  int size;
  void *buffer = recibir_buffer(&size, conexion_shceduler);

  int desplazamiento = 0;
  int pid;
  int milisegundos;

  // saco el PID
  memcpy(&pid, buffer + desplazamiento, sizeof(int));
  desplazamiento += sizeof(int);

  // saco el tiempo
  memcpy(&milisegundos, buffer + desplazamiento, sizeof(int));

  // logs obligatorios
  log_info(logger, "## PID: %d - Inicio de IO", pid);
  log_info(logger, "PID: %d - Haciendo sleep por %d milisegundos.", pid,
           milisegundos);

  usleep(milisegundos * 1000); // usleep en microsegundos

  // logs obligatorios
  log_info(logger, "## PID: %d - Fin de IO", pid);

  free(buffer);

  // notifico al planificador q terminé
  enviar_mensaje("finalizó el modulo IO", conexion_shceduler);
}

void ejecutar_stdin(int conexion_shceduler, t_log *logger) {
  int size;
  void *buffer = recibir_buffer(&size, conexion_shceduler);
  int desplzamiento = 0;
  int pid;
  int size_to_read;

  // saco pid
  memcpy(&pid, buffer + desplzamiento, sizeof(int));
  desplzamiento += sizeof(int);

  // saco tamaño de lectura
  memcpy(&size_to_read, buffer + desplzamiento, sizeof(int));

  // log obligatorio
  log_info(logger, "## PID: %d - Inicio de IO", pid);
  // ------------------
  log_info(logger, "PID: %d - Esperando ingreso de %d caracteres...", pid,
           size_to_read);
  char *texto_leido = readline("> ");

  char *texto_final = calloc(1, size_to_read);

  if (texto_leido != NULL) {
    strncpy(texto_final, texto_leido, size_to_read);

    free(texto_leido); // readline hace malloc interno, asique tengo q liberar
                       // memoria
  }

  // log obligatorio
  log_info(logger, "## PID: %d - Fin de IO", pid);
  // ------------------
  free(buffer);

  // mando el texto al planificador
  t_paquete *paquete_rta = crear_paquete();
  paquete_rta->cop = IO_STDIN;
  agregar_a_paquete(paquete_rta, texto_final, size_to_read);

  enviar_paquete(paquete_rta, conexion_shceduler);

  eliminar_paquete(paquete_rta);
  free(texto_final);
}

void ejecutar_stdout(int conexion_shceduler, t_log *logger) {
  int size;
  void *buffer = recibir_buffer(&size, conexion_shceduler);

  int desplazamiento = 0;
  int pid;
  int size_of_text;

  // saco pid
  memcpy(&pid, buffer + desplazamiento, sizeof(int));
  desplazamiento += sizeof(int);

  // saco el tamaño del texto
  memcpy(&size_of_text, buffer + desplazamiento, sizeof(int));
  desplazamiento += sizeof(int);

  // saco el texto real y reservo memoria
  char *texto = malloc(size_of_text);
  memcpy(texto, buffer + desplazamiento, size_of_text);

  // log obligatorio
  log_info(logger, "## PID: %d - Inicio de IO", pid);

  log_info(logger, "PID: %d - CONTENIDO A IMPRIMIR: %s", pid, texto);

  log_info(logger, "## PID: %d - Fin de IO", pid);

  free(texto);
  free(buffer);

  // confirmación al kernel que terminó
  t_paquete *paquete_rta = crear_paquete();
  paquete_rta->cop = IO_STDOUT;
  enviar_paquete(paquete_rta, conexion_shceduler);
  eliminar_paquete(paquete_rta);

  // notifico al planificador q terminé
  enviar_mensaje("finalizó el modulo IO", conexion_shceduler);
}