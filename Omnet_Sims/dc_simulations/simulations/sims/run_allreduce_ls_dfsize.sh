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
opp_runall -j50 ../../src/dc_simulations -m -u Cmdenv -c ring -n ..:../../src:../../../inet/src:../../../inet/examples:../../../inet/tutorials:../../../inet/showcases --image-path=../../../inet/images -l ../../../inet/src/INET omnetpp_allreduce_baseLS.ini
do_extract ring_allreduce
mkdir logs/ring_allreduce_ls_50
cp results/*.out logs/ring_allreduce_ls_50/



# Binary tree
echo -e "\n\n-------------------------------------------"
echo -e "Running Binary Tree"
opp_runall -j50 ../../src/dc_simulations -m -u Cmdenv -c binary_tree -n ..:../../src:../../../inet/src:../../../inet/examples:../../../inet/tutorials:../../../inet/showcases --image-path=../../../inet/images -l ../../../inet/src/INET omnetpp_allreduce_baseLS.ini
do_extract tree_allreduce
mkdir logs/tree_allreduce_ls_50
cp results/*.out logs/tree_allreduce_ls_50/



# Optimal
echo -e "\n\n-------------------------------------------"
echo -e "Running Optimal"
opp_runall -j50 ../../src/dc_simulations -m -u Cmdenv -c optimal -n ..:../../src:../../../inet/src:../../../inet/examples:../../../inet/tutorials:../../../inet/showcases --image-path=../../../inet/images -l ../../../inet/src/INET omnetpp_allreduce_baseLS.ini
do_extract mcast_optimal_srcreduce
mkdir logs/mcast_optimal_allreduce_ls_50
cp results/*.out logs/mcast_optimal_allreduce_ls_50/



# Orca
echo -e "\n\n-------------------------------------------"
echo -e "Running Orca"
opp_runall -j50 ../../src/dc_simulations -m -u Cmdenv -c orca -n ..:../../src:../../../inet/src:../../../inet/examples:../../../inet/tutorials:../../../inet/showcases --image-path=../../../inet/images -l ../../../inet/src/INET omnetpp_allreduce_baseLS.ini
do_extract mcast_orca_srcreduce
mkdir logs/orca_allreduce_ls_50
cp results/*.out logs/orca_allreduce_ls_50/



# Elmo
echo -e "\n\n-------------------------------------------"
echo -e "Running Elmo"
opp_runall -j50 ../../src/dc_simulations -m -u Cmdenv -c elmo -n ..:../../src:../../../inet/src:../../../inet/examples:../../../inet/tutorials:../../../inet/showcases --image-path=../../../inet/images -l ../../../inet/src/INET omnetpp_allreduce_baseLS.ini
do_extract mcast_elmo_srcreduce
mkdir logs/elmo_allreduce_ls_50
cp results/*.out logs/elmo_allreduce_ls_50/



# Peel
echo -e "\n\n-------------------------------------------"
echo -e "Running Peel"
opp_runall -j50 ../../src/dc_simulations -m -u Cmdenv -c peel -n ..:../../src:../../../inet/src:../../../inet/examples:../../../inet/tutorials:../../../inet/showcases --image-path=../../../inet/images -l ../../../inet/src/INET omnetpp_allreduce_baseLS.ini
do_extract mcast_peel_srcreduce
mkdir logs/peel_allreduce_ls_50
cp results/*.out logs/peel_allreduce_ls_50/



# Optireduce
echo -e "\n\n-------------------------------------------"
echo -e "Running Optireduce"
opp_runall -j50 ../../src/dc_simulations -m -u Cmdenv -c optireduce -n ..:../../src:../../../inet/src:../../../inet/examples:../../../inet/tutorials:../../../inet/showcases --image-path=../../../inet/images -l ../../../inet/src/INET omnetpp_allreduce_baseLS.ini
do_extract optireduce
mkdir logs/optireduce_allreduce_ls_50
cp results/*.out logs/optireduce_allreduce_ls_50/



# Optireduce + Optimal multicast
echo -e "\n\n-------------------------------------------"
echo -e "Running Optireduce + Optimal multicast"
opp_runall -j50 ../../src/dc_simulations -m -u Cmdenv -c optireduce_optimal -n ..:../../src:../../../inet/src:../../../inet/examples:../../../inet/tutorials:../../../inet/showcases --image-path=../../../inet/images -l ../../../inet/src/INET omnetpp_allreduce_baseLS.ini
do_extract optireduce_mcast_optimal
mkdir logs/optireduce_mcast_optimal_allreduce_ls_50
cp results/*.out logs/optireduce_mcast_optimal_allreduce_ls_50/



# Optireduce + peel
echo -e "\n\n-------------------------------------------"
echo -e "Running Optireduce + Optimal multicast"
opp_runall -j50 ../../src/dc_simulations -m -u Cmdenv -c optireduce_peel -n ..:../../src:../../../inet/src:../../../inet/examples:../../../inet/tutorials:../../../inet/showcases --image-path=../../../inet/images -l ../../../inet/src/INET omnetpp_allreduce_baseLS.ini
do_extract optireduce_mcast_peel
mkdir logs/optireduce_mcast_peel_allreduce_ls_50
cp results/*.out logs/optireduce_mcast_peel_allreduce_ls_50/



# sharp
echo -e "\n\n-------------------------------------------"
echo -e "Running SHArP"
opp_runall -j50 ../../src/dc_simulations -m -u Cmdenv -c sharp -n ..:../../src:../../../inet/src:../../../inet/examples:../../../inet/tutorials:../../../inet/showcases --image-path=../../../inet/images -l ../../../inet/src/INET omnetpp_allreduce_baseLS.ini
do_extract ina_sharp
mkdir logs/ina_sharp_allreduce_ls_50
cp results/*.out logs/ina_sharp_allreduce_ls_50/