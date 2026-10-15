#!/bin/bash
error_msg=$(ls /root/no_permission 2>&1)
echo "Running Zircon..."
if make -j; then
    echo "Zircon compiled successfully."
else
    echo "ERROR:$error_msg"
    exit 1
fi
make run