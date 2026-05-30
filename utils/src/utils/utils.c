#include "utils.h"
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/*--------------------
fns server
----------------------*/

int iniciar_servidor(char *puerto, t_log *logger_instancia) {
  struct addrinfo hints, *server_info;
  const int enable = 1;
  int fd_escucha;
  int err;

  log_trace(logger_instancia, "Estoy creando un socket en el puerto %s",
            puerto);

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  err = getaddrinfo(NULL, puerto, &hints, &server_info);
  if (err) {
    // Se agregó gai_strerror(err) para que imprima la causa exacta del error
    // DNS/IP
    log_error(logger_instancia, "Error en getaddrinfo: %s", gai_strerror(err));
    exit(EXIT_FAILURE);
  }

  fd_escucha = socket(server_info->ai_family, server_info->ai_socktype,
                      server_info->ai_protocol);
  if (fd_escucha < 0) {
    log_error(logger_instancia, "Error creando Socket...");
    exit(EXIT_FAILURE);
  }

  if (setsockopt(fd_escucha, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) <
      0)
    log_warning(logger_instancia,
                "Error al setear la dirección del socket como REUSABLE");

  if (setsockopt(fd_escucha, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(int)) <
      0)
    log_warning(logger_instancia,
                "Error al setear el puerto del socket como REUSABLE");

  bind(fd_escucha, server_info->ai_addr, server_info->ai_addrlen);
  log_trace(logger_instancia, "Socket creado satisfactoriamente");

  listen(fd_escucha, SOMAXCONN);
  log_trace(logger_instancia, "Socket listo para escuchar a mi cliente");

  freeaddrinfo(server_info);

  return fd_escucha;
}

int esperar_cliente(int socket_servidor) {

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

  getaddrinfo(ip, puerto, &hints, &server_info);

  // creacion del socket
  int socket_cliente = socket(server_info->ai_family, server_info->ai_socktype,
                              server_info->ai_protocol);

  // conexion al server
  connect(socket_cliente, server_info->ai_addr, server_info->ai_addrlen);

  freeaddrinfo(server_info);

  return socket_cliente;
}

void liberar_conexion(int socket_cliente) { close(socket_cliente); }