#ifndef UTILS_H_
#define UTILS_H_

#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<unistd.h>
#include<netdb.h>
#include<string.h>
#include<commons/log.h>

//fn server
int iniciar_servidor(char *ip, char *puerto);
int esperar_cliente(int socket_servidor);

//fn client
int crear_conexion(char *ip, char *puerto);
void liberar_conexion(int socket_cliente);

#endif /* UTILS_H_ */

