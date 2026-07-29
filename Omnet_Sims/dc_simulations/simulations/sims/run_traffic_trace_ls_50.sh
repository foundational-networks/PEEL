#!/bin/bash


do_extract () {
    python3 ./extractor_shell_creator.py $1
    pushd ./results/
    bash extractor.sh
    popd
    sleep 5
}

rm -rf results

# create the directory to save extracted_results
bash dir_creator.sh

# Ring
echo -e "\n\n-------------------------------------------"
echo -e "Running Baseline"
opp_runall -j50 ../../src/dc_simulations -m -u Cmdenv -c baseline -n ..:../../src:../../../inet/src:../../../inet/examples:../../../inet/tutorials:../../../inet/showcases --image-path=../../../inet/images -l ../../../inet/src/INET omnetpp_combination_base.ini
do_extract baseline
mkdir logs/baseline_trace_ls_50
cp results/*.out logs/baseline_trace_ls_50/



# Binary tree
echo -e "\n\n-------------------------------------------"
echo -e "Running Baseline AUTOCCL"
opp_runall -j50 ../../src/dc_simulations -m -u Cmdenv -c baseline_autoccl -n ..:../../src:../../../inet/src:../../../inet/examples:../../../inet/tutorials:../../../inet/showcases --image-path=../../../inet/images -l ../../../inet/src/INET omnetpp_combination_base.ini
do_extract baseline_autoccl
mkdir logs/autoccl_trace_ls_50
cp results/*.out logs/autoccl_trace_ls_50/



# Optimal
echo -e "\n\n-------------------------------------------"
echo -e "Running Optimal"
opp_runall -j50 ../../src/dc_simulations -m -u Cmdenv -c optimal -n ..:../../src:../../../inet/src:../../../inet/examples:../../../inet/tutorials:../../../inet/showcases --image-path=../../../inet/images -l ../../../inet/src/INET omnetpp_combination_base.ini
do_extract mcast_optimal
mkdir logs/mcast_optimal_trace_ls_50
cp results/*.out logs/mcast_optimal_trace_ls_50/



# Peel
echo -e "\n\n-------------------------------------------"
echo -e "Running Peel"
opp_runall -j50 ../../src/dc_simulations -m -u Cmdenv -c peel -n ..:../../src:../../../inet/src:../../../inet/examples:../../../inet/tutorials:../../../inet/showcases --image-path=../../../inet/images -l ../../../inet/src/INET omnetpp_combination_base.ini
do_extract mcast_peel
mkdir logs/peel_trace_ls_50
cp results/*.out logs/peel_trace_ls_50/