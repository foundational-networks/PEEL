python3 Compressor.py

echo -e "\n\nPrinting results into archive/ls_allgather_dfsize.txt\n"

cp VARIABLES_ls_allgather_dfsize.py VARIABLES.py
python3 response_time_constant_load.py > archive/ls_allgather_dfsize.txt
python plotter_linechart.py --input_file_path archive/ls_allgather_dfsize.txt --output_fig_path figs/ls_allgather_dfsize --cases 2 8 32 128 512 --x_axis_title "Message Size (MB)"

echo "Done!\n\n"