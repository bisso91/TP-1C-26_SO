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
sem_t sem_cpu_libre;

t_dictionary *interfaces_io;
pthread_mutex_t mutex_interfaces_io;

t_list *nombres_recursos_global;
t_dictionary *prioridades_procesos;
pthread_mutex_t mutex_prioridades;

// ================ FUNCIONES ================ //
void crear_proceso() {
  
  t_pcb *nuevo_pcb = malloc(sizeof(t_pcb));
  nuevo_pcb->pid = generador_pid++;
  nuevo_pcb->program_counter = 0;
  nuevo_pcb->estado = ESTADO_NEW;
  nuevo_pcb->cantidad_segmentos = 0;
  nuevo_pcb->tabla_segmentos = NULL;

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
        while(socket_cpu_dispatch == -1) {
            usleep(100 * 1000);
        }
        sem_wait(&sem_cpu_libre); // Espera a que la CPU esté libre
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

    eliminar_prioridad(pcb->pid);

    sem_post(&sem_grado_multiprogramacion);
    sem_post(&sem_cpu_libre);
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
        while(socket_cpu_dispatch == -1) {
            usleep(100 * 1000);
        }
        sem_wait(&sem_cpu_libre); // Espera a que la CPU esté libre
        sem_wait(&sem_procesos_en_ready);

        pthread_mutex_lock(&mutex_ready);
        t_pcb *pcb_a_ejecutar = queue_pop(cola_ready);
        pthread_mutex_unlock(&mutex_ready);

        pcb_a_ejecutar->estado = ESTADO_EXEC;
        log_info(logger_server, "## (PID %d) Pasa a estado EXEC (round robin)", pcb_a_ejecutar->pid);

        pthread_t hilo_timer;
        int *pid_prt = malloc(sizeof(int));
        *pid_prt = pcb_a_ejecutar->pid;
        pthread_create(&hilo_timer, NULL, temporizador_quantum, pid_prt);
        pthread_detach(hilo_timer);

        // Enviamos el PCB a la CPU para que trabaje
        if(socket_cpu_dispatch != -1){
            log_info(logger_server, "Despachando proceso PID: %d a la CPU...", pcb_a_ejecutar->pid);
            enviar_pcb(pcb_a_ejecutar, socket_cpu_dispatch, DISPATCH_PCB);
        } else {
            log_error(logger_server, "Error: La CPU no esta conectada aún");
        }
    }
    return NULL;
}

// --- MANEJO DE RECURSOS COMPARTIDOS (MUTEX) --- //

void inicializar_recursos(char **nombres, char **instancias){
    recursos_sistema = dictionary_create();

    for (int i = 0; nombres[i] != NULL; i++){
        t_recurso * recurso_nuevo = malloc(sizeof(t_recurso));
        recurso_nuevo->instancias = atoi(instancias[i]);
        recurso_nuevo->cola_bloqueados = queue_create();
        recurso_nuevo->pid_dueno = -1; // -1 indica libre
        pthread_mutex_init(&(recurso_nuevo->mutex_recurso), NULL);

        dictionary_put(recursos_sistema, nombres[i], recurso_nuevo);
        list_add(nombres_recursos_global, string_duplicate(nombres[i]));
        log_info(logger_server, "Recurso inicializado: %s con %d instancias.", nombres[i], recurso_nuevo->instancias);
    }
}

void solicitar_recurso_wait(t_pcb *pcb, char *nombre_recurso, int cliente_fd){
    t_recurso *recurso = dictionary_get(recursos_sistema, nombre_recurso);

    if (recurso == NULL){
        // Si no existe el recurso lo creamos dinámicamente como un Mutex (1 instancia)
        recurso = malloc(sizeof(t_recurso));
        recurso->instancias = 1;
        recurso->cola_bloqueados = queue_create();
        recurso->pid_dueno = -1;
        pthread_mutex_init(&(recurso->mutex_recurso), NULL);

        dictionary_put(recursos_sistema, nombre_recurso, recurso);
        list_add(nombres_recursos_global, string_duplicate(nombre_recurso));
        log_info(logger_server, "Recurso %s creado dinámicamente con %d instancias.", nombre_recurso, recurso->instancias);
    }

    pthread_mutex_lock(&(recurso->mutex_recurso));
    recurso->instancias--;

    if(recurso->instancias < 0){
        log_info(logger_server, "## (PID: %d) Bloqueado por espera de recurso %s", pcb->pid, nombre_recurso);
        pcb->estado = ESTADO_BLOCK;
        queue_push(recurso->cola_bloqueados, pcb);

        if (recurso->pid_dueno != -1) {
            aplicar_herencia_prioridad(pcb->pid, recurso->pid_dueno, recurso);
        }
    } else {
        log_info(logger_server, "## (PID: %d) Asignado al recurso %s. Continúa ejecutando.", pcb->pid, nombre_recurso);
        recurso->pid_dueno = pcb->pid;
        log_info(logger_server, "## (%d) Toma el Mutex %s", pcb->pid, nombre_recurso);

        pcb->estado = ESTADO_READY;
        pthread_mutex_lock(&mutex_ready);
        queue_push(cola_ready, pcb);
        pthread_mutex_unlock(&mutex_ready);
        sem_post(&sem_procesos_en_ready);
    }
    pthread_mutex_unlock(&(recurso->mutex_recurso));

    sem_post(&sem_cpu_libre);
}

