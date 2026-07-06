#include "funciones_cpu.h"
#include <commons/config.h>
#include <commons/log.h>
#include <commons/string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <utils/hello.h>
#include <utils/utils.h>

t_registros registros_por_pid[1000];

uint32_t obtener_valor_registro(char *nombre_reg) {
  if (strcmp(nombre_reg, "AX") == 0) return registros.AX;
  if (strcmp(nombre_reg, "BX") == 0) return registros.BX;
  if (strcmp(nombre_reg, "CX") == 0) return registros.CX;
  if (strcmp(nombre_reg, "DX") == 0) return registros.DX;
  if (strcmp(nombre_reg, "EAX") == 0) return registros.EAX;
  if (strcmp(nombre_reg, "EBX") == 0) return registros.EBX;
  if (strcmp(nombre_reg, "ECX") == 0) return registros.ECX;
  if (strcmp(nombre_reg, "EDX") == 0) return registros.EDX;
  if (strcmp(nombre_reg, "PC") == 0) return registros.PC;
  if (strcmp(nombre_reg, "SI") == 0) return registros.SI;
  if (strcmp(nombre_reg, "DI") == 0) return registros.DI;
  return 0;
}

void asignar_valor_registro(char *nombre_reg, uint32_t valor) {
  if (strcmp(nombre_reg, "AX") == 0) registros.AX = (uint8_t)valor;
  else if (strcmp(nombre_reg, "BX") == 0) registros.BX = (uint8_t)valor;
  else if (strcmp(nombre_reg, "CX") == 0) registros.CX = (uint8_t)valor;
  else if (strcmp(nombre_reg, "DX") == 0) registros.DX = (uint8_t)valor;
  else if (strcmp(nombre_reg, "EAX") == 0) registros.EAX = valor;
  else if (strcmp(nombre_reg, "EBX") == 0) registros.EBX = valor;
  else if (strcmp(nombre_reg, "ECX") == 0) registros.ECX = valor;
  else if (strcmp(nombre_reg, "EDX") == 0) registros.EDX = valor;
  else if (strcmp(nombre_reg, "PC") == 0) registros.PC = valor;
  else if (strcmp(nombre_reg, "SI") == 0) registros.SI = valor;
  else if (strcmp(nombre_reg, "DI") == 0) registros.DI = valor;
}

int obtener_tamanio_registro(char *nombre_reg) {
  if (strcmp(nombre_reg, "AX") == 0 || strcmp(nombre_reg, "BX") == 0 ||
      strcmp(nombre_reg, "CX") == 0 || strcmp(nombre_reg, "DX") == 0) {
    return 1;
  }
  if (strcmp(nombre_reg, "EAX") == 0 || strcmp(nombre_reg, "EBX") == 0 ||
      strcmp(nombre_reg, "ECX") == 0 || strcmp(nombre_reg, "EDX") == 0 ||
      strcmp(nombre_reg, "SI") == 0 || strcmp(nombre_reg, "DI") == 0 ||
      strcmp(nombre_reg, "PC") == 0) {
    return 4;
  }
  return 0;
}

uint32_t obtener_direccion_logica(char *token) {
  if (strcmp(token, "AX") == 0 || strcmp(token, "BX") == 0 ||
      strcmp(token, "CX") == 0 || strcmp(token, "DX") == 0 ||
      strcmp(token, "EAX") == 0 || strcmp(token, "EBX") == 0 ||
      strcmp(token, "ECX") == 0 || strcmp(token, "EDX") == 0 ||
      strcmp(token, "SI") == 0 || strcmp(token, "DI") == 0 ||
      strcmp(token, "PC") == 0) {
    return obtener_valor_registro(token);
  }
  return (uint32_t)atoi(token);
}

