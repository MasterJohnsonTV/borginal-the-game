#!/bin/bash

# Define variables
BINARY="Borginal-test"

# 1. Compile all .c files in the current folder
echo "Compiling all source files..."
gcc *.c -o "$BINARY" -lncurses

# 2. Check if compilation worked
if [ $? -eq 0 ]; then
    echo "Compilation successful. Executing..."
    echo "------------------------------------"
    # 3. Run the program
    ./"$BINARY"
    echo "------------------------------------"
    echo "Execution finished."
else
    echo "ERROR: Compilation failed."
    exit 1
fi
