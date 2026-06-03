#include "scheduler-funciones.h"
#include <commons/collections/queue.h>
#include <commons/log.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>

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

t_dictionary *interfaces_io;
pthread_mutex_t mutex_interfaces_io;

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

void *planificador_corto_plazo_fifo(void *arg){
    while (1){
        sem_wait(&sem_procesos_en_ready);

        pthread_mutex_lock(&mutex_ready);
        t_pcb *pcb_a_ejecutar = queue_pop(cola_ready);
        pthread_mutex_unlock(&mutex_ready);
        
        pcb_a_ejecutar->estado = ESTADO_EXEC;

        log_info(logger_server, "## PID: %d - Estado Anterior: READY - Estado Actual: EXEC", pcb_a_ejecutar->pid);

        if(socket_cpu_dispatch != -1){
            log_info(logger_server, "Despachando proceso PID: %d a la CPU...", pcb_a_ejecutar->pid);
            enviar_pcb(pcb_a_ejecutar, socket_cpu_dispatch, DISPATCH_PCB);
            //enviar_pcb(pcb_a_ejecutar, socket_cpu_dispatch, op_code);
        } else {
            log_error(logger_server, "Error: La CPU no esta conectada aún");
        }

        log_info(logger_server, "Despachando proceso PID: %d a la CPU...", pcb_a_ejecutar->pid);    
    }
    //armar paquete y enviarlo por el socket dispatch al cpu

    return NULL;
};

void bloquear_proceso_por_io(t_pcb *pcb, char *nombre_syscall){
    // logs obligatorios
    log_info(logger_server, "## (%d) Solicitó syscall: %s", pcb->pid, nombre_syscall);
    log_info(logger_server, "## (%d) Pasa del estado EXECUTE al estado BLOCKED", pcb->pid);

    pcb->estado = ESTADO_BLOCK;

    pthread_mutex_lock(&mutex_block);
    queue_push(cola_block, pcb);
    pthread_mutex_unlock(&mutex_block);
}

void desbloquear_proceso_de_io(t_pcb *pcb){
    // logs obligatorios
    log_info(logger_server, "## (%d) finalizó IO y pasa a READY / SUSPENDED READY", pcb->pid);
    log_info(logger_server, "## (%d) Pasa del estado BLOCKED al estado READY", pcb->pid);

    pcb->estado = ESTADO_READY;

    pthread_mutex_lock(&mutex_ready);
    queue_push(cola_ready, pcb);
    pthread_mutex_unlock(&mutex_ready);

    sem_post(&sem_procesos_en_ready); //notifica al PCP que hay un proceso disponible
}

void finalizar_proceso(t_pcb *pcb, char *motivo){
    // cambiar estado, si ya tenia uno (revisar que pasa cuando no trae uno)
    pcb->estado = ESTADO_EXIT;

    // proteger la cola
    pthread_mutex_lock(&mutex_exit);
    queue_push(cola_exit, pcb);
    pthread_mutex_unlock(&mutex_exit);

    // log obligatorio
    log_info(logger_server, "## (%d) finalizó su ejecución con motivo de %s", pcb->pid, motivo);

    sem_post(&sem_grado_multiprogramacion);

}

t_pcb *sacar_de_cola_block(int pid){
    t_pcb *pcb_encontrado = NULL;

    pthread_mutex_lock(&mutex_block);

    // recorro la cola buscando el PID
    int size = queue_size(cola_block);
    for (int i = 0; i < size; i++) {
        t_pcb *pcb_aux = queue_pop(cola_block);

        if (pcb_aux->pid == pid) {
            pcb_encontrado = pcb_aux;
        } else {
            queue_push(cola_block, pcb_aux);
        }
    }
    pthread_mutex_unlock(&mutex_block);
    return pcb_encontrado;
}

// --- LOGICA RR Y TEMPORIZADOR --- //
void *temporizador_quantum(void *arg){
    int pid_en_ejecucion = *(int*)arg;
    free(arg);

    usleep(quantum * 1000);

    if(socket_cpu_interrupt != -1){
        int op_code = INTERRUPCION_QUANTUM;
        send(socket_cpu_interrupt, &op_code, sizeof(int), 0);
        enviar_entero(socket_cpu_interrupt, pid_en_ejecucion);
        log_info(logger_server, "Interrupcion de quantum enviada para PID %d", pid_en_ejecucion);

        // NOTA: Si el proceso hizo una I/O ANTES de que termine este hilo, 
        // la CPU deberá ignorar esta interrupción leyendo el PID.
        return NULL;
    }
    return NULL;
}

