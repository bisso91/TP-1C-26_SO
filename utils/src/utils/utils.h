#ifndef UTILS_H_
#define UTILS_H_

#include <commons/log.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

// Esto es para que decidamos el formato pero tengo que poner algo sino rompe
typedef enum {
  // Handshake inicial
  HANDSHAKE_CPU,

  // Kernel Scheduler -> CPU
  EJECUTAR_PROCESO, // "Tomá este contexto y empezá el ciclo"
  INTERRUPCION,     // "Che, pará todo que hay que desalojar"

  // CPU -> Kernel Scheduler
  FIN_PROCESO,  // "Terminé la última instrucción (EXIT)"
  SOLICITUD_IO, // "El proceso pidió usar un dispositivo"

  // CPU -> Kernel Memory
  FETCH_INSTRUCCION, // "Dame lo que hay en el PC actual"
  LECTURA_MEMORIA,   // "Necesito el valor de esta dirección"
  ESCRITURA_MEMORIA  // "Guardá este valor en esta dirección"
} op_code;

// fn server
int iniciar_servidor(char *ip, char *puerto);
int esperar_cliente(int socket_servidor);

// fn client
int crear_conexion(char *ip, char *puerto);
void liberar_conexion(int socket_cliente);

#endif /* UTILS_H_ */
