#include "stick_utils.h"
#include <unistd.h>
#include <commons/string.h>
#include <pthread.h>
#include <arpa/inet.h>

typedef struct {
    int socket_fd;
    void* espacio_usuario;
    int tamanio_memoria;
    int retardo;
    t_log* logger;
} t_hilo_cliente_args;

void* atender_kernel_memory(void* arg) {
    t_hilo_cliente_args* args = (t_hilo_cliente_args*)arg;
    int socket_fd = args->socket_fd;
    void* espacio_usuario = args->espacio_usuario;
    int tamanio_memoria = args->tamanio_memoria;
    int retardo = args->retardo;
    t_log* logger = args->logger;
    free(args);

    while (1) {
        int op = recibir_operacion(socket_fd);
        if (op <= 0) {
            log_warning(logger, "Kernel Memory se desconectó.");
            break;
        }
        switch (op) {
            case LECTURA_MEMORIA:
                ejecutar_lectura_stick(socket_fd, espacio_usuario, tamanio_memoria, retardo, logger);
                break;
            case ESCRITURA_MEMORIA:
                ejecutar_escritura_stick(socket_fd, espacio_usuario, tamanio_memoria, retardo, logger);
                break;
            default:
                log_warning(logger, "Operacion desconocida recibida de Kernel Memory: %d", op);
                break;
        }
    }
    close(socket_fd);
    return NULL;
}

void* atender_cpu(void* arg) {
    t_hilo_cliente_args* args = (t_hilo_cliente_args*)arg;
    int socket_fd = args->socket_fd;
    void* espacio_usuario = args->espacio_usuario;
    int tamanio_memoria = args->tamanio_memoria;
    int retardo = args->retardo;
    t_log* logger = args->logger;
    free(args);

    // Esperar identificación de la CPU
    int cod_op = recibir_operacion(socket_fd);
    if (cod_op == IDENTIFICACION_CPU) {
        char* cpu_id = recibir_string(socket_fd);
        log_info(logger, "## CPU %s Conectada", cpu_id);
        free(cpu_id);
    } else {
        log_warning(logger, "Cliente conectado sin IDENTIFICACION_CPU. Primer opcode: %d", cod_op);
        // Si no es identificación, pero es lectura o escritura, lo manejamos
        if (cod_op == LECTURA_MEMORIA) {
            ejecutar_lectura_stick(socket_fd, espacio_usuario, tamanio_memoria, retardo, logger);
        } else if (cod_op == ESCRITURA_MEMORIA) {
            ejecutar_escritura_stick(socket_fd, espacio_usuario, tamanio_memoria, retardo, logger);
        } else if (cod_op <= 0) {
            close(socket_fd);
            return NULL;
        }
    }

    while (1) {
        int op = recibir_operacion(socket_fd);
        if (op <= 0) {
            break;
        }
        switch (op) {
            case LECTURA_MEMORIA:
                ejecutar_lectura_stick(socket_fd, espacio_usuario, tamanio_memoria, retardo, logger);
                break;
            case ESCRITURA_MEMORIA:
                ejecutar_escritura_stick(socket_fd, espacio_usuario, tamanio_memoria, retardo, logger);
                break;
            default:
                log_warning(logger, "Operacion desconocida recibida de CPU: %d", op);
                break;
        }
    }
    close(socket_fd);
    return NULL;
}

