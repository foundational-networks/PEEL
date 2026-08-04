#!/bin/bash

# Check if an argument was provided
if [ -z "$1" ]; then
    echo "Usage: $0 <rules_file.py>"
    exit 1
fi

RULE_FILE="$1"

# Optional: verify the file exists
if [ ! -f "$RULE_FILE" ]; then
    echo "Error: File '$RULE_FILE' not found."
    exit 1
fi

# Run the command with the provided file
/opt/venvs/ryu39/bin/python \
    /opt/venvs/ryu39/bin/ryu-manager \
    --ofp-tcp-listen-port 6653 \
    "$RULE_FILE"
