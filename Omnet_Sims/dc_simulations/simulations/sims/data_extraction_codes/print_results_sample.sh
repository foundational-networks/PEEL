python3 Compressor.py

echo -e "\n\nPrinting results into archive/sample.txt\n"

cp VARIABLES_sample.py VARIABLES.py
python3 response_time_constant_load.py > archive/sample.txt
python plotter_linechart.py --input_file_path archive/sample.txt --output_fig_path figs/sample --cases 8 --x_axis_title "Message Size (MB)"

echo "Done!\n\n"