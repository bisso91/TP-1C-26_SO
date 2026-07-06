#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>
#include <utils/utils.h>
#include <commons/log.h>
#include <commons/config.h>

int main(int argc, char* argv[]) {
    saludar("swap");

    t_log *logger = log_create("swap.log", "SWAP", true, LOG_LEVEL_INFO);

    if(logger == NULL){
        printf("Error al crear el logger\n");
        return 1;
    }

    if (argc < 2) {
        log_error(logger, "Debe especificar el archivo de configuración. Uso: ./swap [config_path]");
        log_destroy(logger);
        return 1;
    }

    t_config *config = config_create(argv[1]);
    if (config == NULL){
        log_error(logger, "No se pudo encontrar el archivo de configuración %s", argv[1]);
        log_destroy(logger);
        return 1;
    }

    char *ip = config_get_string_value(config, "IP_SERVIDOR");
    char *puerto = config_get_string_value(config, "PUERTO_SERVIDOR");
    int swap_file_size = config_get_int_value(config, "SWAP_FILE_SIZE");
    int block_size = config_get_int_value(config, "BLOCK_SIZE");
    char *swap_file_path = config_get_string_value(config, "SWAP_FILE_PATH");

    // Initialize/Create Swap File of the exact size filled with zeroes
    FILE *f = fopen(swap_file_path, "wb+");
    if (f != NULL) {
        void *zeros = calloc(1, 1024);
        int written = 0;
        while (written < swap_file_size) {
            int to_write = (swap_file_size - written < 1024) ? (swap_file_size - written) : 1024;
            fwrite(zeros, to_write, 1, f);
            written += to_write;
        }
        free(zeros);
        fclose(f);
    } else {
        log_error(logger, "No se pudo crear el archivo de swap en %s", swap_file_path);
        config_destroy(config);
        log_destroy(logger);
        return 1;
    }

    int conexion = crear_conexion(ip, puerto);
    if (conexion != -1) {
        log_info(logger, "## Conectado a Kernel Memory");
        
        // Identify ourselves to Kernel Memory
        t_paquete *paquete = crear_paquete();
        paquete->cop = IDENTIFICACION_SWAP;
        agregar_a_paquete(paquete, &swap_file_size, sizeof(int));
        agregar_a_paquete(paquete, &block_size, sizeof(int));
        enviar_paquete(paquete, conexion);
        eliminar_paquete(paquete);
    } else {
        log_error(logger, "Error al intentar conectarse al servidor");
        config_destroy(config);
        log_destroy(logger);
        return 1;
    }

    while (1) {
        int cod_op = recibir_operacion(conexion);
        if (cod_op == -1) {
            log_warning(logger, "Se desconectó Kernel Memory. Finalizando SWAP.");
            break;
        }

        switch (cod_op) {
            case ESCRIBIR_BLOQUE: {
                int size_total;
                void *stream = recibir_buffer(&size_total, conexion);

                int desplazamiento = 0;
                int block_num;
                memcpy(&block_num, stream + desplazamiento, sizeof(int));
                desplazamiento += sizeof(int);

                void *block_data = malloc(block_size);
                memcpy(block_data, stream + desplazamiento, block_size);
                free(stream);

                FILE *f_swap = fopen(swap_file_path, "rb+");
                if (f_swap != NULL) {
                    fseek(f_swap, block_num * block_size, SEEK_SET);
                    fwrite(block_data, block_size, 1, f_swap);
                    fclose(f_swap);
                }
                free(block_data);

                log_info(logger, "## Escritura del bloque: %d", block_num);

                int ok = 1;
                send(conexion, &ok, sizeof(int), 0);
                break;
            }

            case LEER_BLOQUE: {
                int block_num = recibir_entero(conexion);

                void *block_data = malloc(block_size);
                memset(block_data, 0, block_size);

                FILE *f_swap = fopen(swap_file_path, "rb");
                if (f_swap != NULL) {
                    fseek(f_swap, block_num * block_size, SEEK_SET);
                    fread(block_data, block_size, 1, f_swap);
                    fclose(f_swap);
                }

                log_info(logger, "## Lectura del bloque: %d", block_num);

                send(conexion, block_data, block_size, 0);
                free(block_data);
                break;
            }

            default:
                log_warning(logger, "Operación desconocida recibida: %d", cod_op);
                break;
        }
    }

    liberar_conexion(conexion);
    config_destroy(config);
    log_destroy(logger);

    return 0;
}
