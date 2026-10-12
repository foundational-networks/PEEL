python3 Compressor.py

echo -e "\n\nPrinting results into archive/hpn_allreduce_dfsize.txt\n"

cp VARIABLES_hpn_allreduce_dfsize.py VARIABLES.py
python3 response_time_constant_load_hpn.py > archive/hpn_allreduce_dfsize.txt
python3 plotter_linechart.py --input_file_path archive/hpn_allreduce_dfsize.txt --output_fig_path figs/hpn_allreduce_dfsize --cases 2 8 32 64 128 512 --x_axis_title "Message Size (MB)"

echo "Done!\n\n"
