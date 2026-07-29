echo "Extracting dist files!"

mkdir distributions

cp peel_workload_ft_allgather_allreduce_dfsize_50/* distributions/

bunzip2 -vf ./distributions/*.bz2

echo "Done!"