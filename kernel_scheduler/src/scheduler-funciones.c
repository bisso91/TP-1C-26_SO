#include "scheduler-funciones.h"
#include <commons/collections/queue.h>
#include <commons/log.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>

// --- Definición de variables globales --- //
t_log *logger_server;
int generador_pid = 1;
bool compactacion_activa = false;

t_queue *cola_new;
t_queue *cola_ready;
t_queue *cola_block;
t_queue *cola_exit;
t_queue *cola_susp_block;
t_queue *cola_susp_ready;

pthread_mutex_t mutex_new;
pthread_mutex_t mutex_ready;
pthread_mutex_t mutex_block;
pthread_mutex_t mutex_exit;
pthread_mutex_t mutex_susp_block;
pthread_mutex_t mutex_susp_ready;
int suspension_timeout = 3000;
t_dictionary *tiempos_suspension;
int susp_counter = 0;

sem_t sem_procesos_en_new;
sem_t sem_cpu_libre;

t_dictionary *interfaces_io;
pthread_mutex_t mutex_interfaces_io;

t_list *nombres_recursos_global;
t_dictionary *prioridades_procesos;
pthread_mutex_t mutex_prioridades;

char **queues_algorithms = NULL;
bool queue_preemption = false;
t_queue **colas_multinivel = NULL;
int cant_colas_multinivel = 0;
int pid_ejecutando_cmn = -1; // track executing pid in CMN
int prioridad_ejecutando_cmn = -1; // track priority of executing process in CMN

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

        encolar_proceso_ready(pcb);
    }
    return NULL;
}

void encolar_proceso_ready(t_pcb *pcb) {
    char *prev_estado_str;
    if (pcb->estado == ESTADO_NEW) prev_estado_str = "NEW";
    else if (pcb->estado == ESTADO_BLOCK) prev_estado_str = "BLOCK";
    else if (pcb->estado == ESTADO_EXEC) prev_estado_str = "EXEC";
    else if (pcb->estado == ESTADO_SUS_READY) prev_estado_str = "SUSP_READY";
    else if (pcb->estado == ESTADO_SUS_BLOCK) prev_estado_str = "SUSP_BLOCK";
    else prev_estado_str = "READY";
    
    pcb->estado = ESTADO_READY;
    log_info(logger_server, "## (%d) Pasa del estado %s al estado READY", pcb->pid, prev_estado_str);

    pthread_mutex_lock(&mutex_ready);
    if (strcmp(algoritmo_de_planificacion, "CMN") == 0) {
        int prio = pcb->prioridad_actual;
        if (prio < 0) prio = 0;
        if (prio >= cant_colas_multinivel) prio = cant_colas_multinivel - 1;
        
        queue_push(colas_multinivel[prio], pcb);
        
        // Check preemption
        if (queue_preemption && pid_ejecutando_cmn != -1 && prio < prioridad_ejecutando_cmn) {
            if (socket_cpu_interrupt != -1) {
                int cop_int = INTERRUPCION_DESALOJO;
                send(socket_cpu_interrupt, &cop_int, sizeof(int), 0);
                enviar_entero(socket_cpu_interrupt, pid_ejecutando_cmn);
                
                log_info(logger_server, "## (%d) Prioridad: %d - Desalojado por cola más prioritaria por el proceso %d con prioridad %d",
                         pid_ejecutando_cmn, prioridad_ejecutando_cmn, pcb->pid, pcb->prioridad_actual);
            }
        }
    } else {
        queue_push(cola_ready, pcb);
    }
    pthread_mutex_unlock(&mutex_ready);
    
    sem_post(&sem_procesos_en_ready);
}

