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
            // Enviar paquete LEER_MEMORIA a Kernel Memory
            t_paquete *paquete_km = crear_paquete();
            paquete_km->cop = LEER_MEMORIA;
            agregar_a_paquete(paquete_km, &(pcb->pid), sizeof(int));
            agregar_a_paquete(paquete_km, &dir_fisica, sizeof(int));
            agregar_a_paquete(paquete_km, &tamanio, sizeof(int));
            enviar_paquete(paquete_km, fd_memory);
            eliminar_paquete(paquete_km);
            
            // Esperar la respuesta de la Memoria y guardar en el registro
            if (tamanio == 1) {
              uint8_t valor;
              recv(fd_memory, &valor, sizeof(uint8_t), MSG_WAITALL);
              asignar_valor_registro(reg, valor);
              log_info(logger_cpu, "PID: %d - Acción: LEER - Dirección Física: %d - Valor: %u", pcb->pid, dir_fisica, valor);
            } else {
              uint32_t valor;
              recv(fd_memory, &valor, sizeof(uint32_t), MSG_WAITALL);
              asignar_valor_registro(reg, valor);
              log_info(logger_cpu, "PID: %d - Acción: LEER - Dirección Física: %d - Valor: %u", pcb->pid, dir_fisica, valor);
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
            // Enviar paquete ESCRIBIR_MEMORIA a Kernel Memory
            t_paquete *paquete_km = crear_paquete();
            paquete_km->cop = ESCRIBIR_MEMORIA;
            agregar_a_paquete(paquete_km, &(pcb->pid), sizeof(int));
            agregar_a_paquete(paquete_km, &dir_fisica, sizeof(int));
            agregar_a_paquete(paquete_km, &tamanio, sizeof(int));
            if (tamanio == 1) {
              uint8_t val8 = (uint8_t)valor;
              agregar_a_paquete(paquete_km, &val8, sizeof(uint8_t));
            } else {
              agregar_a_paquete(paquete_km, &valor, sizeof(uint32_t));
            }
            enviar_paquete(paquete_km, fd_memory);
            eliminar_paquete(paquete_km);
            
            // Esperar el mensaje de OK
            int ok;
            recv(fd_memory, &ok, sizeof(int), MSG_WAITALL);
            log_info(logger_cpu, "PID: %d - Acción: ESCRIBIR - Dirección Física: %d - Valor: %u", pcb->pid, dir_fisica, valor);
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
          registros.PC++;
          pcb->program_counter = registros.PC;
          registros_por_pid[pcb->pid] = registros;
          enviar_pcb(pcb, fd_dispatch, IO_STDIN);
          enviar_entero(fd_dispatch, dir);
          enviar_entero(fd_dispatch, size_to_read);
          desaloja = true;
        } else if (strcmp(op, "STDOUT") == 0) {
          char *reg_dir = tokens[1];
          char *reg_size = tokens[2];
          uint32_t dir = obtener_valor_registro(reg_dir);
          uint32_t size_to_read = obtener_valor_registro(reg_size);
          registros.PC++;
          pcb->program_counter = registros.PC;
          registros_por_pid[pcb->pid] = registros;
          enviar_pcb(pcb, fd_dispatch, IO_STDOUT);
          enviar_entero(fd_dispatch, dir);
          enviar_entero(fd_dispatch, size_to_read);
          desaloja = true;
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
        if (interrupted_pid == pcb->pid) {
          log_info(logger_cpu, "## Interrupción recibida");
          pcb->program_counter = registros.PC;
          registros_por_pid[pcb->pid] = registros;
          enviar_pcb(pcb, fd_dispatch, FIN_QUANTUM);
          interrupted_pid = 0;
          break;
        }
      }

      free(pcb->tabla_segmentos);
      free(pcb);
    }
  }

  finalizar_cpu();
  return 0;
}