#!/bin/bash

python3 Compressor.py

echo -e "\n\nPrinting HPN sample results into archive/sample_hpn.txt\n"

cp VARIABLES_sample_hpn.py VARIABLES.py
python3 response_time_constant_load_hpn.py > archive/sample_hpn.txt

cat archive/sample_hpn.txt

echo -e "\nDone!\n"
