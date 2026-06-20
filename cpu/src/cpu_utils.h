#ifndef CPU_MMU_UTILS_H_
#define CPU_MMU_UTILS_H_

#include <utils/utils.h>
#include <stdint.h>

#define TAMANIO_MAX_SEGMENTO 256

int mmu_traducir_direccion(uint32_t direccion_logica, int tamanio_dato, int cantidad_segmentos, t_segmento *tabla_segmentos);

#endif /* CPU_MMU_UTILS_H_ */
