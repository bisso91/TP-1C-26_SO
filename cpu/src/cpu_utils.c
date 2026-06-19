#include "cpu_utils.h"
#include <stdio.h>
#include <stdlib.h>

int mmu_traducir_direccion(uint32_t direccion_logica, int tamanio_dato, int cantidad_segmentos, t_segmento *tabla_segmentos) {
    int num_segmento = (int)(direccion_logica / TAMANIO_MAX_SEGMENTO);
    int offset = (int)(direccion_logica % TAMANIO_MAX_SEGMENTO);

    t_segmento *segmento = NULL;
    
    // Buscar por ID de segmento
    for (int i = 0; i < cantidad_segmentos; i++) {
        if (tabla_segmentos[i].id == num_segmento) {
            segmento = &tabla_segmentos[i];
            break;
        }
    }

    // Fallback: usar el índice del segmento si es válido
    if (segmento == NULL && num_segmento >= 0 && num_segmento < cantidad_segmentos) {
        segmento = &tabla_segmentos[num_segmento];
    }

    if (segmento == NULL) {
        return -1;
    }

    if (offset + tamanio_dato > segmento->limite) {
        return -1; // Excede el límite del segmento -> Segmentation Fault
    }

    return segmento->base + offset;
}
