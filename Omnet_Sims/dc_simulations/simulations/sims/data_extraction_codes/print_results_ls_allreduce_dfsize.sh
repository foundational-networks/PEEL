python3 Compressor.py

echo -e "\n\nPrinting results into archive/ls_allreduce_dfsize.txt\n"

cp VARIABLES_ls_allreduce_dfsize.py VARIABLES.py
python3 response_time_constant_load.py > archive/ls_allreduce_dfsize.txt

echo "Done!\n\n"