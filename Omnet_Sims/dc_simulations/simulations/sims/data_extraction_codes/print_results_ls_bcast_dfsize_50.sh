python3 Compressor.py

echo -e "\n\nPrinting results into archive/ls_bcast_dfsize_50.txt\n"

cp VARIABLES_ls_bcast_dfsize_50.py VARIABLES.py
python3 response_time_constant_load.py > archive/ls_bcast_dfsize_50.txt
python plotter_linechart.py --input_file_path archive/ls_bcast_dfsize_50.txt --output_fig_path figs/ls_bcast_dfsize_50 --cases 2 8 32 128 512 --x_axis_title "Message Size (MB)"

echo "Done!\n\n"