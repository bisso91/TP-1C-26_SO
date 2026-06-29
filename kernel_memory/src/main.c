#include <commons/config.h>
#include <commons/log.h>
#include <commons/string.h>
#include <commons/collections/list.h>
#include <commons/collections/dictionary.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <poll.h>
#include <utils/hello.h>
#include <utils/utils.h>

typedef struct {
    int socket;
    int base;
    int limite;
    int puerto;
    char *ip;
} t_memory_stick_reg;

typedef struct {
    int id;
    int base;
    int limite;
} t_segmento_mem;

typedef struct {
    int pid;
    t_list *segmentos; // list of t_segmento_mem*
} t_tabla_segmentos;

typedef struct {
    int base;
    int limite;
    int size;
} t_free_block;

t_log *logger;
t_dictionary *diccionario_instrucciones;
char *path_base_scripts;
int socket_scheduler = -1;
char *estrategia_asignacion = "BEST";

t_list *lista_sticks = NULL;
pthread_mutex_t mutex_sticks;

t_dictionary *tablas_segmentos_procesos = NULL; // PID string -> t_tabla_segmentos*
t_list *huecos_libres = NULL; // list of t_free_block*
pthread_mutex_t mutex_memoria;

// Firmas de funciones
void *atender_cliente(void *arg);
void procesar_iniciar_proceso(int cliente_fd);
void procesar_pedir_instruccion(int cliente_fd);
void procesar_identificacion_stick(int cliente_fd);
void procesar_obtener_sticks(int cliente_fd);
void procesar_mem_alloc(int client_fd);
void procesar_mem_free(int client_fd);
bool stick_leer_datos(uint32_t dir_fisica, int tamanio, void *dest_buffer);
bool stick_escribir_datos(uint32_t dir_fisica, int tamanio, void *src_buffer);
void procesar_compactacion();

int main(int argc, char *argv[]) {
    saludar("kernel_memory");
    
    logger = log_create("kernel_memory.log", "KERNEL_MEMORY", true, LOG_LEVEL_INFO);
    if (logger == NULL) {
        printf("No se creo el logger\n");
        return 1;
    }

    t_config *config = config_create("kernel_memory.config"); // Corregido el typo congfig_create
    if (config == NULL) {
        log_error(logger, "No se pudo encontrar el archivo kernel_memory.config");
        return 1;
    }

    char *ip = config_get_string_value(config, "IP_MEMORIA");
    char *puerto = config_get_string_value(config, "PUERTO_ESCUCHA");
    path_base_scripts = config_get_string_value(config, "SCRIPTS_BASEPATH");
    if (config_has_property(config, "ALLOCATION_STRATEGY")) {
        estrategia_asignacion = config_get_string_value(config, "ALLOCATION_STRATEGY");
    }

    // guardo las lineas asociadas al PID del txt
    diccionario_instrucciones = dictionary_create();

    lista_sticks = list_create();
    pthread_mutex_init(&mutex_sticks, NULL);

    tablas_segmentos_procesos = dictionary_create();
    huecos_libres = list_create();
    pthread_mutex_init(&mutex_memoria, NULL);

    int server_fd = iniciar_servidor(ip, puerto);
    log_info(logger, "Kernel Memory iniciado en %s:%s. Esperando conexiones...", ip, puerto);

    // ciclo multihilo 
    while(1){
        int cliente_fd = esperar_cliente(server_fd);
        if (cliente_fd != -1) {
            log_info(logger, "## Nuevo Cliente Conectado - FD del socket: %d", cliente_fd);
            
            pthread_t hilo_cliente;
            int *socket_hilo = malloc(sizeof(int));
            *socket_hilo = cliente_fd;

            pthread_create(&hilo_cliente, NULL, atender_cliente, socket_hilo); // Corregido el typo hiloo_cliente
            pthread_detach(hilo_cliente);
        }
    }

    config_destroy(config);
    log_destroy(logger);
    dictionary_destroy(diccionario_instrucciones);
    return 0;
}

