echo "Extracting dist files!"

mkdir distributions

mv peel_workload_sample_bcast/* distributions/

rm -rf peel_workload_sample_bcast

bunzip2 -vf ./distributions/*.bz2

echo "Done!"