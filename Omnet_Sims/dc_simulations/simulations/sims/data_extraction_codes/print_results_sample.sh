python3 Compressor.py

echo "Printing results into archive/sample.txt"

cp VARIABLES_sample.py VARIABLES.py
python3 response_time_constant_load.py > archive/sample.txt

echo "Done!"