void *atender_cliente(void *arg) {
    int cliente_fd = *(int*) arg; // Corregido el casteo y paréntesis faltante
    free(arg);

    while(1) { // Corregido el paréntesis sobrante
        int cod_op = recibir_operacion(cliente_fd);
        if (cod_op == -1) {
            log_warning(logger, "El cliente con FD %d se desconectó.", cliente_fd);
            
            pthread_mutex_lock(&mutex_sticks);
            int stick_index = -1;
            int cant_sticks = list_size(lista_sticks);
            t_memory_stick_reg *disconnected_stick = NULL;
            for (int i = 0; i < cant_sticks; i++) {
                t_memory_stick_reg *stick = list_get(lista_sticks, i);
                if (stick->socket == cliente_fd) {
                    disconnected_stick = stick;
                    stick_index = i;
                    break;
                }
            }
            
            if (disconnected_stick != NULL) {
                log_error(logger, "## Memory Stick en puerto %d desconectado. Notificando al Scheduler...", disconnected_stick->puerto);
                list_remove(lista_sticks, stick_index);
                free(disconnected_stick->ip);
                free(disconnected_stick);
                pthread_mutex_unlock(&mutex_sticks);
                
                int socket_sched = crear_conexion("127.0.0.1", "8001");
                if (socket_sched != -1) {
                    int cop = STICK_DESCONECTADO;
                    send(socket_sched, &cop, sizeof(int), 0);
                    close(socket_sched);
                } else {
                    log_error(logger, "No se pudo conectar al Scheduler para notificar BSOD.");
                }
            } else {
                pthread_mutex_unlock(&mutex_sticks);
            }
            
            liberar_conexion(cliente_fd);
            break;             
        } // <- Agregada llave faltante del if

        switch (cod_op) {
            case IDENTIFICACION_STICK:
                procesar_identificacion_stick(cliente_fd);
                break;

            case OBTENER_STICKS:
                procesar_obtener_sticks(cliente_fd);
                break;

            case MEM_ALLOC:
                procesar_mem_alloc(cliente_fd);
                break;

            case MEM_FREE:
                procesar_mem_free(cliente_fd);
                break;

            case SUSPENDER_PROCESO:
                procesar_suspender_proceso(cliente_fd);
                break;

            case DES_SUSPENDER_PROCESO:
                procesar_des_suspender_proceso(cliente_fd);
                break;

            case INICIAR_PROCESO:
                socket_scheduler = cliente_fd;
                procesar_iniciar_proceso(cliente_fd);
                break;
                
            case PEDIR_INSTRUCCION:
                procesar_pedir_instruccion(cliente_fd);
                break;
                
            case CONSULTAR_ESPACIO_LIBRE: {
                // Mockeo de espacio
                uint32_t espacio_falso = 999999;
                send(cliente_fd, &espacio_falso, sizeof(uint32_t), 0);
                log_info(logger, "Simulando espacio libre (MOCK) = %u", espacio_falso);
                break;
            }
            case LEER_MEMORIA: {
                struct pollfd pfd;
                pfd.fd = cliente_fd;
                pfd.events = POLLIN;
                int poll_res = poll(&pfd, 1, 0);
                if (poll_res > 0 && (pfd.revents & POLLIN)) {
                    int size_total;
                    void *stream = recibir_buffer(&size_total, cliente_fd);
                    
                    int desplazamiento = sizeof(int); // skip pid size (4)
                    int pid;
                    memcpy(&pid, stream + desplazamiento, sizeof(int));
                    desplazamiento += sizeof(int);
                    
                    desplazamiento += sizeof(int); // skip dir size (4)
                    int dir_fisica;
                    memcpy(&dir_fisica, stream + desplazamiento, sizeof(int));
                    desplazamiento += sizeof(int);
                    
                    desplazamiento += sizeof(int); // skip size size (4)
                    int tamanio;
                    memcpy(&tamanio, stream + desplazamiento, sizeof(int));
                    
                    free(stream);
                    
                    void *valor_mock = calloc(1, tamanio);
                    if (tamanio == 1) {
                        *(uint8_t*)valor_mock = 0x2A; // 42 in hex
                    } else if (tamanio == 4) {
                        *(uint32_t*)valor_mock = 42;
                    }
                    send(cliente_fd, valor_mock, tamanio, 0);
                    free(valor_mock);
                    log_info(logger, "Simulando éxito en LECTURA de memoria (PID: %d, Dir: %d, Tam: %d).", pid, dir_fisica, tamanio);
                } else {
                    int ok = 1;
                    send(cliente_fd, &ok, sizeof(int), 0);
                    log_info(logger, "Simulando éxito en LECTURA de memoria (MOCK raw).");
                }
                break;
            }

            case ESCRIBIR_MEMORIA: {
                struct pollfd pfd;
                pfd.fd = cliente_fd;
                pfd.events = POLLIN;
                int poll_res = poll(&pfd, 1, 0);
                if (poll_res > 0 && (pfd.revents & POLLIN)) {
                    int size_total;
                    void *stream = recibir_buffer(&size_total, cliente_fd);
                    
                    int desplazamiento = sizeof(int); // skip pid size (4)
                    int pid;
                    memcpy(&pid, stream + desplazamiento, sizeof(int));
                    desplazamiento += sizeof(int);
                    
                    desplazamiento += sizeof(int); // skip dir size (4)
                    int dir_fisica;
                    memcpy(&dir_fisica, stream + desplazamiento, sizeof(int));
                    desplazamiento += sizeof(int);
                    
                    desplazamiento += sizeof(int); // skip size size (4)
                    int tamanio;
                    memcpy(&tamanio, stream + desplazamiento, sizeof(int));
                    desplazamiento += sizeof(int);
                    
                    desplazamiento += sizeof(int); // skip value size (4)
                    void *valor = malloc(tamanio);
                    memcpy(valor, stream + desplazamiento, tamanio);
                    
                    free(stream);
                    
                    if (tamanio == 1) {
                        log_info(logger, "Simulando éxito en ESCRITURA (PID: %d, Dir: %d, Tam: 1, Val: %u).", pid, dir_fisica, *(uint8_t*)valor);
                    } else if (tamanio == 4) {
                        log_info(logger, "Simulando éxito en ESCRITURA (PID: %d, Dir: %d, Tam: 4, Val: %u).", pid, dir_fisica, *(uint32_t*)valor);
                    } else {
                        log_info(logger, "Simulando éxito en ESCRITURA (PID: %d, Dir: %d, Tam: %d).", pid, dir_fisica, tamanio);
                    }
                    free(valor);
                    
                    int ok = 1;
                    send(cliente_fd, &ok, sizeof(int), 0);
                } else {
                    int ok = 1;
                    send(cliente_fd, &ok, sizeof(int), 0);
                    log_info(logger, "Simulando éxito en ESCRITURA de memoria (MOCK raw).");
                }
                break;
            }

            default:
                log_warning(logger, "Operación desconocida o no implementada: %d", cod_op);
                break;
        } 
    } // <- Agregada llave faltante del while
    return NULL;    
}

