#!/bin/bash

# Exit on error
set -e

echo "=== Compiling utils ==="
cd /home/cesar/Documentos/utn/tpso/tp-2026-1c-cortisolmaxxing/utils
make clean && make

echo "=== Compiling memory_stick ==="
cd /home/cesar/Documentos/utn/tpso/tp-2026-1c-cortisolmaxxing/memory_stick
make clean && make

echo "=== Compiling verification client ==="
cd /home/cesar/Documentos/utn/tpso/tp-2026-1c-cortisolmaxxing/scratch
gcc -o test_stick test_stick.c -I../utils/src -L../utils/lib -lutils -lpthread -lcommons

echo "=== Running verification client mock and server ==="
cd /home/cesar/Documentos/utn/tpso/tp-2026-1c-cortisolmaxxing
export LD_LIBRARY_PATH=/home/cesar/Documentos/utn/tpso/tp-2026-1c-cortisolmaxxing/utils/bin:$LD_LIBRARY_PATH

# Run verification test in background (it will start mock kernel on 8000)
./scratch/test_stick &
PID_TEST=$!

# Wait for mock kernel to start listening
sleep 1.5

# Start memory stick
echo "Starting memory stick..."
./memory_stick/bin/memory_stick ./memory_stick/memory_stick.config 1024 > ./logs/stick_server.log 2>&1 &
PID_STICK=$!

# Wait for the test to complete
wait $PID_TEST

# Stop the memory stick
echo "Stopping memory stick..."
kill $PID_STICK 2>/dev/null || true

sleep 1

echo "=== Memory Stick Logs ==="
cat logs/stick_server.log
echo "=== Test Done ==="
