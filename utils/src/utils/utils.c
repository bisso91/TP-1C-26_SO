#include "utils.h"
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>


/*--------------------
fns server
----------------------*/

int iniciar_servidor(char *ip, char *puerto){
    int socket_servidor;
    struct addrinfo hints, *servinfo;

    memset(&hints, 0 , sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    getaddrinfo(ip, puerto, &hints, &servinfo);

    //socket de escucha
    socket_servidor = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);


    //bind del puerto
    bind(socket_servidor, servinfo->ai_addr, servinfo->ai_addrlen);

    //escucha de conexiones
    listen(socket_servidor, SOMAXCONN);

    freeaddrinfo(servinfo);

    return socket_servidor;
}

int esperar_cliente(int socket_servidor){
    
    //aceptacion del cliente nuevo
    int socket_cliente = accept(socket_servidor,NULL, NULL);
    return socket_cliente;
}

/*--------------------
fns client
----------------------*/

int crear_conexion(char *ip, char *puerto){
    struct addrinfo hints, *server_info;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    getaddrinfo(ip, puerto, &hints, &server_info);

    //creacion del socket
    int socket_cliente = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);

    //conexion al server
    connect(socket_cliente, server_info->ai_addr, server_info->ai_addrlen);

    freeaddrinfo(server_info);

    return socket_cliente;
}

void liberar_conexion(int socket_cliente){
    close(socket_cliente);
}