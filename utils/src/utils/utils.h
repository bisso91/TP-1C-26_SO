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

// --- ESTADOS DE UN PROCESO --- //
typedef enum{
    ESTADO_NEW,
    ESTADO_READY,
    ESTADO_BLOCK,
    ESTADO_SUS_READY,
    ESTADO_SUS_BLOCK,
    ESTADO_EXIT
} t_estado;

// --- PROCESS CONTROL BLOCK --- //
typedef struct{
    int pid;
    int program_counter;
    t_estado estado;
    // agregar registro cpu y otros
} t_pcb;

#endif /* UTILS_H_ */

