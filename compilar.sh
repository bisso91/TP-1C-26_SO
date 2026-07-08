#!/bin/bash

# Script de compilación manual para el TP Plug & Pray
# Compila la biblioteca utils primero y luego el resto de los módulos en orden.

# Salir inmediatamente si algún comando falla
set -e

# Colores para la consola
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== Iniciando Compilación del TP ===${NC}"

# 1. Compilar biblioteca compartida utils
echo -e "${GREEN}--> Compilando biblioteca utils...${NC}"
make -C utils clean
make -C utils

# 2. Compilar módulos del TP
MODULOS=("swap" "kernel_memory" "memory_stick" "kernel_scheduler" "cpu" "io")

for modulo in "${MODULOS[@]}"; do
    echo -e "${GREEN}--> Compilando módulo: $modulo...${NC}"
    make -C "$modulo" clean
    make -C "$modulo"
done

echo -e "${BLUE}=== ¡Compilación Completada Exitosamente! ===${NC}"