void procesar_iniciar_proceso(int cliente_fd) {
    // El scheduler debe mandar algo como: "1 proceso1.txt" (PID + PATH)
    char* mensaje_recibido = recibir_string(cliente_fd); 
    char** parametros = string_split(mensaje_recibido, " ");
    char* pid_string = string_duplicate(parametros[0]);
    char* nombre_archivo = parametros[1];

    char* ruta_completa = string_from_format("%s/%s", path_base_scripts, nombre_archivo);
    
    FILE* archivo = fopen(ruta_completa, "r");
    if (archivo == NULL) {
        log_error(logger, "No se pudo abrir el script %s", ruta_completa);
        free(ruta_completa);
        free(mensaje_recibido);
        string_array_destroy(parametros);
        return;
    }

    // Leemos el archivo entero
    fseek(archivo, 0, SEEK_END);
    long fsize = ftell(archivo);
    fseek(archivo, 0, SEEK_SET);
    char* contenido = malloc(fsize + 1);
    fread(contenido, fsize, 1, archivo);
    fclose(archivo);
    contenido[fsize] = '\0'; 

    // Dividimos por enter (\n) y guardamos en diccionario usando el PID de llave
    char** array_instrucciones = string_split(contenido, "\n");
    dictionary_put(diccionario_instrucciones, pid_string, array_instrucciones);

    log_info(logger, "## PID: %s - Proceso Creado - Instrucciones cargadas", pid_string);

    t_tabla_segmentos *tabla = malloc(sizeof(t_tabla_segmentos));
    tabla->pid = atoi(pid_string);
    tabla->segmentos = list_create();
    
    pthread_mutex_lock(&mutex_memoria);
    dictionary_put(tablas_segmentos_procesos, pid_string, tabla);
    pthread_mutex_unlock(&mutex_memoria);

    // Respondemos OK al scheduler
    int ok = 1;
    send(cliente_fd, &ok, sizeof(int), 0);

    free(ruta_completa);
    free(contenido);
    free(mensaje_recibido);
    string_array_destroy(parametros);
}

// Busca en el diccionario y devuelve la instrucción según el Program Counter
void procesar_pedir_instruccion(int cliente_fd) {
    // La CPU debe mandar algo como: "1 3" (PID + Program Counter)
    char* mensaje = recibir_string(cliente_fd);
    char** parametros = string_split(mensaje, " ");
    char* pid_string = parametros[0];
    int pc_recibido = atoi(parametros[1]);

    char** array_instrucciones = dictionary_get(diccionario_instrucciones, pid_string);

    if (array_instrucciones != NULL) {
        char* instruccion = array_instrucciones[pc_recibido];
        
        if(instruccion != NULL) {
            log_info(logger, "## Obtener instrucción - PID: %s - Instrucción: %s", pid_string, instruccion);
            // Usamos tu funcion de utils que ya serializa textos
            enviar_mensaje(instruccion, cliente_fd);
        } else {
             log_warning(logger, "Se llego al fin de instrucciones de PID %s", pid_string);
             enviar_mensaje("EXIT", cliente_fd); 
        }
    } else {
        log_error(logger, "No se encontraron instrucciones para el PID %s", pid_string);
        enviar_mensaje("ERROR", cliente_fd);
    }

    free(mensaje);
    string_array_destroy(parametros);
}


void procesar_identificacion_stick(int cliente_fd) {
    int size_total;
    void *stream = recibir_buffer(&size_total, cliente_fd);
    
    int desplazamiento = 0;
    int tamanio;
    memcpy(&tamanio, stream + desplazamiento, sizeof(int));
    desplazamiento += sizeof(int);
    
    int port_num;
    memcpy(&port_num, stream + desplazamiento, sizeof(int));
    desplazamiento += sizeof(int);
    
    char *ip = string_duplicate((char*)(stream + desplazamiento));
    free(stream);

    t_memory_stick_reg *stick = malloc(sizeof(t_memory_stick_reg));
    stick->socket = cliente_fd;
    stick->puerto = port_num;
    stick->ip = ip;
    
    pthread_mutex_lock(&mutex_sticks);
    int current_total_size = 0;
    int cant_sticks = list_size(lista_sticks);
    if (cant_sticks > 0) {
        t_memory_stick_reg *last_stick = list_get(lista_sticks, cant_sticks - 1);
        current_total_size = last_stick->limite + 1;
    }
    stick->base = current_total_size;
    stick->limite = current_total_size + tamanio - 1;
    list_add(lista_sticks, stick);
    
    // Al conectar un stick, expandimos los huecos libres.
    t_free_block *nuevo_hueco = malloc(sizeof(t_free_block));
    nuevo_hueco->base = stick->base;
    nuevo_hueco->limite = stick->limite;
    nuevo_hueco->size = tamanio;
    
    pthread_mutex_lock(&mutex_memoria);
    list_add(huecos_libres, nuevo_hueco);
    pthread_mutex_unlock(&mutex_memoria);
    
    pthread_mutex_unlock(&mutex_sticks);

    log_info(logger, "## Memory Stick de %d bytes Conectada", tamanio);
}

