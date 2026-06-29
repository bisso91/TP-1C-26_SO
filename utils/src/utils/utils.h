
#ifndef UTILS_H_
#define UTILS_H_

#include <commons/log.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

typedef enum {
  // Handshake inicial
  HANDSHAKE_CPU,

  // Kernel Scheduler -> CPU
  EJECUTAR_PROCESO, // "Tomá este contexto y empezá el ciclo"
  INTERRUPCION,     // "Che, pará todo que hay que desalojar"

  // CPU -> Kernel Scheduler
  FIN_PROCESO,  // "Terminé la última instrucción (EXIT)"
  SOLICITUD_IO, // "El proceso pidió usar un dispositivo"

  // CPU -> Kernel Memory
  FETCH_INSTRUCCION, // "Dame lo que hay en el PC actual"
  LECTURA_MEMORIA,   // "Necesito el valor de esta dirección"
  ESCRITURA_MEMORIA,  // "Guardá este valor en esta dirección"

  // --- COSAS DE IO --- //
  MENSAJE,
  PAQUETE,
  IO_GENERICA,//reemplaza a: IO_SLEEP, IO_STDIN, IO_STDOUT
  WAIT_RECURSO, // opcode de semaforos  usados para sincronización
  SIGNAL_RECURSO, // opcode de semaforos usados para sincronización
  DISPATCH_PCB,
  IDENTIFICACION_IO,
  SEGMENTATION_FAULT,
  IDENTIFICACION_CPU_DISPATCH,
  IDENTIFICACION_CPU_INTERRUPT,
  IO_SLEEP,
  IO_STDIN,
  IO_STDOUT,
  FIN_IO,
  FIN_QUANTUM,
  INTERRUPCION_QUANTUM,
  INIT_PROC,

  // kernel memory
  INICIAR_PROCESO,
  PEDIR_INSTRUCCION,
  CONSULTAR_ESPACIO_LIBRE,
  LEER_MEMORIA,
  ESCRIBIR_MEMORIA,
  LEER_BLOQUE_STICK,
  ESCRIBIR_BLOQUE_STICK,
  IDENTIFICACION_STICK,
  IDENTIFICACION_CPU,
  OBTENER_STICKS,
  INICIAR_COMPACTACION,
  CONFIRMAR_DESALOJO,
  MEM_ALLOC,
  MEM_FREE,
  INTERRUPCION_DESALOJO,
  STICK_DESCONECTADO,
  SUSPENDER_PROCESO,
  DES_SUSPENDER_PROCESO
} op_code;

// fn server
int iniciar_servidor(char *ip, char *puerto); // <-- CORREGIDO PARA MATCHEAR EL C
int esperar_cliente(int socket_servidor);

// fn client
int crear_conexion(char *ip, char *puerto);
void liberar_conexion(int socket_cliente);

// --- ESTADOS DE UN PROCESO --- //
typedef enum{
    ESTADO_NEW,
    ESTADO_READY,
    ESTADO_EXEC,
    ESTADO_BLOCK,
    ESTADO_SUS_READY,
    ESTADO_SUS_BLOCK,
    ESTADO_EXIT
} t_estado;

typedef struct {
    int id;
    int base;
    int limite;
} t_segmento;

// --- PROCESS CONTROL BLOCK (PCB)--- //
typedef struct{
    int pid;
    int program_counter;
    t_estado estado;
    int cantidad_segmentos;
    t_segmento *tabla_segmentos;
    int prioridad_actual;
    int prioridad_original;
    // agregar registro cpu y otros
} t_pcb;

typedef struct {
  int size;
  void *stream;
} t_buffer;

typedef struct {
  op_code cop;
  t_buffer *buffer;
} t_paquete;

// fn' asociadas a paquetes y sockets

void enviar_entero(int socket_cliente, int numero);
int recibir_entero(int socket_cliente);

// fn's de paquetes
void *serializar_paquete(t_paquete *paquete, int bytes); // la pongo arriba de enviar mensaje para que la detecte antes de usarla
void enviar_mensaje(char *mensaje, int socket_cliente);
void crear_buffer(t_paquete *paquete);
t_paquete *crear_paquete(void);
void agregar_a_paquete(t_paquete *paquete, void *valor, int size);
void enviar_paquete(t_paquete *paquete, int socket_cliente);
void eliminar_paquete(t_paquete *paquete);
int recibir_operacion(int socket_cliente);
void *recibir_buffer(int *size, int socket_cliente);
void recibir_mensaje(int socket_cliente, t_log *logger);
void enviar_string(char *mensaje, int socket_cliente, op_code cop);

// ---------------------------------------------------- //

void enviar_pcb(t_pcb *pcb, int socket_cliente, int op_code);
t_pcb *recibir_pcb(int socket_cliente);

char* recibir_string(int socket_cliente);

#endif /* UTILS_H_ */