void iniciar_operacion_stick(char* config_path, int tamanio) {
    t_config* config = config_create(config_path);
    if (config == NULL) {
        printf("Error: No se pudo cargar la configuracion: %s\n", config_path);
        return;
    }

    char* log_level_str = config_get_string_value(config, "LOG_LEVEL");
    t_log_level log_level = log_level_from_string(log_level_str ? log_level_str : "INFO");
    t_log* logger = log_create("memory_stick.log", "MEMORY_STICK", true, log_level);
    if (logger == NULL) {
        printf("Error: No se pudo crear el logger.\n");
        config_destroy(config);
        return;
    }

    void* espacio_usuario = malloc(tamanio);
    if (espacio_usuario == NULL) {
        log_error(logger, "No se pudo reservar %d bytes de memoria.", tamanio);
        config_destroy(config);
        log_destroy(logger);
        return;
    }
    memset(espacio_usuario, 0, tamanio);

    char* ip_servidor = config_get_string_value(config, "IP_SERVIDOR");
    char* puerto_servidor = config_get_string_value(config, "PUERTO_SERVIDOR");
    char* ip_memoria = config_get_string_value(config, "IP_MEMORIA");
    char* puerto_escucha = config_get_string_value(config, "PUERTO_ESCUCHA");

    int retardo = 0;
    if (config_has_property(config, "RETARDO")) {
        retardo = config_get_int_value(config, "RETARDO");
    } else if (config_has_property(config, "MEMORY_DELAY")) {
        retardo = config_get_int_value(config, "MEMORY_DELAY");
    }

    log_info(logger, "Intentando conectar a Kernel Memory en %s:%s...", ip_servidor, puerto_servidor);
    int kernel_fd = crear_conexion(ip_servidor, puerto_servidor);
    if (kernel_fd < 0) {
        log_error(logger, "Error: No se pudo conectar a Kernel Memory.");
        free(espacio_usuario);
        config_destroy(config);
        log_destroy(logger);
        return;
    }

    // Log obligatorio: "## Conectado a Kernel Memory"
    log_info(logger, "## Conectado a Kernel Memory");

    // Identificación ante Kernel Memory
    t_paquete* paquete = crear_paquete();
    paquete->cop = IDENTIFICACION_STICK;
    int port_num = atoi(puerto_escucha);
    agregar_a_paquete(paquete, &tamanio, sizeof(int));
    agregar_a_paquete(paquete, &port_num, sizeof(int));
    agregar_a_paquete(paquete, ip_memoria, strlen(ip_memoria) + 1);
    enviar_paquete(paquete, kernel_fd);
    eliminar_paquete(paquete);

    // Hilo para atender las peticiones de Kernel Memory
    pthread_t hilo_kernel;
    t_hilo_cliente_args* args_kernel = malloc(sizeof(t_hilo_cliente_args));
    args_kernel->socket_fd = kernel_fd;
    args_kernel->espacio_usuario = espacio_usuario;
    args_kernel->tamanio_memoria = tamanio;
    args_kernel->retardo = retardo;
    args_kernel->logger = logger;
    pthread_create(&hilo_kernel, NULL, atender_kernel_memory, args_kernel);
    pthread_detach(hilo_kernel);

    // Iniciar servidor para CPUs
    int server_fd = iniciar_servidor(ip_memoria, puerto_escucha);
    if (server_fd == -1) {
        log_error(logger, "Error: No se pudo iniciar el servidor en %s:%s", ip_memoria, puerto_escucha);
        close(kernel_fd);
        free(espacio_usuario);
        config_destroy(config);
        log_destroy(logger);
        return;
    }

    log_info(logger, "Servidor Memory Stick iniciado en %s:%s. Esperando conexiones de CPUs...", ip_memoria, puerto_escucha);

    while (1) {
        int cliente_fd = esperar_cliente(server_fd);
        if (cliente_fd == -1) {
            log_error(logger, "Error al aceptar conexion de CPU.");
            continue;
        }

        pthread_t hilo_cpu;
        t_hilo_cliente_args* args_cpu = malloc(sizeof(t_hilo_cliente_args));
        args_cpu->socket_fd = cliente_fd;
        args_cpu->espacio_usuario = espacio_usuario;
        args_cpu->tamanio_memoria = tamanio;
        args_cpu->retardo = retardo;
        args_cpu->logger = logger;
        pthread_create(&hilo_cpu, NULL, atender_cpu, args_cpu);
        pthread_detach(hilo_cpu);
    }

    close(server_fd);
    close(kernel_fd);
    free(espacio_usuario);
    config_destroy(config);
    log_destroy(logger);
}

void ejecutar_lectura_stick(int cliente_fd, void* espacio_usuario, int tamanio_memoria, int retardo, t_log* logger) {
    uint32_t dir_fisica;
    uint32_t tamanio;

    if (recv(cliente_fd, &dir_fisica, sizeof(uint32_t), MSG_WAITALL) <= 0) {
        log_error(logger, "Error al recibir direccion fisica de lectura.");
        return;
    }
    if (recv(cliente_fd, &tamanio, sizeof(uint32_t), MSG_WAITALL) <= 0) {
        log_error(logger, "Error al recibir tamaño de lectura.");
        return;
    }

    if (dir_fisica + tamanio > (uint32_t)tamanio_memoria) {
        log_error(logger, "Error de lectura: Acceso fuera de limites (Dir: %u, Size: %u, Max: %u)", dir_fisica, tamanio, tamanio_memoria);
        void* error_buf = calloc(1, tamanio);
        send(cliente_fd, error_buf, tamanio, 0);
        free(error_buf);
        return;
    }

    if (retardo > 0) {
        usleep(retardo * 1000);
    }

    void* buffer = malloc(tamanio);
    memcpy(buffer, espacio_usuario + dir_fisica, tamanio);

    send(cliente_fd, buffer, tamanio, 0);

    log_info(logger, "## Lectura de %u bytes", tamanio);

    free(buffer);
}

void ejecutar_escritura_stick(int cliente_fd, void* espacio_usuario, int tamanio_memoria, int retardo, t_log* logger) {
    uint32_t dir_fisica;
    uint32_t tamanio;

    if (recv(cliente_fd, &dir_fisica, sizeof(uint32_t), MSG_WAITALL) <= 0) {
        log_error(logger, "Error al recibir direccion fisica de escritura.");
        return;
    }
    if (recv(cliente_fd, &tamanio, sizeof(uint32_t), MSG_WAITALL) <= 0) {
        log_error(logger, "Error al recibir tamaño de escritura.");
        return;
    }

    void* buffer = malloc(tamanio);
    if (recv(cliente_fd, buffer, tamanio, MSG_WAITALL) <= 0) {
        log_error(logger, "Error al recibir datos de escritura.");
        free(buffer);
        return;
    }

    int resultado = 1;
    if (dir_fisica + tamanio > (uint32_t)tamanio_memoria) {
        log_error(logger, "Error de escritura: Acceso fuera de limites (Dir: %u, Size: %u, Max: %u)", dir_fisica, tamanio, tamanio_memoria);
        resultado = -1;
    } else {
        if (retardo > 0) {
            usleep(retardo * 1000);
        }

        memcpy(espacio_usuario + dir_fisica, buffer, tamanio);
        log_info(logger, "## Escritura de %u bytes", tamanio);
    }

    send(cliente_fd, &resultado, sizeof(int), 0);

    free(buffer);
}
