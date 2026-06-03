#include "cpu_interrupt.h"
#include "cpu_utils.h"

void *interrupt_server(void *) {

  int err;
  // INICIALIZACION_CONEXION
  log_trace(logger_cpu, "Inicio el Thread de Interrupt");
  kernel_interrupt_fd = esperar_cliente(fd_interrupt);
  log_trace(logger_cpu, "Kernel se conecto a CPU_INTERRUPT!");
  // sem_post(&initialization_mutex);

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
  // IMPLEMENTAR FIN DE BUCLE*
  close(kernel_interrupt_fd);
  return NULL;
}