python3 Compressor.py

echo -e "Printing results into archive/sample.txt\n"

cp VARIABLES_sample.py VARIABLES.py
python3 response_time_constant_load.py > archive/sample.txt

echo "Done!\n"