int main(int argc, char *argv[]) {
  saludar("cpu");

  char *config_path = (argc > 1) ? argv[1] : "./cpu.config";
  char *id_cpu = (argc > 2) ? argv[2] : "1";

  inicializar_cpu(config_path, id_cpu);

  log_info(logger_cpu, "Entrando al bucle de escucha de dispatch en FD: %d", fd_dispatch);

  while (1) {
    int cod_op = recibir_operacion(fd_dispatch);

    if (cod_op <= 0) {
      log_error(logger_cpu, "El Scheduler se desconectó del puerto Dispatch. Saliendo.");
      break;
    }

    if (cod_op == DISPATCH_PCB) {
      t_pcb *pcb = recibir_pcb(fd_dispatch);
      log_info(logger_cpu, "Me llegó un proceso (PID: %d). Iniciando Ciclo de Instrucción.", pcb->pid);

      // Cargar registros
      registros = registros_por_pid[pcb->pid];
      registros.PC = pcb->program_counter;
      interrupted_pid = 0;

      while (1) {
        // 1. Fetch
        log_info(logger_cpu, "## PID: %d - FETCH - Program Counter: %d", pcb->pid, registros.PC);

        char *payload_fetch = string_from_format("%d %d", pcb->pid, registros.PC);
        enviar_string(payload_fetch, fd_memory, PEDIR_INSTRUCCION);
        free(payload_fetch);

        int cop_res = recibir_operacion(fd_memory);
        if (cop_res <= 0) {
          log_error(logger_cpu, "Fallo al recibir instruccion de memoria.");
          exit(EXIT_FAILURE);
        }
        char *instruccion = recibir_string(fd_memory);

        // 2. Decode & Execute
        log_info(logger_cpu, "## PID: %d - Ejecutando: %s", pcb->pid, instruccion);

        char **tokens = string_split(instruccion, " ");
        char *op = tokens[0];

        bool desaloja = false;

        if (strcmp(op, "NOOP") == 0) {
          registros.PC++;
        } else if (strcmp(op, "SET") == 0) {
          char *reg = tokens[1];
          int val = atoi(tokens[2]);
          asignar_valor_registro(reg, val);
          registros.PC++;
        } else if (strcmp(op, "MOV_IN") == 0) {
          char *reg;
          uint32_t dir_logica;
          if (tokens[2] != NULL) {
            reg = tokens[1];
            dir_logica = obtener_direccion_logica(tokens[2]);
          } else {
            reg = tokens[1];
            dir_logica = registros.SI;
          }
          int tamanio = obtener_tamanio_registro(reg);
          
          int dir_fisica = mmu_traducir_direccion(dir_logica, tamanio, pcb->cantidad_segmentos, pcb->tabla_segmentos);
          if (dir_fisica == -1) {
            log_error(logger_cpu, "Segmentation Fault al traducir direccion logica %u para lectura en registro %s", dir_logica, reg);
            registros.PC++;
            pcb->program_counter = registros.PC;
            registros_por_pid[pcb->pid] = registros;
            enviar_pcb(pcb, fd_dispatch, SEGMENTATION_FAULT);
            desaloja = true;
          } else {
            log_info(logger_cpu, "PID: %d - Acción: LEER - Dirección Física: %d - Dirección Lógica: %u", pcb->pid, dir_fisica, dir_logica);
            
            if (tamanio == 1) {
              uint8_t valor;
              if (cpu_leer_memoria_segmentado(dir_fisica, 1, &valor)) {
                asignar_valor_registro(reg, valor);
                log_info(logger_cpu, "PID: %d - Acción: LEER - Dirección Física: %d - Valor: %u", pcb->pid, dir_fisica, valor);
              } else {
                log_error(logger_cpu, "Error al leer direccion fisica %d", dir_fisica);
              }
            } else {
              uint32_t valor;
              if (cpu_leer_memoria_segmentado(dir_fisica, 4, &valor)) {
                asignar_valor_registro(reg, valor);
                log_info(logger_cpu, "PID: %d - Acción: LEER - Dirección Física: %d - Valor: %u", pcb->pid, dir_fisica, valor);
              } else {
                log_error(logger_cpu, "Error al leer direccion fisica %d", dir_fisica);
              }
            }
            registros.PC++;
          }
        } else if (strcmp(op, "MOV_OUT") == 0) {
          char *reg;
          uint32_t dir_logica;
          if (tokens[2] != NULL) {
            dir_logica = obtener_direccion_logica(tokens[1]);
            reg = tokens[2];
          } else {
            reg = tokens[1];
            dir_logica = registros.DI;
          }
          int tamanio = obtener_tamanio_registro(reg);
          uint32_t valor = obtener_valor_registro(reg);
          
          int dir_fisica = mmu_traducir_direccion(dir_logica, tamanio, pcb->cantidad_segmentos, pcb->tabla_segmentos);
          if (dir_fisica == -1) {
            log_error(logger_cpu, "Segmentation Fault al traducir direccion logica %u para escritura de registro %s", dir_logica, reg);
            registros.PC++;
            pcb->program_counter = registros.PC;
            registros_por_pid[pcb->pid] = registros;
            enviar_pcb(pcb, fd_dispatch, SEGMENTATION_FAULT);
            desaloja = true;
          } else {
            log_info(logger_cpu, "PID: %d - Acción: ESCRIBIR - Dirección Física: %d - Dirección Lógica: %u", pcb->pid, dir_fisica, dir_logica);
            if (tamanio == 1) {
              uint8_t val8 = (uint8_t)valor;
              if (cpu_escribir_memoria_segmentado(dir_fisica, 1, &val8)) {
                log_info(logger_cpu, "PID: %d - Acción: ESCRIBIR - Dirección Física: %d - Valor: %u", pcb->pid, dir_fisica, val8);
              } else {
                log_error(logger_cpu, "Error al escribir en direccion fisica %d", dir_fisica);
              }
            } else {
              if (cpu_escribir_memoria_segmentado(dir_fisica, 4, &valor)) {
                log_info(logger_cpu, "PID: %d - Acción: ESCRIBIR - Dirección Física: %d - Valor: %u", pcb->pid, dir_fisica, valor);
              } else {
                log_error(logger_cpu, "Error al escribir en direccion fisica %d", dir_fisica);
              }
            }
            registros.PC++;
          }
        } else if (strcmp(op, "SUM") == 0) {
          char *reg_dest = tokens[1];
          char *reg_orig = tokens[2];
          uint32_t val_dest = obtener_valor_registro(reg_dest);
          uint32_t val_orig = obtener_valor_registro(reg_orig);
          asignar_valor_registro(reg_dest, val_dest + val_orig);
          registros.PC++;
        } else if (strcmp(op, "SUB") == 0) {
          char *reg_dest = tokens[1];
          char *reg_orig = tokens[2];
          uint32_t val_dest = obtener_valor_registro(reg_dest);
          uint32_t val_orig = obtener_valor_registro(reg_orig);
          asignar_valor_registro(reg_dest, val_dest - val_orig);
          registros.PC++;
        } else if (strcmp(op, "COPY_MEM") == 0) {
          char *reg_size = tokens[1];
          int size_to_copy = (int)obtener_valor_registro(reg_size);
          
          int dir_fisica_src = mmu_traducir_direccion(registros.SI, size_to_copy, pcb->cantidad_segmentos, pcb->tabla_segmentos);
          int dir_fisica_dst = mmu_traducir_direccion(registros.DI, size_to_copy, pcb->cantidad_segmentos, pcb->tabla_segmentos);
          
          if (dir_fisica_src == -1 || dir_fisica_dst == -1) {
            log_error(logger_cpu, "Segmentation Fault al traducir SI (%u) o DI (%u) para COPY_MEM (size: %d)", registros.SI, registros.DI, size_to_copy);
            registros.PC++;
            pcb->program_counter = registros.PC;
            registros_por_pid[pcb->pid] = registros;
            enviar_pcb(pcb, fd_dispatch, SEGMENTATION_FAULT);
            desaloja = true;
          } else {
            void *buffer = malloc(size_to_copy);
            if (cpu_leer_memoria_segmentado(dir_fisica_src, size_to_copy, buffer)) {
              if (cpu_escribir_memoria_segmentado(dir_fisica_dst, size_to_copy, buffer)) {
                log_info(logger_cpu, "COPY_MEM ejecutada con éxito (de DF %d a DF %d, %d bytes)", dir_fisica_src, dir_fisica_dst, size_to_copy);
              } else {
                log_error(logger_cpu, "Error al escribir en DF %d para COPY_MEM", dir_fisica_dst);
              }
            } else {
              log_error(logger_cpu, "Error al leer de DF %d para COPY_MEM", dir_fisica_src);
            }
            free(buffer);
            registros.PC++;
          }
        } else if (strcmp(op, "JNZ") == 0) {
          char *reg = tokens[1];
          int pc_target = atoi(tokens[2]);
          uint32_t val = obtener_valor_registro(reg);
          if (val != 0) {
            registros.PC = pc_target;
          } else {
            registros.PC++;
          }
        } else if (strcmp(op, "EXIT") == 0) {
          registros.PC++;
          pcb->program_counter = registros.PC;
          registros_por_pid[pcb->pid] = registros;
          enviar_pcb(pcb, fd_dispatch, FIN_PROCESO);
          desaloja = true;
        } else if (strcmp(op, "SLEEP") == 0) {
          int tiempo = atoi(tokens[1]);
          registros.PC++;
          pcb->program_counter = registros.PC;
          registros_por_pid[pcb->pid] = registros;
          enviar_pcb(pcb, fd_dispatch, IO_SLEEP);
          enviar_entero(fd_dispatch, tiempo);
          desaloja = true;
        } else if (strcmp(op, "STDIN") == 0) {
          char *reg_dir = tokens[1];
          char *reg_size = tokens[2];
          uint32_t dir = obtener_valor_registro(reg_dir);
          uint32_t size_to_read = obtener_valor_registro(reg_size);
          
          int physical_addr = mmu_traducir_direccion(dir, size_to_read, pcb->cantidad_segmentos, pcb->tabla_segmentos);
          if (physical_addr == -1) {
            log_error(logger_cpu, "Segmentation Fault al traducir direccion logica %u para STDIN (size: %u)", dir, size_to_read);
            registros.PC++;
            pcb->program_counter = registros.PC;
            registros_por_pid[pcb->pid] = registros;
            enviar_pcb(pcb, fd_dispatch, SEGMENTATION_FAULT);
            desaloja = true;
          } else {
            registros.PC++;
            pcb->program_counter = registros.PC;
            registros_por_pid[pcb->pid] = registros;
            enviar_pcb(pcb, fd_dispatch, IO_STDIN);
            enviar_entero(fd_dispatch, dir);
            enviar_entero(fd_dispatch, size_to_read);
            desaloja = true;
          }
        } else if (strcmp(op, "STDOUT") == 0) {
          char *reg_dir = tokens[1];
          char *reg_size = tokens[2];
          uint32_t dir = obtener_valor_registro(reg_dir);
          uint32_t size_to_read = obtener_valor_registro(reg_size);
          
          int physical_addr = mmu_traducir_direccion(dir, size_to_read, pcb->cantidad_segmentos, pcb->tabla_segmentos);
          if (physical_addr == -1) {
            log_error(logger_cpu, "Segmentation Fault al traducir direccion logica %u para STDOUT (size: %u)", dir, size_to_read);
            registros.PC++;
            pcb->program_counter = registros.PC;
            registros_por_pid[pcb->pid] = registros;
            enviar_pcb(pcb, fd_dispatch, SEGMENTATION_FAULT);
            desaloja = true;
          } else {
            registros.PC++;
            pcb->program_counter = registros.PC;
            registros_por_pid[pcb->pid] = registros;
            enviar_pcb(pcb, fd_dispatch, IO_STDOUT);
            enviar_entero(fd_dispatch, dir);
            enviar_entero(fd_dispatch, size_to_read);
            desaloja = true;
          }
        } else if (strcmp(op, "MUTEX_LOCK") == 0 || strcmp(op, "MUTEX_CREATE") == 0) {
          char *nombre_recurso = tokens[1];
          registros.PC++;
          pcb->program_counter = registros.PC;
          registros_por_pid[pcb->pid] = registros;
          enviar_pcb(pcb, fd_dispatch, WAIT_RECURSO);
          enviar_string(nombre_recurso, fd_dispatch, WAIT_RECURSO);
          desaloja = true;
        } else if (strcmp(op, "MUTEX_UNLOCK") == 0) {
          char *nombre_recurso = tokens[1];
          registros.PC++;
          pcb->program_counter = registros.PC;
          registros_por_pid[pcb->pid] = registros;
          enviar_pcb(pcb, fd_dispatch, SIGNAL_RECURSO);
          enviar_string(nombre_recurso, fd_dispatch, SIGNAL_RECURSO);
          desaloja = true;
        } else if (strcmp(op, "MEM_ALLOC") == 0) {
          int id_segmento = atoi(tokens[1]);
          int tamanio = atoi(tokens[2]);
          registros.PC++;
          pcb->program_counter = registros.PC;
          registros_por_pid[pcb->pid] = registros;
          enviar_pcb(pcb, fd_dispatch, MEM_ALLOC);
          enviar_entero(fd_dispatch, id_segmento);
          enviar_entero(fd_dispatch, tamanio);
          desaloja = true;
        } else if (strcmp(op, "MEM_FREE") == 0) {
          int id_segmento = atoi(tokens[1]);
          registros.PC++;
          pcb->program_counter = registros.PC;
          registros_por_pid[pcb->pid] = registros;
          enviar_pcb(pcb, fd_dispatch, MEM_FREE);
          enviar_entero(fd_dispatch, id_segmento);
          desaloja = true;
        } else if (strcmp(op, "INIT_PROC") == 0) {
          char *path_proceso = tokens[1];
          int prioridad = atoi(tokens[2]);
          registros.PC++;
          pcb->program_counter = registros.PC;
          registros_por_pid[pcb->pid] = registros;
          enviar_pcb(pcb, fd_dispatch, INIT_PROC);
          enviar_string(path_proceso, fd_dispatch, INIT_PROC);
          enviar_entero(fd_dispatch, prioridad);
          desaloja = true;
        } else {
          log_warning(logger_cpu, "Instrucción desconocida: %s. Desalojando.", op);
          registros.PC++;
          pcb->program_counter = registros.PC;
          registros_por_pid[pcb->pid] = registros;
          enviar_pcb(pcb, fd_dispatch, FIN_PROCESO);
          desaloja = true;
        }

        free(instruccion);
        string_array_destroy(tokens);

        if (desaloja) {
          break;
        }

        // 3. Check Interrupt
        if (interrupted_pid != 0) {
          if (interrupted_pid == pcb->pid) {
            log_info(logger_cpu, "## Interrupción recibida (op: %d)", interrupt_op);
            pcb->program_counter = registros.PC;
            registros_por_pid[pcb->pid] = registros;
            if (interrupt_op == INTERRUPCION_DESALOJO) {
              enviar_pcb(pcb, fd_dispatch, INTERRUPCION_DESALOJO);
            } else {
              enviar_pcb(pcb, fd_dispatch, FIN_QUANTUM);
            }
            interrupted_pid = 0;
            interrupt_op = 0;
            break;
          } else {
            log_warning(logger_cpu, "INTERRUPT: Descartando interrupcion fantasma para PID %u (actualmente ejecuta PID %d)", interrupted_pid, pcb->pid);
            interrupted_pid = 0;
            interrupt_op = 0;
          }
        }
      }

      free(pcb->tabla_segmentos);
      free(pcb);
    }
  }

  finalizar_cpu();
  return 0;
}