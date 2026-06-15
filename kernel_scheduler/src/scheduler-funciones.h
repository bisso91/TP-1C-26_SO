#ifndef SCHEDULER_FUNCIONES_H_
#define SCHEDULER_FUNCIONES_H_

#include <commons/collections/queue.h>
#include <commons/log.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <utils/utils.h>
#include <commons/collections/dictionary.h>

typedef struct{
    int instancias;
    t_queue *cola_bloqueados; // cada instancia tiene su cola
    pthread_mutex_t mutex_recurso; // protege la instancia y las colas
} t_recurso;

// --- VARIABLES GLOBALES (compartidas) --- //
extern t_log *logger_server;
extern char *algoritmo_de_planificacion;
extern int quantum;
extern t_dictionary *recursos_sistema;

// --- files descriptors de CPU (revisar si son globales) preguntar a fede --- //
extern int socket_cpu_dispatch;
extern int socket_cpu_interrupt;

// --- asigna el pid --- //
extern int generador_pid;

// --- colas de planificación --- //
extern t_queue *cola_new;
extern t_queue *cola_ready;
extern t_queue *cola_block;
extern t_queue *cola_exit;
// ------------------------------ //

// --- mutex para proteger colas --- //
extern pthread_mutex_t mutex_new;
extern pthread_mutex_t mutex_ready;
extern pthread_mutex_t mutex_block;
extern pthread_mutex_t mutex_exit;
// --------------------------------- //

// --- semáforos --- //
extern sem_t sem_procesos_en_new;
extern sem_t sem_grado_multiprogramacion;
extern sem_t sem_procesos_en_ready;
extern sem_t sem_cpu_libre; 
// --------------------------------- //

// --- Diccionario para guardar los sockets de IO --- //
extern t_dictionary *interfaces_io;
extern pthread_mutex_t mutex_interfaces_io;



// --- socket --- //
extern int socket_cpu_dispatch;
extern int socket_cpu_interrupt;
extern int socket_kernel_memory;
extern int active_stdin_pid;
extern int active_stdout_pid;

// --------------------------------- //


// --- PROTOTIPOS DE FUNCIONES --- //
void crear_proceso();
void *planificador_largo_plazo(void *arg);
void *planificador_corto_plazo_fifo(void *arg);
void finalizar_proceso(t_pcb *pcb, char * motivo);

// --- FUNCIONES PARA MANEJO DE IO Y BLOQUEDOS ---//
void bloquear_proceso_por_io(t_pcb *pcb, char *nombre_syscall);
void desbloquear_proceso_de_io(t_pcb *pcb);

// --- FUNCIONES MANEJO DE RECURSOS --- //
void inicializar_recursos(char* *nombres, char* *instancias);
void solicitar_recurso_wait(t_pcb *pcb, char *nombre_recurso, int cliente_fd);
void liberar_recurso_signal(t_pcb *pcb, char *nombres_recurso, int cliente_fd);

void *temporizador_quantum(void *arg);
void *planificador_corto_plazo_rr(void *arg);

t_pcb *sacar_de_cola_block(int pid);

#endif