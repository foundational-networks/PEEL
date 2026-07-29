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
echo -e "Running Ring"
opp_runall -j50 ../../src/dc_simulations -m -u Cmdenv -c ring -n ..:../../src:../../../inet/src:../../../inet/examples:../../../inet/tutorials:../../../inet/showcases --image-path=../../../inet/images -l ../../../inet/src/INET omnetpp_allgather_baseLS.ini
do_extract ring
mkdir logs/ring_allgather_ls_50
cp results/*.out logs/ring_allgather_ls_50/



# Binary tree
echo -e "\n\n-------------------------------------------"
echo -e "Running Binary Tree"
opp_runall -j50 ../../src/dc_simulations -m -u Cmdenv -c binary_tree -n ..:../../src:../../../inet/src:../../../inet/examples:../../../inet/tutorials:../../../inet/showcases --image-path=../../../inet/images -l ../../../inet/src/INET omnetpp_allgather_baseLS.ini
do_extract tree
mkdir logs/tree_allgather_ls_50
cp results/*.out logs/tree_allgather_ls_50/



# Optimal
echo -e "\n\n-------------------------------------------"
echo -e "Running Optimal"
opp_runall -j50 ../../src/dc_simulations -m -u Cmdenv -c optimal -n ..:../../src:../../../inet/src:../../../inet/examples:../../../inet/tutorials:../../../inet/showcases --image-path=../../../inet/images -l ../../../inet/src/INET omnetpp_allgather_baseLS.ini
do_extract mcast_optimal
mkdir logs/mcast_optimal_allgather_ls_50
cp results/*.out logs/mcast_optimal_allgather_ls_50/



# Orca
echo -e "\n\n-------------------------------------------"
echo -e "Running Orca"
opp_runall -j50 ../../src/dc_simulations -m -u Cmdenv -c orca -n ..:../../src:../../../inet/src:../../../inet/examples:../../../inet/tutorials:../../../inet/showcases --image-path=../../../inet/images -l ../../../inet/src/INET omnetpp_allgather_baseLS.ini
do_extract mcast_orca
mkdir logs/orca_allgather_ls_50
cp results/*.out logs/orca_allgather_ls_50/



# Elmo
echo -e "\n\n-------------------------------------------"
echo -e "Running Elmo"
opp_runall -j50 ../../src/dc_simulations -m -u Cmdenv -c elmo -n ..:../../src:../../../inet/src:../../../inet/examples:../../../inet/tutorials:../../../inet/showcases --image-path=../../../inet/images -l ../../../inet/src/INET omnetpp_allgather_baseLS.ini
do_extract mcast_elmo
mkdir logs/elmo_allgather_ls_50
cp results/*.out logs/elmo_allgather_ls_50/



# Peel
echo -e "\n\n-------------------------------------------"
echo -e "Running Peel"
opp_runall -j50 ../../src/dc_simulations -m -u Cmdenv -c peel -n ..:../../src:../../../inet/src:../../../inet/examples:../../../inet/tutorials:../../../inet/showcases --image-path=../../../inet/images -l ../../../inet/src/INET omnetpp_allgather_baseLS.ini
do_extract mcast_peel
mkdir logs/peel_allgather_ls_50
cp results/*.out logs/peel_allgather_ls_50/