void *planificador_corto_plazo_rr(void *arg){
    while(1){
        sem_wait(&sem_procesos_en_ready);

        pthread_mutex_lock(&mutex_ready);
        t_pcb *pcb_a_ejecutar = queue_pop(cola_ready);
        pthread_mutex_unlock(&mutex_ready);

        pcb_a_ejecutar->estado = ESTADO_EXEC;
        log_info(logger_server, "## (PPID %d) Pasa a estada EXEC (round robin)", pcb_a_ejecutar->pid);

        pthread_t hilo_timer;
        int *pid_prt = malloc(sizeof(int));
        *pid_prt = pcb_a_ejecutar->pid;
        pthread_create(&hilo_timer, NULL, temporizador_quantum, pid_prt);
        pthread_detach(hilo_timer);

        // Enviamos el PCB a la CPU para que trabaje
        // enviar_pcb(pcb_a_ejecutar, socket_cpu_dispatch, EJECUTAR_PROCESO);
    }
    return NULL;
}

// --- MANEJO DE RECURSOS COMPARTIDOS (MUTEX) --- //

void inicializar_recursos(char **nombres, char **instancias){
    recursos_sistema = dictionary_create();

    for (int i = 0; nombres[1] != NULL; i++){
        t_recurso * recurso_nuevo = malloc(sizeof(t_recurso));
        recurso_nuevo->instancias = atoi(instancias[i]);
        recurso_nuevo->cola_bloqueados = queue_create();
        pthread_mutex_init(&(recurso_nuevo->mutex_recurso), NULL);

        dictionary_put(recursos_sistema, nombres[i], recurso_nuevo);
        log_info(logger_server, "Recurso inicializado: %s con %d instancias.", nombres[i], recurso_nuevo->instancias);
    }
}

void solicitar_recurso_wait(t_pcb *pcb, char *nombre_recurso, int cliente_fd){
    t_recurso *recurso = dictionary_get(recursos_sistema, nombre_recurso);

    if (recurso == NULL){
        log_error(logger_server, "El recurso %s no existe. Abortando PID %d", nombre_recurso, pcb->pid);
        // cambiar estado a EXIT y liberar
        return;
    }

    pthread_mutex_lock(&(recurso->mutex_recurso));
    recurso->instancias--;

    if(recurso->instancias < 0){
        // si no hay instancias: va a la cola del mutex, no a la de IO
        log_info(logger_server, "## (PID: %d) Bloqueado por espera de recurso %s", pcb->pid, nombre_recurso);
        pcb->estado = ESTADO_BLOCK;
        queue_push(recurso->cola_bloqueados, pcb);
    } else {
        // si hay instancias: se le da el permiso y la CPU puede continuar
        log_info(logger_server, "## (PID: %d) Asignado al recurso %s. Continúa ejecutando.", pcb->pid, nombre_recurso);
        //devuelvo pcb y un OK a la cpu para que retome elciclo de instruccion
        //enviar_pcb(pcb, cliente_fd, WAIT_OK);
    }
    pthread_mutex_unlock(&(recurso->mutex_recurso));
}
void liberar_recurso_signal(t_pcb *pcb, char *nombre_recurso, int cliente_fd){
    t_recurso *recurso = dictionary_get(recursos_sistema, nombre_recurso);

    if(recurso == NULL){
        log_error(logger_server, "El recurso %s no existe", nombre_recurso);
        return;
    }

    pthread_mutex_lock(&(recurso->mutex_recurso));
    recurso->instancias++;

    //si hay algun proceso  esperandoo eeste recurso lo despierto
    if (queue_size(recurso->cola_bloqueados) > 0){
        t_pcb *pcb_desbloqueado = queue_pop(recurso->cola_bloqueados);

        log_info(logger_server, "## (PID: %d) Desbloqueado del recruso %s. Pasando a READY.", pcb_desbloqueado->pid, nombre_recurso);

        pcb_desbloqueado->estado = ESTADO_READY;

        pthread_mutex_lock(&mutex_ready);
        queue_push(cola_ready, pcb_desbloqueado);
        pthread_mutex_unlock(&mutex_ready);
    }
    pthread_mutex_unlock(&(recurso->mutex_recurso));

    // El proceso actual (el que hizo Signal) retiene la CPU y sigue ejecutando
    // enviar_pcb(pcb, cliente_fd, SIGNAL_OK);
}
// =========================================== //