void liberar_recurso_signal(t_pcb *pcb, char *nombre_recurso, int cliente_fd){
    t_recurso *recurso = dictionary_get(recursos_sistema, nombre_recurso);

    if(recurso == NULL){
        log_error(logger_server, "El recurso %s no existe", nombre_recurso);
        pcb->estado = ESTADO_READY;
        pthread_mutex_lock(&mutex_ready);
        queue_push(cola_ready, pcb);
        pthread_mutex_unlock(&mutex_ready);
        sem_post(&sem_procesos_en_ready);
        sem_post(&sem_cpu_libre);
        return;
    }

    pthread_mutex_lock(&(recurso->mutex_recurso));

    // Log mutex liberado obligatorio
    log_info(logger_server, "## (%d) Libera el Mutex %s", pcb->pid, nombre_recurso);

    recurso->instancias++;

    if (queue_size(recurso->cola_bloqueados) > 0){
        t_pcb *pcb_desbloqueado = queue_pop(recurso->cola_bloqueados);
        log_info(logger_server, "## (PID: %d) Desbloqueado del recurso %s. Pasando a READY.", pcb_desbloqueado->pid, nombre_recurso);

        recurso->pid_dueno = pcb_desbloqueado->pid;
        log_info(logger_server, "## (%d) Toma el Mutex %s", pcb_desbloqueado->pid, nombre_recurso);

        pcb_desbloqueado->estado = ESTADO_READY;
        pthread_mutex_lock(&mutex_ready);
        queue_push(cola_ready, pcb_desbloqueado);
        pthread_mutex_unlock(&mutex_ready);
        sem_post(&sem_procesos_en_ready);
    } else {
        recurso->pid_dueno = -1;
    }

    recalcular_prioridad(pcb);

    pthread_mutex_unlock(&(recurso->mutex_recurso));

    pcb->estado = ESTADO_READY;
    pthread_mutex_lock(&mutex_ready);
    queue_push(cola_ready, pcb);
    pthread_mutex_unlock(&mutex_ready);
    sem_post(&sem_procesos_en_ready);

    sem_post(&sem_cpu_libre);
}

// --- FUNCIONES GESTION DE PRIORIDADES E INVERSION --- //

void registrar_prioridad(int pid, int prioridad) {
    t_proceso_prioridad *p_prio = malloc(sizeof(t_proceso_prioridad));
    p_prio->pid = pid;
    p_prio->prioridad_original = prioridad;
    p_prio->prioridad_actual = prioridad;

    char pid_str[32];
    sprintf(pid_str, "%d", pid);

    pthread_mutex_lock(&mutex_prioridades);
    dictionary_put(prioridades_procesos, pid_str, p_prio);
    pthread_mutex_unlock(&mutex_prioridades);
}

void eliminar_prioridad(int pid) {
    char pid_str[32];
    sprintf(pid_str, "%d", pid);

    pthread_mutex_lock(&mutex_prioridades);
    t_proceso_prioridad *p_prio = dictionary_remove(prioridades_procesos, pid_str);
    if (p_prio != NULL) {
        free(p_prio);
    }
    pthread_mutex_unlock(&mutex_prioridades);
}

