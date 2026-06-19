#include <commons/config.h>
#include <commons/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <utils/hello.h>
#include <utils/utils.h>
#include "stick_utils.h"

int main(int argc, char *argv[]) {
  saludar("memory_stick");

  if (argc < 3){
    printf("Error: Faltan argumentos. Uso: ./bin/memory_stick [Config] [Tamaño]\n");
    return 1;
  }

  int tamanio_memoria = atoi(argv[2]);
  iniciar_operacion_stick(argv[1], tamanio_memoria);

  return 0;
}
