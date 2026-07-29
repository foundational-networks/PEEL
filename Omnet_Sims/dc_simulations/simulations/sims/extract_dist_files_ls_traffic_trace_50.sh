echo "Extracting dist files!"

mkdir distributions

cp peel_workload_ls_traffic_trace_50/* distributions/

bunzip2 -vf ./distributions/*.bz2

echo "Done!"