void actualizar_prioridades_pcb(t_pcb *pcb) {
    char pid_str[32];
    sprintf(pid_str, "%d", pcb->pid);

    pthread_mutex_lock(&mutex_prioridades);
    t_proceso_prioridad *p_prio = dictionary_get(prioridades_procesos, pid_str);
    if (p_prio != NULL) {
        pcb->prioridad_actual = p_prio->prioridad_actual;
        pcb->prioridad_original = p_prio->prioridad_original;
    }
    pthread_mutex_unlock(&mutex_prioridades);
}

int obtener_max_prioridad_bloqueados(t_recurso *recurso) {
    int max_prio = 99999; // Representa la menor prioridad (mayor valor numérico)
    t_list *elements = recurso->cola_bloqueados->elements;
    int size = list_size(elements);
    for (int i = 0; i < size; i++) {
        t_pcb *p = list_get(elements, i);
        if (p->prioridad_actual < max_prio) {
            max_prio = p->prioridad_actual;
        }
    }
    return max_prio;
}

void recalcular_prioridad(t_pcb *pcb) {
    char pid_str[32];
    sprintf(pid_str, "%d", pcb->pid);

    pthread_mutex_lock(&mutex_prioridades);
    t_proceso_prioridad *p_prio = dictionary_get(prioridades_procesos, pid_str);
    if (p_prio == NULL) {
        pthread_mutex_unlock(&mutex_prioridades);
        return;
    }

    int prio_original = p_prio->prioridad_original;
    int prio_nueva = prio_original;

    int size = list_size(nombres_recursos_global);
    for (int i = 0; i < size; i++) {
        char *nombre = list_get(nombres_recursos_global, i);
        t_recurso *r = dictionary_get(recursos_sistema, nombre);
        if (r != NULL && r->pid_dueno == pcb->pid) {
            int max_prio_bloq = obtener_max_prioridad_bloqueados(r);
            if (max_prio_bloq < prio_nueva) {
                prio_nueva = max_prio_bloq;
            }
        }
    }

    if (p_prio->prioridad_actual != prio_nueva) {
        int anterior = p_prio->prioridad_actual;
        p_prio->prioridad_actual = prio_nueva;
        log_info(logger_server, "## %d Cambio de prioridad: %d - %d", pcb->pid, anterior, prio_nueva);
        pcb->prioridad_actual = prio_nueva;
    }
    pthread_mutex_unlock(&mutex_prioridades);
}

void aplicar_herencia_prioridad(int pid_esperando, int pid_dueno, t_recurso *recurso_bloqueante) {
    char waiting_str[32];
    char owner_str[32];
    sprintf(waiting_str, "%d", pid_esperando);
    sprintf(owner_str, "%d", pid_dueno);

    pthread_mutex_lock(&mutex_prioridades);
    t_proceso_prioridad *p_waiting = dictionary_get(prioridades_procesos, waiting_str);
    t_proceso_prioridad *p_owner = dictionary_get(prioridades_procesos, owner_str);

    if (p_waiting != NULL && p_owner != NULL) {
        if (p_waiting->prioridad_actual < p_owner->prioridad_actual) {
            int anterior = p_owner->prioridad_actual;
            p_owner->prioridad_actual = p_waiting->prioridad_actual;
            log_info(logger_server, "## %d Cambio de prioridad: %d - %d", pid_dueno, anterior, p_owner->prioridad_actual);
            pthread_mutex_unlock(&mutex_prioridades);

            // Propagamos la herencia de prioridad recursivamente al PCB en la cola
            t_recurso *next_recurso = NULL;
            int size = list_size(nombres_recursos_global);
            for (int i = 0; i < size; i++) {
                char *nombre = list_get(nombres_recursos_global, i);
                t_recurso *r = dictionary_get(recursos_sistema, nombre);
                if (r != NULL) {
                    t_list *elements = r->cola_bloqueados->elements;
                    int queue_len = list_size(elements);
                    bool found = false;
                    for (int j = 0; j < queue_len; j++) {
                        t_pcb *p = list_get(elements, j);
                        if (p->pid == pid_dueno) {
                            found = true;
                            p->prioridad_actual = p_owner->prioridad_actual;
                            break;
                        }
                    }
                    if (found) {
                        next_recurso = r;
                        break;
                    }
                }
            }

            if (next_recurso != NULL && next_recurso->pid_dueno != -1) {
                aplicar_herencia_prioridad(pid_dueno, next_recurso->pid_dueno, next_recurso);
            }
            return;
        }
    }
    pthread_mutex_unlock(&mutex_prioridades);
}
// =========================================== //
