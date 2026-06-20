#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utils/utils.h>
#include <unistd.h>
#include <sys/socket.h>
#include <pthread.h>

void* mock_kernel_memory_thread(void* arg) {
    int server_fd = iniciar_servidor("127.0.0.1", "8000");
    if (server_fd < 0) {
        printf("MOCK_KERNEL: Error al iniciar el servidor en el puerto 8000.\n");
        return NULL;
    }
    printf("MOCK_KERNEL: Escuchando en el puerto 8000 para conexiones de Memory Stick...\n");

    int stick_fd = esperar_cliente(server_fd);
    if (stick_fd < 0) {
        printf("MOCK_KERNEL: Error al aceptar conexion de Memory Stick.\n");
        close(server_fd);
        return NULL;
    }
    printf("MOCK_KERNEL: Memory Stick conectado!\n");

    // Recibir identificación
    int op = recibir_operacion(stick_fd);
    if (op == IDENTIFICACION_STICK) {
        int size;
        void* buffer = recibir_buffer(&size, stick_fd);
        int offset = 0;
        int size_header;
        
        // Campo 1: tamanio
        memcpy(&size_header, buffer + offset, sizeof(int));
        offset += sizeof(int);
        int stick_size;
        memcpy(&stick_size, buffer + offset, sizeof(int));
        offset += size_header;
        
        // Campo 2: puerto
        memcpy(&size_header, buffer + offset, sizeof(int));
        offset += sizeof(int);
        int stick_port;
        memcpy(&stick_port, buffer + offset, sizeof(int));
        offset += size_header;
        
        // Campo 3: IP
        memcpy(&size_header, buffer + offset, sizeof(int));
        offset += sizeof(int);
        char* stick_ip = buffer + offset;
        
        printf("MOCK_KERNEL: Identificacion recibida -> Size: %d bytes, Port: %d, IP: %s\n", stick_size, stick_port, stick_ip);
        free(buffer);
    } else {
        printf("MOCK_KERNEL: Opcode inesperado del Memory Stick: %d\n", op);
    }

    // Mantener la conexión abierta para que el stick no falle
    while (1) {
        char dummy[10];
        if (recv(stick_fd, dummy, sizeof(dummy), 0) <= 0) {
            printf("MOCK_KERNEL: Conexión con Memory Stick cerrada.\n");
            break;
        }
    }

    close(stick_fd);
    close(server_fd);
    return NULL;
}

int main() {
    pthread_t thread;
    pthread_create(&thread, NULL, mock_kernel_memory_thread, NULL);
    pthread_detach(thread);

    // Dar tiempo para que el mock server inicie
    sleep(1);

    printf("MOCK_KERNEL: Listo.\n");
    
    // Esperar a que el stick conecte al mock y luego inicie su servidor
    sleep(3);

    printf("Conectando al memory stick en el puerto 8002 como CPU...\n");
    int connection = crear_conexion("127.0.0.1", "8002");
    if (connection <= 0) {
        printf("No se pudo conectar al memory stick.\n");
        return 1;
    }
    printf("Conectado exitosamente!\n");

    // Identificación
    printf("Enviando identificacion CPU...\n");
    enviar_string("CPU_TEST", connection, IDENTIFICACION_CPU);

    // Pedido de escritura
    uint32_t dir_fisica = 10;
    char* data = "Hello from memory stick test!";
    uint32_t tamanio = strlen(data) + 1;
    
    printf("Pedido de escritura de %u bytes en la dir física %u...\n", tamanio, dir_fisica);
    enviar_entero(connection, ESCRITURA_MEMORIA);
    send(connection, &dir_fisica, sizeof(uint32_t), 0);
    send(connection, &tamanio, sizeof(uint32_t), 0);
    send(connection, data, tamanio, 0);

    int res = recibir_entero(connection);
    printf("Resultado de la escritura: %d\n", res);

    // Pedido de lectura
    printf("Pedido de lectura de %u bytes de la dir física %u...\n", tamanio, dir_fisica);
    enviar_entero(connection, LECTURA_MEMORIA);
    send(connection, &dir_fisica, sizeof(uint32_t), 0);
    send(connection, &tamanio, sizeof(uint32_t), 0);

    char* read_buffer = malloc(tamanio);
    recv(connection, read_buffer, tamanio, MSG_WAITALL);
    printf("Contenido leido de la memoria: '%s'\n", read_buffer);
    free(read_buffer);

    liberar_conexion(connection);
    return 0;
}
