#!/bin/bash

# Create logs directory
mkdir -p logs

echo "Starting Kernel Memory..."
cd /home/cesar/Documentos/utn/tpso/tp-2026-1c-cortisolmaxxing/kernel_memory
./bin/kernel_memory ./kernel_memory.config > ../logs/memory.log 2>&1 &
PID_MEM=$!

sleep 1

echo "Starting Kernel Scheduler..."
cd /home/cesar/Documentos/utn/tpso/tp-2026-1c-cortisolmaxxing/kernel_scheduler
./bin/kernel_scheduler ./kernel_scheduler.config proceso_main.txt > ../logs/scheduler.log 2>&1 &
PID_SCHED=$!

sleep 1

echo "Starting CPU..."
cd /home/cesar/Documentos/utn/tpso/tp-2026-1c-cortisolmaxxing/cpu
./bin/cpu ./cpu.config 1 > ../logs/cpu.log 2>&1 &
PID_CPU=$!

sleep 15

echo "Stopping all processes..."
kill $PID_MEM $PID_SCHED $PID_CPU 2>/dev/null

echo "Logs collected:"
echo "=== Memory Log ==="
cat ../logs/memory.log
echo "=== Scheduler Log ==="
cat ../logs/scheduler.log
echo "=== CPU Log ==="
cat ../logs/cpu.log