void procesar_obtener_sticks(int cliente_fd) {
    pthread_mutex_lock(&mutex_sticks);
    int cant_sticks = list_size(lista_sticks);
    
    t_paquete *paquete = crear_paquete();
    paquete->cop = OBTENER_STICKS;
    agregar_a_paquete(paquete, &cant_sticks, sizeof(int));
    
    for (int i = 0; i < cant_sticks; i++) {
        t_memory_stick_reg *stick = list_get(lista_sticks, i);
        agregar_a_paquete(paquete, &(stick->base), sizeof(int));
        agregar_a_paquete(paquete, &(stick->limite), sizeof(int));
        int ip_len = strlen(stick->ip) + 1;
        agregar_a_paquete(paquete, &ip_len, sizeof(int));
        agregar_a_paquete(paquete, stick->ip, ip_len);
        agregar_a_paquete(paquete, &(stick->puerto), sizeof(int));
    }
    
    enviar_paquete(paquete, cliente_fd);
    eliminar_paquete(paquete);
    pthread_mutex_unlock(&mutex_sticks);
}


void enviar_segmentos_actualizados(int client_fd, t_tabla_segmentos *tabla) {
    int cant_seg = list_size(tabla->segmentos);
    send(client_fd, &cant_seg, sizeof(int), 0);
    for (int i = 0; i < cant_seg; i++) {
        t_segmento_mem *seg = list_get(tabla->segmentos, i);
        send(client_fd, &(seg->id), sizeof(int), 0);
        send(client_fd, &(seg->base), sizeof(int), 0);
        send(client_fd, &(seg->limite), sizeof(int), 0);
    }
}

void procesar_mem_alloc(int client_fd) {
    int pid = recibir_entero(client_fd);
    int id_segmento = recibir_entero(client_fd);
    int tamanio = recibir_entero(client_fd);
    
    char pid_str[32];
    sprintf(pid_str, "%d", pid);
    
    pthread_mutex_lock(&mutex_memoria);
    t_tabla_segmentos *tabla = dictionary_get(tablas_segmentos_procesos, pid_str);
    if (tabla == NULL) {
        pthread_mutex_unlock(&mutex_memoria);
        log_error(logger, "No se encontró tabla de segmentos para PID %d", pid);
        int error = -1;
        send(client_fd, &error, sizeof(int), 0);
        return;
    }
    
    // Buscar hueco libre
    t_free_block *selected_block = NULL;
    int selected_index = -1;
    int size_huecos = list_size(huecos_libres);
    
    if (strcmp(estrategia_asignacion, "BEST") == 0) {
        int best_size = 999999;
        for (int i = 0; i < size_huecos; i++) {
            t_free_block *block = list_get(huecos_libres, i);
            if (block->size >= tamanio && block->size < best_size) {
                best_size = block->size;
                selected_block = block;
                selected_index = i;
            }
        }
    } else { // WORST
        int worst_size = -1;
        for (int i = 0; i < size_huecos; i++) {
            t_free_block *block = list_get(huecos_libres, i);
            if (block->size >= tamanio && block->size > worst_size) {
                worst_size = block->size;
                selected_block = block;
                selected_index = i;
            }
        }
    }
    
    if (selected_block != NULL) {
        t_segmento_mem *nuevo_seg = malloc(sizeof(t_segmento_mem));
        nuevo_seg->id = id_segmento;
        nuevo_seg->base = selected_block->base;
        nuevo_seg->limite = tamanio;
        list_add(tabla->segmentos, nuevo_seg);
        
        if (selected_block->size == tamanio) {
            list_remove_and_destroy_element(huecos_libres, selected_index, free);
        } else {
            selected_block->base += tamanio;
            selected_block->size -= tamanio;
        }
        pthread_mutex_unlock(&mutex_memoria);
        
        log_info(logger, "## PID: %d - Segmento Creado %d - Tamaño: %d", pid, id_segmento, tamanio);
        
        int ok = 1;
        send(client_fd, &ok, sizeof(int), 0);
        enviar_segmentos_actualizados(client_fd, tabla);
    } else {
        int espacio_total_libre = 0;
        for (int i = 0; i < size_huecos; i++) {
            t_free_block *block = list_get(huecos_libres, i);
            espacio_total_libre += block->size;
        }
        
        if (espacio_total_libre >= tamanio) {
            pthread_mutex_unlock(&mutex_memoria);
            
            int cop = INICIAR_COMPACTACION;
            send(client_fd, &cop, sizeof(int), 0);
            
            int ok_conf;
            recv(client_fd, &ok_conf, sizeof(int), MSG_WAITALL);
            
            procesar_compactacion();
            
            pthread_mutex_lock(&mutex_memoria);
            selected_block = NULL;
            size_huecos = list_size(huecos_libres);
            for (int i = 0; i < size_huecos; i++) {
                t_free_block *block = list_get(huecos_libres, i);
                if (block->size >= tamanio) {
                    selected_block = block;
                    selected_index = i;
                    break;
                }
            }
            
            if (selected_block != NULL) {
                t_segmento_mem *nuevo_seg = malloc(sizeof(t_segmento_mem));
                nuevo_seg->id = id_segmento;
                nuevo_seg->base = selected_block->base;
                nuevo_seg->limite = tamanio;
                list_add(tabla->segmentos, nuevo_seg);
                
                if (selected_block->size == tamanio) {
                    list_remove_and_destroy_element(huecos_libres, selected_index, free);
                } else {
                    selected_block->base += tamanio;
                    selected_block->size -= tamanio;
                }
                pthread_mutex_unlock(&mutex_memoria);
                
                log_info(logger, "## PID: %d - Segmento Creado %d - Tamaño: %d", pid, id_segmento, tamanio);
                
                int ok = 1;
                send(client_fd, &ok, sizeof(int), 0);
                enviar_segmentos_actualizados(client_fd, tabla);
            } else {
                pthread_mutex_unlock(&mutex_memoria);
                log_error(logger, "Fallo crítico: compactación no generó espacio contiguo.");
                int error = -1;
                send(client_fd, &error, sizeof(int), 0);
            }
        } else {
            pthread_mutex_unlock(&mutex_memoria);
            log_error(logger, "Espacio insuficiente para asignar %d bytes al PID %d", tamanio, pid);
            int error = -1;
            send(client_fd, &error, sizeof(int), 0);
        }
    }
}

