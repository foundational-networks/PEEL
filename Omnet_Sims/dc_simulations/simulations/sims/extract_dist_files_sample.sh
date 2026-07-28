echo "Extracting dist files!"

mkdir distributions

cp peel_workload_sample_bcast/* distributions/

bunzip2 -vf ./distributions/*.bz2

echo "Done!"