void *planificador_corto_plazo_cmn(void *arg) {
    while (1) {
        while (socket_cpu_dispatch == -1) {
            usleep(100 * 1000);
        }
        sem_wait(&sem_cpu_libre);
        sem_wait(&sem_procesos_en_ready);
        
        pthread_mutex_lock(&mutex_ready);
        t_pcb *pcb_a_ejecutar = NULL;
        int cola_elegida = -1;
        
        for (int i = 0; i < cant_colas_multinivel; i++) {
            if (queue_size(colas_multinivel[i]) > 0) {
                pcb_a_ejecutar = queue_pop(colas_multinivel[i]);
                cola_elegida = i;
                break;
            }
        }
        pthread_mutex_unlock(&mutex_ready);
        
        if (pcb_a_ejecutar == NULL) {
            sem_post(&sem_cpu_libre);
            continue;
        }
        
        pcb_a_ejecutar->estado = ESTADO_EXEC;
        pid_ejecutando_cmn = pcb_a_ejecutar->pid;
        prioridad_ejecutando_cmn = pcb_a_ejecutar->prioridad_actual;
        
        log_info(logger_server, "## (%d) Pasa a estado EXEC (round robin/fifo cmn)", pcb_a_ejecutar->pid);
        
        if (strcmp(queues_algorithms[cola_elegida], "RR") == 0) {
            pthread_t hilo_timer;
            int *pid_ptr = malloc(sizeof(int));
            *pid_ptr = pcb_a_ejecutar->pid;
            pthread_create(&hilo_timer, NULL, temporizador_quantum, pid_ptr);
            pthread_detach(hilo_timer);
        }
        
        log_info(logger_server, "Despachando proceso PID: %d a la CPU...", pcb_a_ejecutar->pid);
        enviar_pcb(pcb_a_ejecutar, socket_cpu_dispatch, DISPATCH_PCB);
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
    log_info(logger_server, "## (%d) Pasa del estado EXEC al estado BLOCK", pcb->pid);

    encolar_proceso_block(pcb);
}

void desbloquear_proceso_de_io(t_pcb *pcb){
    encolar_proceso_ready(pcb);
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
    intentar_des_suspender_procesos();
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
        pid_ejecutando_cmn = -1;

        if (recurso->pid_dueno != -1) {
            aplicar_herencia_prioridad(pcb->pid, recurso->pid_dueno, recurso);
        }
    } else {
        log_info(logger_server, "## (PID: %d) Asignado al recurso %s. Continúa ejecutando.", pcb->pid, nombre_recurso);
        recurso->pid_dueno = pcb->pid;
        log_info(logger_server, "## (%d) Toma el Mutex %s", pcb->pid, nombre_recurso);

        encolar_proceso_ready(pcb);
    }
    pthread_mutex_unlock(&(recurso->mutex_recurso));

    sem_post(&sem_cpu_libre);
}

void liberar_recurso_signal(t_pcb *pcb, char *nombre_recurso, int cliente_fd){
    t_recurso *recurso = dictionary_get(recursos_sistema, nombre_recurso);

    if(recurso == NULL){
        log_error(logger_server, "El recurso %s no existe", nombre_recurso);
        encolar_proceso_ready(pcb);
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

        encolar_proceso_ready(pcb_desbloqueado);
    } else {
        recurso->pid_dueno = -1;
    }

    recalcular_prioridad(pcb);

    pthread_mutex_unlock(&(recurso->mutex_recurso));

    encolar_proceso_ready(pcb);

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
        pthread_mutex_unlock(&mutex_prioridades);
        actualizar_prioridad_en_ready(pcb->pid, prio_nueva);
    } else {
        pthread_mutex_unlock(&mutex_prioridades);
    }
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
            actualizar_prioridad_en_ready(pid_dueno, p_owner->prioridad_actual);

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

char *estado_a_string(t_estado estado) {
    switch (estado) {
        case ESTADO_NEW: return "NEW";
        case ESTADO_READY: return "READY";
        case ESTADO_EXEC: return "EXEC";
        case ESTADO_BLOCK: return "BLOCK";
        case ESTADO_SUS_READY: return "SUSP_READY";
        case ESTADO_SUS_BLOCK: return "SUSP_BLOCK";
        case ESTADO_EXIT: return "EXIT";
        default: return "UNKNOWN";
    }
}

void cambiar_estado(t_pcb *pcb, t_estado nuevo_estado) {
    char *ant = estado_a_string(pcb->estado);
    char *nue = estado_a_string(nuevo_estado);
    pcb->estado = nuevo_estado;
    log_info(logger_server, "## (%d) Pasa del estado %s al estado %s", pcb->pid, ant, nue);
}

void encolar_proceso_block(t_pcb *pcb) {
    pcb->estado = ESTADO_BLOCK;
    pthread_mutex_lock(&mutex_block);
    queue_push(cola_block, pcb);
    pthread_mutex_unlock(&mutex_block);

    // Disparamos el temporizador de suspensión en un hilo separado
    pthread_t hilo_susp;
    int *pid_ptr = malloc(sizeof(int));
    *pid_ptr = pcb->pid;
    pthread_create(&hilo_susp, NULL, temporizador_suspension, pid_ptr);
    pthread_detach(hilo_susp);
}

t_pcb *sacar_de_cola_susp_block(int pid) {
    t_pcb *pcb_encontrado = NULL;
    pthread_mutex_lock(&mutex_susp_block);
    int size = queue_size(cola_susp_block);
    for (int i = 0; i < size; i++) {
        t_pcb *pcb_aux = queue_pop(cola_susp_block);
        if (pcb_aux->pid == pid) {
            pcb_encontrado = pcb_aux;
        } else {
            queue_push(cola_susp_block, pcb_aux);
        }
    }
    pthread_mutex_unlock(&mutex_susp_block);
    return pcb_encontrado;
}