void procesar_mem_free(int client_fd) {
    int pid = recibir_entero(client_fd);
    int id_segmento = recibir_entero(client_fd);
    
    char pid_str[32];
    sprintf(pid_str, "%d", pid);
    
    pthread_mutex_lock(&mutex_memoria);
    t_tabla_segmentos *tabla = dictionary_get(tablas_segmentos_procesos, pid_str);
    if (tabla == NULL) {
        pthread_mutex_unlock(&mutex_memoria);
        int error = -1;
        send(client_fd, &error, sizeof(int), 0);
        return;
    }
    
    int seg_index = -1;
    int size_seg = list_size(tabla->segmentos);
    t_segmento_mem *target_seg = NULL;
    for (int i = 0; i < size_seg; i++) {
        t_segmento_mem *seg = list_get(tabla->segmentos, i);
        if (seg->id == id_segmento) {
            target_seg = seg;
            seg_index = i;
            break;
        }
    }
    
    if (target_seg == NULL) {
        pthread_mutex_unlock(&mutex_memoria);
        log_error(logger, "Segmento %d no encontrado para PID %d", id_segmento, pid);
        int error = -1;
        send(client_fd, &error, sizeof(int), 0);
        return;
    }
    
    t_free_block *nuevo_hueco = malloc(sizeof(t_free_block));
    nuevo_hueco->base = target_seg->base;
    nuevo_hueco->limite = target_seg->base + target_seg->limite - 1;
    nuevo_hueco->size = target_seg->limite;
    list_add(huecos_libres, nuevo_hueco);
    
    bool comparador_huecos(void *a, void *b) {
        return ((t_free_block*)a)->base < ((t_free_block*)b)->base;
    }
    list_sort(huecos_libres, comparador_huecos);
    
    int h = 0;
    while (h < list_size(huecos_libres) - 1) {
        t_free_block *h1 = list_get(huecos_libres, h);
        t_free_block *h2 = list_get(huecos_libres, h + 1);
        if (h1->limite + 1 == h2->base) {
            h1->limite = h2->limite;
            h1->size += h2->size;
            list_remove_and_destroy_element(huecos_libres, h + 1, free);
        } else {
            h++;
        }
    }
    
    list_remove_and_destroy_element(tabla->segmentos, seg_index, free);
    pthread_mutex_unlock(&mutex_memoria);
    
    log_info(logger, "## PID: %d - Segmento Liberado %d", pid, id_segmento);
    
    int ok = 1;
    send(client_fd, &ok, sizeof(int), 0);
    enviar_segmentos_actualizados(client_fd, tabla);
}

bool stick_leer_datos(uint32_t dir_fisica, int tamanio, void *dest_buffer) {
    int bytes_leidos = 0;
    while (bytes_leidos < tamanio) {
        uint32_t curr_dir = dir_fisica + bytes_leidos;
        int rest = tamanio - bytes_leidos;
        
        t_memory_stick_reg *matching_stick = NULL;
        int size_list = list_size(lista_sticks);
        for (int i = 0; i < size_list; i++) {
            t_memory_stick_reg *stick = list_get(lista_sticks, i);
            if (curr_dir >= stick->base && curr_dir <= stick->limite) {
                matching_stick = stick;
                break;
            }
        }
        
        if (matching_stick == NULL) {
            log_error(logger, "Dirección física %u fuera de rango de los Memory Sticks", curr_dir);
            return false;
        }
        
        int limit_in_stick = matching_stick->limite - curr_dir + 1;
        int chunk_size = (rest < limit_in_stick) ? rest : limit_in_stick;
        
        int op = LECTURA_MEMORIA;
        send(matching_stick->socket, &op, sizeof(int), 0);
        send(matching_stick->socket, &curr_dir, sizeof(uint32_t), 0);
        send(matching_stick->socket, &chunk_size, sizeof(uint32_t), 0);
        
        int r = recv(matching_stick->socket, dest_buffer + bytes_leidos, chunk_size, MSG_WAITALL);
        if (r != chunk_size) {
            return false;
        }
        bytes_leidos += chunk_size;
    }
    return true;
}

