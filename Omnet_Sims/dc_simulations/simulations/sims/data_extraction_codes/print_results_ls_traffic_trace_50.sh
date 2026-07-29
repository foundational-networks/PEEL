python3 Compressor.py

echo -e "\n\nPrinting results into archive/ls_traffic_trace_50.txt\n"

cp VARIABLES_ls_traffic_trace_50.py VARIABLES.py
python3 response_time_combination.py > archive/ls_traffic_trace_50.txt

echo "Done!\n\n"