#include "cpu_interrupt.h"
#include "funciones_cpu.h"

void *interrupt_server(void *arg) {

  int err;
  // INICIALIZACION_CONEXION
  log_trace(logger_cpu, "Inicio el Thread de Interrupt");
  kernel_interrupt_fd = fd_interrupt;
  log_trace(logger_cpu, "Kernel interrupt socket asignado!");

  while (1) {
    log_trace(logger_cpu, "INTERRUPT: Esperando nueva interrupcion...");
    int op;
    err = recv(kernel_interrupt_fd, &op, sizeof(int), MSG_WAITALL);
    if (err <= 0) {
      log_error(logger_cpu, "Se Desconectó el Kernel (Interrupt)");
      exit(EXIT_FAILURE);
    }
    
    uint32_t pid;
    err = recv(kernel_interrupt_fd, &pid, sizeof(uint32_t), MSG_WAITALL);
    if (err <= 0) {
      log_error(logger_cpu, "Se Desconectó el Kernel (Interrupt PID)");
      exit(EXIT_FAILURE);
    }

    interrupted_pid = pid;
    interrupt_op = op;
    log_trace(logger_cpu, "INTERRUPT: Se recibio interrupcion (op: %d) para el PID: %u",
              op, interrupted_pid);
  }
  close(kernel_interrupt_fd);
  return NULL;
}