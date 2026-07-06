#!/bin/bash

# Create logs directory
mkdir -p logs

# Cleanup function to kill background processes on exit
cleanup() {
    echo "Stopping all processes..."
    kill $PID_MEM $PID_SCHED $PID_CPU 2>/dev/null
    exit
}
trap cleanup SIGINT SIGTERM

echo "Starting Kernel Memory..."
cd /home/cesar/Documentos/utn/tpso/tp-2026-1c-cortisolmaxxing/kernel_memory
./bin/kernel_memory ./kernel_memory.config > ../logs/memory.log 2>&1 &
PID_MEM=$!

sleep 1

echo "Starting Kernel Scheduler..."
cd /home/cesar/Documentos/utn/tpso/tp-2026-1c-cortisolmaxxing/kernel_scheduler
./bin/kernel_scheduler ./kernel_scheduler.config PLANI_PRE_0.prc > ../logs/scheduler.log 2>&1 &
PID_SCHED=$!

sleep 1

echo "Starting CPU..."
cd /home/cesar/Documentos/utn/tpso/tp-2026-1c-cortisolmaxxing/cpu
./bin/cpu ./cpu.config 1 > ../logs/cpu.log 2>&1 &
PID_CPU=$!

echo "========================================================="
echo "Planificación Preliminar is now running in the background."
echo "Press [Ctrl+C] to stop all modules."
echo "========================================================="
echo "Monitoring logs (press Ctrl+C to exit):"
echo "========================================================="

tail -f ../logs/scheduler.log ../logs/cpu.log ../logs/memory.log