void actualizar_prioridad_en_ready(int pid, int nueva_prioridad) {
    pthread_mutex_lock(&mutex_ready);
    if (strcmp(algoritmo_de_planificacion, "CMN") == 0) {
        for (int i = 0; i < cant_colas_multinivel; i++) {
            t_list *elements = colas_multinivel[i]->elements;
            int len = list_size(elements);
            for (int j = 0; j < len; j++) {
                t_pcb *p = list_get(elements, j);
                if (p->pid == pid) {
                    list_remove(elements, j);
                    p->prioridad_actual = nueva_prioridad;
                    int dest_prio = nueva_prioridad;
                    if (dest_prio < 0) dest_prio = 0;
                    if (dest_prio >= cant_colas_multinivel) dest_prio = cant_colas_multinivel - 1;
                    queue_push(colas_multinivel[dest_prio], p);
                    pthread_mutex_unlock(&mutex_ready);
                    return;
                }
            }
        }
    } else {
        t_list *elements = cola_ready->elements;
        int len = list_size(elements);
        for (int j = 0; j < len; j++) {
            t_pcb *p = list_get(elements, j);
            if (p->pid == pid) {
                p->prioridad_actual = nueva_prioridad;
                pthread_mutex_unlock(&mutex_ready);
                return;
            }
        }
    }
    pthread_mutex_unlock(&mutex_ready);
}

void intentar_des_suspender_procesos() {
    pthread_mutex_lock(&mutex_susp_ready);
    if (queue_size(cola_susp_ready) == 0) {
        pthread_mutex_unlock(&mutex_susp_ready);
        return;
    }

    t_list *lista_ordenada = list_create();
    while (queue_size(cola_susp_ready) > 0) {
        list_add(lista_ordenada, queue_pop(cola_susp_ready));
    }

    bool comparador(void *a, void *b) {
        t_pcb *pcb_a = (t_pcb*)a;
        t_pcb *pcb_b = (t_pcb*)b;
        if (pcb_a->prioridad_actual != pcb_b->prioridad_actual) {
            return pcb_a->prioridad_actual < pcb_b->prioridad_actual;
        }
        char pid_a[32], pid_b[32];
        sprintf(pid_a, "%d", pcb_a->pid);
        sprintf(pid_b, "%d", pcb_b->pid);
        int val_a = 0, val_b = 0;
        pthread_mutex_lock(&mutex_prioridades);
        if (dictionary_has_key(tiempos_suspension, pid_a)) {
            val_a = (int)(intptr_t)dictionary_get(tiempos_suspension, pid_a);
        }
        if (dictionary_has_key(tiempos_suspension, pid_b)) {
            val_b = (int)(intptr_t)dictionary_get(tiempos_suspension, pid_b);
        }
        pthread_mutex_unlock(&mutex_prioridades);
        return val_a < val_b;
    }
    list_sort(lista_ordenada, comparador);

    t_list *no_des_suspendidos = list_create();
    int size = list_size(lista_ordenada);
    for (int i = 0; i < size; i++) {
        t_pcb *pcb = list_get(lista_ordenada, i);
        if (sem_trywait(&sem_grado_multiprogramacion) == 0) {
            log_info(logger_server, "Intentando des-suspender PID %d...", pcb->pid);
            int cop = DES_SUSPENDER_PROCESO;
            send(socket_kernel_memory, &cop, sizeof(int), 0);
            enviar_entero(socket_kernel_memory, pcb->pid);

            int response = 0;
            recv(socket_kernel_memory, &response, sizeof(int), MSG_WAITALL);

            if (response == 1) {
                int cant_seg;
                recv(socket_kernel_memory, &cant_seg, sizeof(int), MSG_WAITALL);
                pcb->cantidad_segmentos = cant_seg;
                free(pcb->tabla_segmentos);
                if (cant_seg > 0) {
                    pcb->tabla_segmentos = malloc(sizeof(t_segmento) * cant_seg);
                    for (int s = 0; s < cant_seg; s++) {
                        recv(socket_kernel_memory, &(pcb->tabla_segmentos[s].id), sizeof(int), MSG_WAITALL);
                        recv(socket_kernel_memory, &(pcb->tabla_segmentos[s].base), sizeof(int), MSG_WAITALL);
                        recv(socket_kernel_memory, &(pcb->tabla_segmentos[s].limite), sizeof(int), MSG_WAITALL);
                    }
                } else {
                    pcb->tabla_segmentos = NULL;
                }

                log_info(logger_server, "## (%d) Pasa del estado SUSP_READY al estado READY", pcb->pid);
                encolar_proceso_ready(pcb);

                char pid_str[32];
                sprintf(pid_str, "%d", pcb->pid);
                pthread_mutex_lock(&mutex_prioridades);
                dictionary_remove(tiempos_suspension, pid_str);
                pthread_mutex_unlock(&mutex_prioridades);
            } else {
                sem_post(&sem_grado_multiprogramacion);
                list_add(no_des_suspendidos, pcb);
            }
        } else {
            list_add(no_des_suspendidos, pcb);
        }
    }

    int remaining_size = list_size(no_des_suspendidos);
    for (int i = 0; i < remaining_size; i++) {
        queue_push(cola_susp_ready, list_get(no_des_suspendidos, i));
    }

    list_destroy(lista_ordenada);
    list_destroy(no_des_suspendidos);
    pthread_mutex_unlock(&mutex_susp_ready);
}