bool stick_escribir_datos(uint32_t dir_fisica, int tamanio, void *src_buffer) {
    int bytes_escritos = 0;
    while (bytes_escritos < tamanio) {
        uint32_t curr_dir = dir_fisica + bytes_escritos;
        int rest = tamanio - bytes_escritos;
        
        t_memory_stick_reg *matching_stick = NULL;
        int size_list = list_size(lista_sticks);
        for (int i = 0; i < size_list; i++) {
            t_memory_stick_reg *stick = list_get(lista_sticks, i);
            if (curr_dir >= stick->base && curr_dir <= stick->limite) {
                matching_stick = stick;
                break;
            }
        }
        
        if (matching_stick == NULL) {
            log_error(logger, "Dirección física %u fuera de rango de los Memory Sticks", curr_dir);
            return false;
        }
        
        int limit_in_stick = matching_stick->limite - curr_dir + 1;
        int chunk_size = (rest < limit_in_stick) ? rest : limit_in_stick;
        
        int op = ESCRITURA_MEMORIA;
        send(matching_stick->socket, &op, sizeof(int), 0);
        send(matching_stick->socket, &curr_dir, sizeof(uint32_t), 0);
        send(matching_stick->socket, &chunk_size, sizeof(uint32_t), 0);
        send(matching_stick->socket, src_buffer + bytes_escritos, chunk_size, 0);
        
        int resultado;
        int r = recv(matching_stick->socket, &resultado, sizeof(int), MSG_WAITALL);
        if (r != sizeof(int) || resultado != 1) {
            return false;
        }
        bytes_escritos += chunk_size;
    }
    return true;
}

void procesar_compactacion() {
    log_info(logger, "## Inicio de compactación");
    
    pthread_mutex_lock(&mutex_memoria);
    pthread_mutex_lock(&mutex_sticks);
    
    t_list *todos_los_segmentos = list_create();
    
    typedef struct {
        t_segmento_mem *seg;
        int pid;
    } t_seg_ref;
    
    void collect_segs(char *key, void *value) {
        t_tabla_segmentos *tabla = (t_tabla_segmentos*)value;
        int size = list_size(tabla->segmentos);
        for (int i = 0; i < size; i++) {
            t_seg_ref *ref = malloc(sizeof(t_seg_ref));
            ref->seg = list_get(tabla->segmentos, i);
            ref->pid = tabla->pid;
            list_add(todos_los_segmentos, ref);
        }
    }
    dictionary_iterator(tablas_segmentos_procesos, collect_segs);
    
    bool comparador_segs(void *a, void *b) {
        return ((t_seg_ref*)a)->seg->base < ((t_seg_ref*)b)->seg->base;
    }
    list_sort(todos_los_segmentos, comparador_segs);
    
    int next_free_phys_addr = 0;
    int total_segs = list_size(todos_los_segmentos);
    for (int i = 0; i < total_segs; i++) {
        t_seg_ref *ref = list_get(todos_los_segmentos, i);
        t_segmento_mem *seg = ref->seg;
        
        if (seg->base != next_free_phys_addr) {
            void *data_buffer = malloc(seg->limite);
            stick_leer_datos(seg->base, seg->limite, data_buffer);
            stick_escribir_datos(next_free_phys_addr, seg->limite, data_buffer);
            free(data_buffer);
            
            // Log obligatorio para lectura/escritura en espacio de usuario
            log_info(logger, "## PID: %d - Lectura - Dir. Física: %d - Tamaño: %d", ref->pid, seg->base, seg->limite);
            log_info(logger, "## PID: %d - Escritura - Dir. Física: %d - Tamaño: %d", ref->pid, next_free_phys_addr, seg->limite);
            
            seg->base = next_free_phys_addr;
        }
        next_free_phys_addr += seg->limite;
        free(ref);
    }
    list_destroy(todos_los_segmentos);
    
    list_destroy_and_destroy_elements(huecos_libres, free);
    huecos_libres = list_create();
    
    int total_mem_size = 0;
    int cant_sticks = list_size(lista_sticks);
    if (cant_sticks > 0) {
        t_memory_stick_reg *last_stick = list_get(lista_sticks, cant_sticks - 1);
        total_mem_size = last_stick->limite + 1;
    }
    
    int free_space_left = total_mem_size - next_free_phys_addr;
    if (free_space_left > 0) {
        t_free_block *new_hueco = malloc(sizeof(t_free_block));
        new_hueco->base = next_free_phys_addr;
        new_hueco->limite = total_mem_size - 1;
        new_hueco->size = free_space_left;
        list_add(huecos_libres, new_hueco);
    }
    
    pthread_mutex_unlock(&mutex_sticks);
    pthread_mutex_unlock(&mutex_memoria);
    
    log_info(logger, "## Fin de compactación");
}

void procesar_suspender_proceso(int client_fd) {
    int pid = recibir_entero(client_fd);
    char pid_str[32];
    sprintf(pid_str, "%d", pid);
    
    pthread_mutex_lock(&mutex_memoria);
    t_tabla_segmentos *tabla = dictionary_get(tablas_segmentos_procesos, pid_str);
    if (tabla != NULL) {
        int size_seg = list_size(tabla->segmentos);
        for (int i = 0; i < size_seg; i++) {
            t_segmento_mem *seg = list_get(tabla->segmentos, i);
            
            if (seg->base != -1) {
                t_free_block *nuevo_hueco = malloc(sizeof(t_free_block));
                nuevo_hueco->base = seg->base;
                nuevo_hueco->limite = seg->base + seg->limite - 1;
                nuevo_hueco->size = seg->limite;
                list_add(huecos_libres, nuevo_hueco);
                
                log_info(logger, "## PID: %d - Segmento Liberado %d (por suspension)", pid, seg->id);
                seg->base = -1;
            }
        }
        
        bool comparador_huecos(void *a, void *b) {
            return ((t_free_block*)a)->base < ((t_free_block*)b)->base;
        }
        list_sort(huecos_libres, comparador_huecos);
        
        int h = 0;
        while (h < list_size(huecos_libres) - 1) {
            t_free_block *h1 = list_get(huecos_libres, h);
            t_free_block *h2 = list_get(huecos_libres, h + 1);
            if (h1->limite + 1 == h2->base) {
                h1->limite = h2->limite;
                h1->size += h2->size;
                list_remove_and_destroy_element(huecos_libres, h + 1, free);
            } else {
                h++;
            }
        }
    }
    pthread_mutex_unlock(&mutex_memoria);
    
    int ok = 1;
    send(client_fd, &ok, sizeof(int), 0);
}

