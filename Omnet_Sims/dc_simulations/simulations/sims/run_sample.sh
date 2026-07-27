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
echo "\n\n-------------------------------------------"
echo "Running Ring"
opp_runall -j50 ../../src/dc_simulations -m -u Cmdenv -c ring -n ..:../../src:../../../inet/src:../../../inet/examples:../../../inet/tutorials:../../../inet/showcases --image-path=../../../inet/images -l ../../../inet/src/INET omnetpp_bcast_sample.ini
do_extract ring_bcast
mkdir logs/ring_bcast_sample
cp results/*.out logs/ring_bcast_sample/



# Binary tree
echo "\n\n-------------------------------------------"
echo "Running Binary Tree"
opp_runall -j50 ../../src/dc_simulations -m -u Cmdenv -c binary_tree -n ..:../../src:../../../inet/src:../../../inet/examples:../../../inet/tutorials:../../../inet/showcases --image-path=../../../inet/images -l ../../../inet/src/INET omnetpp_bcast_sample.ini
do_extract tree_bcast
mkdir logs/tree_bcast_sample
cp results/*.out logs/tree_bcast_sample/



# Optimal
echo "\n\n-------------------------------------------"
echo "Running Optimal"
opp_runall -j50 ../../src/dc_simulations -m -u Cmdenv -c optimal -n ..:../../src:../../../inet/src:../../../inet/examples:../../../inet/tutorials:../../../inet/showcases --image-path=../../../inet/images -l ../../../inet/src/INET omnetpp_bcast_sample.ini
do_extract ace_bcast
mkdir logs/optimal_sample
cp results/*.out logs/optimal_sample/



# Orca
echo "\n\n-------------------------------------------"
echo "Running Orca"
opp_runall -j50 ../../src/dc_simulations -m -u Cmdenv -c orca -n ..:../../src:../../../inet/src:../../../inet/examples:../../../inet/tutorials:../../../inet/showcases --image-path=../../../inet/images -l ../../../inet/src/INET omnetpp_bcast_sample.ini
do_extract ace_bcast_orca
mkdir logs/orca_sample
cp results/*.out logs/orca_sample/



# Elmo
echo "\n\n-------------------------------------------"
echo "Running Elmo"
opp_runall -j50 ../../src/dc_simulations -m -u Cmdenv -c elmo -n ..:../../src:../../../inet/src:../../../inet/examples:../../../inet/tutorials:../../../inet/showcases --image-path=../../../inet/images -l ../../../inet/src/INET omnetpp_bcast_sample.ini
do_extract ace_bcast_elmo
mkdir logs/elmo_sample
cp results/*.out logs/elmo_sample/



# Peel
echo "\n\n-------------------------------------------"
echo "Running Peel"
opp_runall -j50 ../../src/dc_simulations -m -u Cmdenv -c peel -n ..:../../src:../../../inet/src:../../../inet/examples:../../../inet/tutorials:../../../inet/showcases --image-path=../../../inet/images -l ../../../inet/src/INET omnetpp_bcast_sample.ini
do_extract ace_bcast_peel
mkdir logs/peel_sample
cp results/*.out logs/peel_sample/