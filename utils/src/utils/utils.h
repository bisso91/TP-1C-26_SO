#ifndef UTILS_H_
#define UTILS_H_

#include <commons/log.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

// struc's de paquetes

typedef enum { MENSAJE, PAQUETE, IO_SLEEP, IO_STDIN, IO_STDOUT } op_code;

typedef struct {
  int size;
  void *stream;
} t_buffer;

typedef struct {
  op_code cop;
  t_buffer *buffer;
} t_paquete;

// fn server
int iniciar_servidor(char *ip, char *puerto);
int esperar_cliente(int socket_servidor);

// fn client
int crear_conexion(char *ip, char *puerto);
void liberar_conexion(int socket_cliente);


// fn's de paquetes
void *serializar_paquete(t_paquete *paquete, int bytes); // la pongo arriba de enviar mensaje para que la detecte antes de usarla
void enviar_mensaje(char *mensaje, int socket_cliente);
void crear_buffer(t_paquete *paquete);
t_paquete *crear_paquete(void);
void agregar_a_paquete(t_paquete *paquete, void *valor, int size);
void enviar_paquete(t_paquete *paquete, int socket_cliente);
void eliminar_paquete(t_paquete *paquete);
int recibir_operacion(int socket_cliente);
void *recibir_buffer(int *size, int socket_cliente);
void recibir_mensaje(int socket_cliente, t_log *logger);


#endif /* UTILS_H_ */