void lanzar_bsod() {
    log_error(logger_server, "====================================================");
    log_error(logger_server, "!!! BLUE SCREEN OF DEATH (BSOD) !!!");
    log_error(logger_server, "Se detectó desconexión de Memory Stick o corrupción.");
    log_error(logger_server, "Finalizando todos los procesos del sistema.");
    log_error(logger_server, "====================================================");

    void finalizar_procesos_en_cola(t_queue *cola, char *motivo, pthread_mutex_t *mutex) {
        pthread_mutex_lock(mutex);
        while (queue_size(cola) > 0) {
            t_pcb *pcb = queue_pop(cola);
            pthread_mutex_unlock(mutex);
            finalizar_proceso(pcb, motivo);
            pthread_mutex_lock(mutex);
        }
        pthread_mutex_unlock(mutex);
    }

    finalizar_procesos_en_cola(cola_new, "BSOD", &mutex_new);
    finalizar_procesos_en_cola(cola_ready, "BSOD", &mutex_ready);
    finalizar_procesos_en_cola(cola_block, "BSOD", &mutex_block);
    finalizar_procesos_en_cola(cola_susp_block, "BSOD", &mutex_susp_block);
    finalizar_procesos_en_cola(cola_susp_ready, "BSOD", &mutex_susp_ready);

    if (strcmp(algoritmo_de_planificacion, "CMN") == 0) {
        for (int i = 0; i < cant_colas_multinivel; i++) {
            pthread_mutex_lock(&mutex_ready);
            while (queue_size(colas_multinivel[i]) > 0) {
                t_pcb *pcb = queue_pop(colas_multinivel[i]);
                pthread_mutex_unlock(&mutex_ready);
                finalizar_proceso(pcb, "BSOD");
                pthread_mutex_lock(&mutex_ready);
            }
            pthread_mutex_unlock(&mutex_ready);
        }
    }

    log_error(logger_server, "Kernel Scheduler finalizado por BSOD.");
    exit(1);
}

void *temporizador_suspension(void *arg) {
    int pid = *(int*)arg;
    free(arg);

    usleep(suspension_timeout * 1000);

    pthread_mutex_lock(&mutex_block);
    bool still_blocked = false;
    t_pcb *target_pcb = NULL;
    t_queue *temp_queue = queue_create();

    while (queue_size(cola_block) > 0) {
        t_pcb *p = queue_pop(cola_block);
        if (p->pid == pid) {
            still_blocked = true;
            target_pcb = p;
        } else {
            queue_push(temp_queue, p);
        }
    }

    while (queue_size(temp_queue) > 0) {
        queue_push(cola_block, queue_pop(temp_queue));
    }
    queue_destroy(temp_queue);
    pthread_mutex_unlock(&mutex_block);

    if (still_blocked && target_pcb != NULL) {
        log_info(logger_server, "## (%d) Pasa del estado BLOCK al estado SUSP_BLOCK", target_pcb->pid);
        target_pcb->estado = ESTADO_SUS_BLOCK;

        pthread_mutex_lock(&mutex_susp_block);
        queue_push(cola_susp_block, target_pcb);
        pthread_mutex_unlock(&mutex_susp_block);

        char pid_str[32];
        sprintf(pid_str, "%d", target_pcb->pid);
        pthread_mutex_lock(&mutex_prioridades);
        dictionary_put(tiempos_suspension, pid_str, (void*)(intptr_t)(susp_counter++));
        pthread_mutex_unlock(&mutex_prioridades);

        log_info(logger_server, "Enviando proceso PID %d a SWAP...", target_pcb->pid);
        if (socket_kernel_memory != -1) {
            int cop = SUSPENDER_PROCESO;
            send(socket_kernel_memory, &cop, sizeof(int), 0);
            enviar_entero(socket_kernel_memory, target_pcb->pid);

            int ok = 0;
            recv(socket_kernel_memory, &ok, sizeof(int), MSG_WAITALL);
        }

        sem_post(&sem_grado_multiprogramacion);
        intentar_des_suspender_procesos();
    }
    return NULL;
}

void *hilo_des_suspension_periodico(void *arg) {
    while (1) {
        usleep(500 * 1000); // 500ms
        intentar_des_suspender_procesos();
    }
    return NULL;
}

