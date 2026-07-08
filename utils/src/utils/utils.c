
#include "utils.h"
#include <commons/log.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/*--------------------
fns server
----------------------*/

int iniciar_servidor(char *ip, char *puerto) {
  int socket_servidor;
  struct addrinfo hints, *servinfo;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  int g = getaddrinfo(ip, puerto, &hints, &servinfo);
  if (g != 0) {
    return -1;
  }

  // socket de escucha
  socket_servidor =
      socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
  if (socket_servidor == -1) {
    freeaddrinfo(servinfo);
    return -1;
  }

  // permitir reutilizar puerto inmediatamente
  int yes = 1;
  if (setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
    close(socket_servidor);
    freeaddrinfo(servinfo);
    return -1;
  }

  // bind del puerto
  if (bind(socket_servidor, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
    close(socket_servidor);
    freeaddrinfo(servinfo);
    return -1;
  }

  // escucha de conexiones
  if (listen(socket_servidor, SOMAXCONN) == -1) {
    close(socket_servidor);
    freeaddrinfo(servinfo);
    return -1;
  }

  freeaddrinfo(servinfo);

  return socket_servidor;
}

int esperar_cliente(int socket_servidor) {
  if (socket_servidor == -1) return -1;
  // aceptacion del cliente nuevo
  int socket_cliente = accept(socket_servidor, NULL, NULL);
  return socket_cliente;
}

/*--------------------
fns client
----------------------*/

int crear_conexion(char *ip, char *puerto) {
  struct addrinfo hints, *server_info;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  int g = getaddrinfo(ip, puerto, &hints, &server_info);
  if (g != 0) {
    return -1;
  }

  // creacion del socket
  int socket_cliente = socket(server_info->ai_family, server_info->ai_socktype,
                              server_info->ai_protocol);
  if (socket_cliente == -1) {
    freeaddrinfo(server_info);
    return -1;
  }

  // conexion al server
  if (connect(socket_cliente, server_info->ai_addr, server_info->ai_addrlen) == -1) {
    close(socket_cliente);
    freeaddrinfo(server_info);
    return -1;
  }

  freeaddrinfo(server_info);

  return socket_cliente;
}

void liberar_conexion(int socket_cliente) { close(socket_cliente); }

/*
void enviar_mensaje(char *mensaje, int socket_cliente) {
  t_paquete *paquete = malloc(sizeof(t_paquete));
  paquete->cop = MENSAJE;
  paquete->buffer = malloc(sizeof(t_buffer));
  paquete->buffer->size = strlen(mensaje) + 1;
  paquete->buffer->stream = malloc(paquete->buffer->size);
  memcpy(paquete->buffer->stream, mensaje, paquete->buffer->size);

  int bytes = paquete->buffer->size + 2 * sizeof(int);
  void *a_enviar = serializar_paquete(paquete, bytes);

  send(socket_cliente, a_enviar, bytes, 0);

  free(a_enviar);
  eliminar_paquete(paquete);
}

//---------------------------------- REVISAR ----------------------------------
void *serializar_paquete(t_paquete *paquete, int bytes) { // REVISAR
  void *magic = malloc(bytes);
  int desplazamiento = 0;

  memcpy(magic + desplazamiento, &(paquete->cop), sizeof(int));
  desplazamiento += sizeof(int);
  memcpy(magic + desplazamiento, &(paquete->buffer->size), sizeof(int));
  desplazamiento += sizeof(int);
  memcpy(magic + desplazamiento, paquete->buffer->stream,
         paquete->buffer->size);
  desplazamiento += paquete->buffer->size;

  return magic;
}
//------------------------------------------------------------------------------

t_paquete *crear_paquete(void) {
  t_paquete *paquete = malloc(sizeof(t_paquete));
  paquete->cop = PAQUETE;
  crear_buffer(paquete);
  return paquete;
}
  
//---------------------------------- REVISAR ----------------------------------
void crear_buffer(t_paquete *paquete) {
  paquete->buffer = malloc(sizeof(t_buffer));
  paquete->buffer->size = 0;
  paquete->buffer->stream = NULL;
}
  */
//------------------------------------------------------------------------------
/*
void agregar_a_paquete(t_paquete *paquete, void *valor, int size) {
  paquete->buffer->stream = realloc(paquete->buffer->stream,
                                    paquete->buffer->size + size + sizeof(int));

  memcpy(paquete->buffer->stream + paquete->buffer->size, &size, sizeof(int));
  memcpy(paquete->buffer->stream + paquete->buffer->size + sizeof(int), valor,
         size);

  paquete->buffer->size += size + sizeof(int);
}

void enviar_paquete(t_paquete *paquete, int socket_cliente) {
  int bytes = paquete->buffer->size + 2 * sizeof(int);
  void *a_enviar = serializar_paquete(paquete, bytes);

  send(socket_cliente, a_enviar, bytes, 0);

  free(a_enviar);
}

void eliminar_paquete(t_paquete *paquete) {
  free(paquete->buffer->stream);
  free(paquete->buffer);
  free(paquete);
}

int recibir_operacion(int socket_cliente) {
  int cod_op;
  if (recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL) > 0) {
    return cod_op;
  } else {
    close(socket_cliente);
    return -1;
  }
}

void *recibir_buffer(int *size, int socket_cliente) {
  void *buffer;
  recv(socket_cliente, size, sizeof(int), MSG_WAITALL);
  buffer = malloc(*size); //RESERVA MEMORIAAA
  recv(socket_cliente, buffer, *size, MSG_WAITALL);
  return buffer;
}

void recibir_mensaje(int socket_cliente, t_log *logger) {
  int size;
  char *buffer = recibir_buffer(&size, socket_cliente);
  log_info(logger, "Mensaje recibido: %s", buffer);
  free(buffer);
}
  */

// --- --- //

void enviar_entero(int socket_cliente, int numero){
  send(socket_cliente, &numero, sizeof(int), 0);
}

int recibir_entero(int socket_cliente){
  int numero;

  recv(socket_cliente, &numero, sizeof(int), MSG_WAITALL); //MSG_WAITALL asegura que se lean todos los bytes del int antes de seguir
  return numero;
}

// --- PCB --- //
void enviar_pcb(t_pcb *pcb, int socket_cliente, int op_code){
    // calculo tamaño del payload
    int size = sizeof(int) * 4 + pcb->cantidad_segmentos * sizeof(t_segmento); //PID, PC, estado, prioridad_actual, prioridad_original
    void *stream = malloc(size);
    int desplazamiento = 0;

    // copio datos
    memcpy(stream + desplazamiento, &(pcb->pid), sizeof(int));
    desplazamiento += sizeof(int);
    memcpy(stream + desplazamiento, &(pcb->program_counter), sizeof(int));
    desplazamiento += sizeof(int);
    memcpy(stream + desplazamiento, &(pcb->estado), sizeof(int));
    desplazamiento += sizeof(int);
    memcpy(stream + desplazamiento, &(pcb->cantidad_segmentos), sizeof(int));
    desplazamiento += sizeof(int);

    if (pcb->cantidad_segmentos > 0 && pcb->tabla_segmentos != NULL) {
        memcpy(stream + desplazamiento, pcb->tabla_segmentos, pcb->cantidad_segmentos * sizeof(t_segmento));
        desplazamiento += pcb->cantidad_segmentos * sizeof(t_segmento);
    }
    memcpy(stream + desplazamiento, &(pcb->prioridad_actual), sizeof(int));
    desplazamiento += sizeof(int);
    memcpy(stream + desplazamiento, &(pcb->prioridad_original), sizeof(int));
    desplazamiento += sizeof(int);

    t_paquete *paquete = crear_paquete();
    paquete->cop = op_code;
    agregar_a_paquete(paquete, stream, size);
    enviar_paquete(paquete, socket_cliente);

    free(stream);
    eliminar_paquete(paquete);
}

t_pcb *recibir_pcb(int socket_cliente){
    t_pcb *pcb = malloc(sizeof(t_pcb));
    int size_total;

    void *stream = recibir_buffer(&size_total, socket_cliente); 
    
    int desplazamiento = sizeof(int);

    memcpy(&(pcb->pid), stream + desplazamiento, sizeof(int));
    desplazamiento += sizeof(int);
    memcpy(&(pcb->program_counter), stream + desplazamiento, sizeof(int));
    desplazamiento += sizeof(int);
    memcpy(&(pcb->estado), stream + desplazamiento, sizeof(int));
    desplazamiento += sizeof(int);
    memcpy(&(pcb->cantidad_segmentos), stream + desplazamiento, sizeof(int));
    desplazamiento += sizeof(int);

    if (pcb->cantidad_segmentos > 0) {
        pcb->tabla_segmentos = malloc(pcb->cantidad_segmentos * sizeof(t_segmento));
        memcpy(pcb->tabla_segmentos, stream + desplazamiento, pcb->cantidad_segmentos * sizeof(t_segmento));
    } else {
        pcb->tabla_segmentos = NULL;
    }
    memcpy(&(pcb->prioridad_actual), stream + desplazamiento, sizeof(int));
    desplazamiento += sizeof(int);
    memcpy(&(pcb->prioridad_original), stream + desplazamiento, sizeof(int));

    free(stream);
    return pcb;
}

// --- PAQUETES --- //
void enviar_mensaje(char *mensaje, int socket_cliente) {
  t_paquete *paquete = malloc(sizeof(t_paquete));
  paquete->cop = MENSAJE;
  paquete->buffer = malloc(sizeof(t_buffer));
  paquete->buffer->size = strlen(mensaje) + 1;
  paquete->buffer->stream = malloc(paquete->buffer->size);
  memcpy(paquete->buffer->stream, mensaje, paquete->buffer->size);

  int bytes = paquete->buffer->size + 2 * sizeof(int);
  void *a_enviar = serializar_paquete(paquete, bytes);

  send(socket_cliente, a_enviar, bytes, 0);

  free(a_enviar);
  eliminar_paquete(paquete);
}

//---------------------------------- REVISAR ----------------------------------
void *serializar_paquete(t_paquete *paquete, int bytes) { // REVISAR
  void *magic = malloc(bytes);
  int desplazamiento = 0;

  memcpy(magic + desplazamiento, &(paquete->cop), sizeof(int));
  desplazamiento += sizeof(int);
  memcpy(magic + desplazamiento, &(paquete->buffer->size), sizeof(int));
  desplazamiento += sizeof(int);
  memcpy(magic + desplazamiento, paquete->buffer->stream,
         paquete->buffer->size);
  desplazamiento += paquete->buffer->size;

  return magic;
}
//------------------------------------------------------------------------------

t_paquete *crear_paquete(void) {
  t_paquete *paquete = malloc(sizeof(t_paquete));
  paquete->cop = PAQUETE;
  crear_buffer(paquete);
  return paquete;
}

//---------------------------------- REVISAR ----------------------------------
void crear_buffer(t_paquete *paquete) {
  paquete->buffer = malloc(sizeof(t_buffer));
  paquete->buffer->size = 0;
  paquete->buffer->stream = NULL;
}
//------------------------------------------------------------------------------

void agregar_a_paquete(t_paquete *paquete, void *valor, int size) {
  paquete->buffer->stream = realloc(paquete->buffer->stream,
                                    paquete->buffer->size + size + sizeof(int));

  memcpy(paquete->buffer->stream + paquete->buffer->size, &size, sizeof(int));
  memcpy(paquete->buffer->stream + paquete->buffer->size + sizeof(int), valor,
         size);

  paquete->buffer->size += size + sizeof(int);
}

void enviar_paquete(t_paquete *paquete, int socket_cliente) {
  int bytes = paquete->buffer->size + 2 * sizeof(int);
  void *a_enviar = serializar_paquete(paquete, bytes);

  send(socket_cliente, a_enviar, bytes, 0);

  free(a_enviar);
}

void eliminar_paquete(t_paquete *paquete) {
  free(paquete->buffer->stream);
  free(paquete->buffer);
  free(paquete);
}

int recibir_operacion(int socket_cliente) {
  int cod_op;
  if (recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL) > 0) {
    return cod_op;
  } else {
    close(socket_cliente);
    return -1;
  }
}

