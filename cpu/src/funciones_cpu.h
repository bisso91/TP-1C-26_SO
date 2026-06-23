#ifndef CPU_UTILS_H_
#define CPU_UTILS_H_

#include <commons/collections/list.h>
#include <commons/config.h>
#include <commons/log.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <utils/utils.h>
#include "cpu_utils.h"

typedef struct {
    int socket;
    int base;
    int limite;
    char *ip;
    int puerto;
} t_cpu_stick_conn;

// Defino estructura del .config
typedef struct {
  char *ip_memory;
  char *puerto_memory;
  char *ip_scheduler;
  char *puerto_scheduler;
  char *ip_memory_stick;
  char *puerto_memory_stick;
  t_log_level log_level;
  char *puerto_escucha_dispatch;
  char *puerto_escucha_interrupt;
} t_config_cpu;

// Estructura para los registros de la CPU (Basado en página 19 del PDF)
typedef struct {
  uint32_t PC; // Program Counter
  uint8_t AX;  // Registros de 1 byte
  uint8_t BX;
  uint8_t CX;
  uint8_t DX;
  uint32_t EAX; // Registros de 4 bytes
  uint32_t EBX;
  uint32_t ECX;
  uint32_t EDX;
  uint32_t SI; // Dirección lógica de origen
  uint32_t DI; // Dirección lógica de destino
} t_registros;

// Agrego los extern
extern t_log *logger_cpu;
extern t_config *config_plana;
extern t_config_cpu config_cpu;
extern t_registros registros;

extern int kernel_interrupt_fd;
extern uint32_t interrupted_pid;
extern int interrupt_op;
extern int fd_memory;
extern int fd_scheduler;
extern int fd_interrupt;
extern int fd_dispatch;

extern t_list *cpu_sticks;
extern pthread_mutex_t mutex_cpu_sticks;

// --- PROTOTIPOS ---
void inicializar_cpu(char *config_path, char *id_cpu);
void finalizar_cpu();
int conectar_a_modulo(char *nombre, char *ip, char *puerto, t_log *logger);
bool cargar_configuracion(t_config_cpu *config_cpu, t_config *config_raw,
                          t_log *logger);
bool inicializar_registros(t_registros *registros, t_log *logger);
int recibir_operacion_cpu(int socket_cliente, t_log *logger); 

void actualizar_sticks_desde_memoria();
int obtener_conexion_stick(int dir_fisica);
bool cpu_leer_memoria_segmentado(uint32_t dir_fisica, int tamanio, void *dest_buffer);
bool cpu_escribir_memoria_segmentado(uint32_t dir_fisica, int tamanio, void *src_buffer);

#endif