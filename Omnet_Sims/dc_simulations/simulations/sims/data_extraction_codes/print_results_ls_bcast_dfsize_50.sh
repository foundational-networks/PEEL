python3 Compressor.py

echo -e "\n\nPrinting results into archive/ls_bcast_dfsize_50.txt\n"

cp VARIABLES_ls_bcast_dfsize_50.py VARIABLES.py
python3 response_time_constant_load.py > archive/ls_bcast_dfsize_50.txt

echo "Done!\n\n"