void procesar_des_suspender_proceso(int client_fd) {
    int pid = recibir_entero(client_fd);
    char pid_str[32];
    sprintf(pid_str, "%d", pid);
    
    pthread_mutex_lock(&mutex_memoria);
    t_tabla_segmentos *tabla = dictionary_get(tablas_segmentos_procesos, pid_str);
    if (tabla == NULL) {
        pthread_mutex_unlock(&mutex_memoria);
        int fail = 0;
        send(client_fd, &fail, sizeof(int), 0);
        return;
    }
    
    t_list *huecos_temp = list_create();
    int size_huecos = list_size(huecos_libres);
    for (int i = 0; i < size_huecos; i++) {
        t_free_block *orig = list_get(huecos_libres, i);
        t_free_block *copy = malloc(sizeof(t_free_block));
        copy->base = orig->base;
        copy->limite = orig->limite;
        copy->size = orig->size;
        list_add(huecos_temp, copy);
    }
    
    bool can_allocate_all = true;
    int size_seg = list_size(tabla->segmentos);
    int *temp_bases = malloc(sizeof(int) * (size_seg > 0 ? size_seg : 1));
    
    for (int s = 0; s < size_seg; s++) {
        t_segmento_mem *seg = list_get(tabla->segmentos, s);
        int tamanio = seg->limite;
        
        t_free_block *selected_block = NULL;
        int selected_index = -1;
        int cant_temp_huecos = list_size(huecos_temp);
        
        if (strcmp(estrategia_asignacion, "BEST") == 0) {
            int best_size = 999999;
            for (int i = 0; i < cant_temp_huecos; i++) {
                t_free_block *block = list_get(huecos_temp, i);
                if (block->size >= tamanio && block->size < best_size) {
                    best_size = block->size;
                    selected_block = block;
                    selected_index = i;
                }
            }
        } else { // WORST
            int worst_size = -1;
            for (int i = 0; i < cant_temp_huecos; i++) {
                t_free_block *block = list_get(huecos_temp, i);
                if (block->size >= tamanio && block->size > worst_size) {
                    worst_size = block->size;
                    selected_block = block;
                    selected_index = i;
                }
            }
        }
        
        if (selected_block != NULL) {
            temp_bases[s] = selected_block->base;
            if (selected_block->size == tamanio) {
                list_remove_and_destroy_element(huecos_temp, selected_index, free);
            } else {
                selected_block->base += tamanio;
                selected_block->size -= tamanio;
            }
        } else {
            can_allocate_all = false;
            break;
        }
    }
    
    if (can_allocate_all) {
        list_destroy_and_destroy_elements(huecos_libres, free);
        huecos_libres = huecos_temp;
        
        for (int s = 0; s < size_seg; s++) {
            t_segmento_mem *seg = list_get(tabla->segmentos, s);
            seg->base = temp_bases[s];
            log_info(logger, "## PID: %d - Segmento Creado %d (por des-suspension) - Base: %d - Tamaño: %d", pid, seg->id, seg->base, seg->limite);
        }
        pthread_mutex_unlock(&mutex_memoria);
        
        int ok = 1;
        send(client_fd, &ok, sizeof(int), 0);
        enviar_segmentos_actualizados(client_fd, tabla);
    } else {
        list_destroy_and_destroy_elements(huecos_temp, free);
        pthread_mutex_unlock(&mutex_memoria);
        
        int fail = 0;
        send(client_fd, &fail, sizeof(int), 0);
    }
    free(temp_bases);
}

