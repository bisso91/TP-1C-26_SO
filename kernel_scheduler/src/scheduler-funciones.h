#ifndef SCHEDULER_FUNCIONES_H_
#define SCHEDULER_FUNCIONES_H_

#include <commons/collections/queue.h>
#include <commons/log.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <utils/utils.h>

// --- VARIABLES GLOBALES (compartidas) --- //
extern t_log *logger_server;

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
// --------------------------------- //


// --- socket --- //
extern int socket_cpu_dispatch;

// --------------------------------- //


// --- PROTOTIPOS DE FUNCIONES --- //
void crear_proceso();
void *planificador_largo_plazo(void *arg);
void *planificador_corto_plazo_fifo(void *arg);

#endif