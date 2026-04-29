#include "scheduler-funciones.h"

// --- Definición de variables globales --- //
t_log *logger_server;
int generador_pid = 1;

t_queue *cola_new;
t_queue *cola_ready;
t_queue *cola_block;
t_queue *cola_exit;

pthread_mutex_t mutex_new;
pthread_mutex_t mutex_ready;
pthread_mutex_t mutex_block;
pthread_mutex_t mutex_exit;

sem_t sem_procesos_en_new;

// ================ FUNCIONES ================ //
void crear_proceso() {
  
  t_pcb *nuevo_pcb = malloc(sizeof(t_pcb));
  nuevo_pcb->pid = generador_pid++;
  nuevo_pcb->program_counter = 0;
  nuevo_pcb->estado = ESTADO_NEW;

  pthread_mutex_lock(&mutex_new);
  queue_push(cola_new, nuevo_pcb);
  pthread_mutex_unlock(&mutex_new);

  log_info(logger_server, "Se creo el proceso con PID %d, y se encoló en NEW", nuevo_pcb->pid);
  sem_post(&sem_procesos_en_new); // habilita el planificador
}

sem_t sem_grado_multiprogramacion;
sem_t sem_procesos_en_ready;

void *planificador_largo_plazo(void *arg){
    while (1){
        sem_wait(&sem_procesos_en_new); // espera hasta q haya algun proceso en NEW
        sem_wait(&sem_grado_multiprogramacion); // espera hasta  que haya lugar en el sistema

        pthread_mutex_lock(&mutex_new);
        t_pcb *pcb = queue_pop(cola_new);
        pthread_mutex_unlock(&mutex_new);

        pcb->estado = ESTADO_READY;

        pthread_mutex_lock(&mutex_ready);
        queue_push(cola_ready, pcb);
        pthread_mutex_unlock(&mutex_ready);

        log_info(logger_server, "## PID: %d - Estado Anterior: NEW - Estado Actual: READY", pcb->pid);
        
        sem_post(&sem_procesos_en_ready); // notifica que hay alguien en READY
    }
    return NULL;
}
// =========================================== //