/*
//prox sacar esto de aca y ponerlo en un .h aparte
void atender_cliente_mock(int cliente_fd, t_log* logger, t_dictionary* diccionario, char* basepath);

// Hilo individual para atender a cada módulo conectado
// Lo mismo que el stick, aca solo invoco la funcion principal, el resto vuela

/*
//prox sacar esto de aca y ponerlo en un .h aparte
void atender_cliente_mock(int cliente_fd, t_log* logger, t_dictionary* diccionario, char* basepath);
void procesar_iniciar_proceso(int cliente_fd, t_log* logger, t_dictionary* diccionario, char* basepath);
void procesar_pedir_instruccion(int cliente_fd, t_log* logger, t_dictionary* diccionario);

int main(int argc, char *argv[]) {
  saludar("kernel_memory");

  t_log *logger =
      log_create("kernel_memory.log", "KERNEL_MEMORY", true, LOG_LEVEL_INFO);
  if (logger == NULL) {
    printf("No se creo el logger");
    return 1;
  }

  t_config *config = config_create("kernel_memory.config");
  if (config == NULL) {
    log_error(logger, "No se pudo encontrar el archivo kernel_memory.config");
    return 1;
  }

  // extraer valores de ip y puerto
  char *ip = config_get_string_value(config, "IP_MEMORIA");
  char *puerto = config_get_string_value(config, "PUERTO_ESCUCHA");

  // iniciar server con ip y puerto
  int server_fd = iniciar_servidor(puerto, logger);
  log_info(logger, "Kernel Memory iniciado en %s:%s. Esperando conexiones...",
           ip, puerto);

  //path base de scripts desde el config
  char* path_base_scripts = config_get_string_value(config, "SCRIPTS_BASEPATH");
  t_dictionary* diccionario_paths = dictionary_create();

  // espero clientes
  while (1) {
    int cliente_fd = esperar_cliente(server_fd);
    log_info(logger, "## Nuevo Cliente Conectado - FD del socket: %d",
             cliente_fd);
    atender_cliente_mock(cliente_fd, logger, diccionario_paths, path_base_scripts);
  }

  // libero memoria
  config_destroy(config);
  log_destroy(logger);

  return 0;
}


void atender_cliente_mock(int cliente_fd, t_log* logger, t_dictionary* diccionario, char* basepath) {
  while(1){
  
  op_code cod_op = recibir_operacion(cliente_fd);

  if (cod_op == -1) {
        log_warning(logger, "El cliente se desconectó.");
        return; // Salimos de la función
    }

    int tamaño_buffer;
    recv(cliente_fd, &tamaño_buffer, sizeof(int), MSG_WAITALL);
    
    // Si el paquete traía texto o datos extra, los sacamos del tubo también
    if (tamaño_buffer > 0) {
        void* basura = malloc(tamaño_buffer);
        recv(cliente_fd, basura, tamaño_buffer, MSG_WAITALL);
        free(basura);
    }

    switch (cod_op) {
        case INICIAR_PROCESO:
            procesar_iniciar_proceso(cliente_fd, logger, diccionario, basepath);
            break;
            
        case PEDIR_INSTRUCCION:
            //log_info(logger, "Me pidieron una instrucción. Próximamente...");
            procesar_pedir_instruccion(cliente_fd, logger, diccionario);
            break;
            
        case CONSULTAR_ESPACIO_LIBRE:
            // MOCK: decimos que hay lugar infinito para avanzar
            uint32_t espacio_falso = 999999;
            send(cliente_fd, &espacio_falso, sizeof(uint32_t), 0);
            log_info(logger, "Le mentí al Scheduler: le dije que tengo %d de espacio", espacio_falso);
            break;

        case LEER_MEMORIA_MOCK:
        case ESCRIBIR_MEMORIA_MOCK:
            // MOCK: Le decimos a la CPU que la operación fue un éxito sin hacer nada
            int ok = 1;
            send(cliente_fd, &ok, sizeof(int), 0);
            log_info(logger, "Simulando éxito en lectura/escritura de memoria.");
            break;

        default:
            log_warning(logger, "Operación desconocida: %d", cod_op);
            break;
    }
  }
}

void procesar_iniciar_proceso(int cliente_fd, t_log* logger, t_dictionary* diccionario, char* basepath) {
   int pid_recibido = 1;
  char* nombre_archivo = "proceso1.txt"; // MOCK: nombre de archivo fijo
  char* ruta_completa = string_from_format("%s/%s", basepath, nombre_archivo);
  FILE* archivo = fopen(ruta_completa, "r");
  if (archivo == NULL) {
      log_error(logger, "No se pudo abrir el archivo %s", ruta_completa);
      free(ruta_completa);
      return;
  }
  fseek(archivo, 0, SEEK_END);
  long fsize = ftell(archivo);
  fseek(archivo, 0, SEEK_SET);
  char* contenido = malloc(fsize + 1);
  fread(contenido, fsize, 1, archivo);
  fclose(archivo);
  contenido[fsize] = 0; // Agregamos el fin de cadena
  char** array_instrucciones = string_split(contenido, "\n");

  char* pid_string = string_itoa(pid_recibido);
  dictionary_put(diccionario, pid_string, array_instrucciones);

  log_info(logger, "## PID: %d - Proceso Creado", pid_recibido);

  // Limpiamos memoria auxiliar
  free(ruta_completa);
  free(contenido);
  free(pid_string);
}

void procesar_pedir_instruccion(int cliente_fd, t_log* logger, t_dictionary* diccionario) {
    int pid_recibido = 1;
    uint32_t pc_recibido = 0;

    char* pid_string = string_itoa(pid_recibido);
    char** array_instrucciones = dictionary_get(diccionario, pid_string);

    if (array_instrucciones != NULL) {
        char* instruccion = array_instrucciones[pc_recibido];

        // Log obligatorio 
        log_info(logger, "## PID: %d - Obtener instrucción: %d - Instrucción: %s", pid_recibido, pc_recibido, instruccion);

        // Armado del paquete y envío
        int tamaño_texto = strlen(instruccion) + 1;
        t_paquete* paquete = crear_paquete();
        paquete->cop = MENSAJE;
        
        paquete->buffer->size = tamaño_texto;
        paquete->buffer->stream = malloc(paquete->buffer->size);
        memcpy(paquete->buffer->stream, instruccion, paquete->buffer->size);
        
        enviar_paquete(paquete, cliente_fd);
        eliminar_paquete(paquete);
    } else {
        log_error(logger, "No se encontraron instrucciones para el PID %d", pid_recibido);
    }

    free(pid_string);
}
*/
