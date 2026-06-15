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
    err = recv(kernel_interrupt_fd, &interrupted_pid, sizeof(uint32_t),
               MSG_WAITALL);

    if (err <= 0) {
      log_error(logger_cpu, "Se Desconectó el Kernel");
      exit(EXIT_FAILURE);
    }
    log_trace(logger_cpu, "INTERRUPT: Se recibio interrupcion para el PID: %d",
              interrupted_pid);
  }
  close(kernel_interrupt_fd);
  return NULL;
}