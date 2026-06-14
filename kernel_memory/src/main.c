#include <commons/config.h>
#include <commons/log.h>
#include <commons/string.h>
#include <commons/collections/dictionary.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <utils/hello.h>
#include <utils/utils.h>

t_log *logger;
t_dictionary *diccionario_instrucciones;
char *path_base_scripts;

// Firmas de funciones
void *atender_clientes(void *arg);
void procesar_iniciar_proceso(int cliente_fd);
void procesar_pedir_instruccion(int cliente_fd);

int main(int argc, char *argv[]) {
    saludar("kernel_memory");
    
    logger = log_create("kernel_memory.log", "KERNEL_MEMORY", true, LOG_LEVEL_INFO);
    if (logger == NULL) {
        printf("No se creo el logger\n");
        return 1;
    }

    t_config *config = congfig_create("kernel_memory.config");
    if (config == NULL) {
        log_error(logger, "No se pudo encontrar el archivo kernel_memory.config");
        return 1;
    }

    char *ip = config_get_string_value(config, "IP_MEMORIA");
    char *puerto = config_get_string_value(config, "PUERTO_ESCUCHA");
    path_base_scripts = config_get_string_value(config, "SCRIPTS_BASEPATH");

    // guardo las lineas asociadas al PID del txt
    diccionario_instrucciones = dictionary_create();

    int server_fd = iniciar_servidor(ip, puerto);
    log_info(logger, "Kernel Memory iniciado en %s:%s. Esperando conexiones...", ip, puerto);

    // ciclo multihilo 
    while(1){
        int cliente_fd = esperar_cliente(server_fd);
        if (cliente_fd != -1) {
            log_info(logger, "## Nuevo Cliente Conectado - FD del socket: %d", cliente_fd);
            
            pthread_t hilo_cliente;
            int *socket_hilo= malloc(sizeof(int));
            *socket_hilo = cliente_fd;

            pthread_create(&hiloo_cliente, NULL, atender_cliente, socket_hilo);
            pthread_detach(hilo_cliente);
        }
    }

    config_destroy(config);
    log_destroy(logger);
    dictionary_destroy(diccionario_instrucciones);
    return 0;
}

void *atender_cliente(void *arg) {
    int cliente_fd = *int*) arg;
    free(arg);

    while(1)) {
        int cod_op = recibir_operacion(cliente_fd);
        if (cod_op == -1) {
            log_warning(logger, "El cliente con FD %d se desconectó.", cliente_fd);
            liberar_conexion(cliente_fd);
            break;             
    }

    switch (cod_op) {
            case INICIAR_PROCESO:
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
            case LEER_MEMORIA:
            case ESCRIBIR_MEMORIA: {
                // Mockeo de lectura
                int ok = 1;
                send(cliente_fd, &ok, sizeof(int), 0);
                log_info(logger, "Simulando éxito en lectura/escritura de memoria (MOCK).");
                break;
            }

            default:
                log_warning(logger, "Operación desconocida o no implementada: %d", cod_op);
                break;
        } 
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