void *recibir_buffer(int *size, int socket_cliente) {
  void *buffer;
  recv(socket_cliente, size, sizeof(int), MSG_WAITALL);
  buffer = malloc(*size); //RESERVA MEMORIAAA
  recv(socket_cliente, buffer, *size, MSG_WAITALL);
  return buffer;
}

void recibir_mensaje(int socket_cliente, t_log *logger) {
  int size;
  char *buffer = recibir_buffer(&size, socket_cliente);
  log_info(logger, "Mensaje recibido: %s", buffer);
  free(buffer);
}

char* recibir_string(int socket_cliente) {
    int size;
    char *buffer = recibir_buffer(&size, socket_cliente);
    return buffer;
}

void enviar_string(char *mensaje, int socket_cliente, op_code cop) {
  t_paquete *paquete = malloc(sizeof(t_paquete));
  paquete->cop = cop;
  paquete->buffer = malloc(sizeof(t_buffer));
  paquete->buffer->size = strlen(mensaje) + 1;
  paquete->buffer->stream = malloc(paquete->buffer->size);
  memcpy(paquete->buffer->stream, mensaje, paquete->buffer->size);

  int bytes = paquete->buffer->size + 2 * sizeof(int);
  void *a_enviar = serializar_paquete(paquete, bytes);

  send(socket_cliente, a_enviar, bytes, 0);

  free(a_enviar);
  eliminar_paquete(paquete);
}