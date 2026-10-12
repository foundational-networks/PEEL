#!/bin/bash

do_extract () {
    python3 ./extractor_shell_creator.py $1

    pushd ./results/
    bash extractor.sh
    popd

    sleep 5
}

# Start from a clean sample run.
rm -rf results
rm -rf logs
rm -rf extracted_results

# Create directories for extracted results and logs.
bash dir_creator.sh


# Ring
echo -e "\n\n-------------------------------------------"
echo -e "Running Ring"

opp_runall -j50 ../../src/dc_simulations \
    -m -u Cmdenv \
    -c ring \
    -n ..:../../src:../../../inet/src:../../../inet/examples:../../../inet/tutorials:../../../inet/showcases \
    --image-path=../../../inet/images \
    -l ../../../inet/src/INET \
    omnetpp_bcast_sample_hpn.ini

do_extract ring_bcast

mkdir logs/ring_bcast_sample
cp results/*.out logs/ring_bcast_sample/


# Binary Tree
echo -e "\n\n-------------------------------------------"
echo -e "Running Binary Tree"

opp_runall -j50 ../../src/dc_simulations \
    -m -u Cmdenv \
    -c binary_tree \
    -n ..:../../src:../../../inet/src:../../../inet/examples:../../../inet/tutorials:../../../inet/showcases \
    --image-path=../../../inet/images \
    -l ../../../inet/src/INET \
    omnetpp_bcast_sample_hpn.ini

do_extract tree_bcast

mkdir logs/tree_bcast_sample
cp results/*.out logs/tree_bcast_sample/


# Optimal
echo -e "\n\n-------------------------------------------"
echo -e "Running Optimal"

opp_runall -j50 ../../src/dc_simulations \
    -m -u Cmdenv \
    -c optimal \
    -n ..:../../src:../../../inet/src:../../../inet/examples:../../../inet/tutorials:../../../inet/showcases \
    --image-path=../../../inet/images \
    -l ../../../inet/src/INET \
    omnetpp_bcast_sample_hpn.ini

do_extract mcast_optimal_bcast

mkdir logs/mcast_optimal_sample
cp results/*.out logs/mcast_optimal_sample/


# Orca
echo -e "\n\n-------------------------------------------"
echo -e "Running Orca"

opp_runall -j50 ../../src/dc_simulations \
    -m -u Cmdenv \
    -c orca \
    -n ..:../../src:../../../inet/src:../../../inet/examples:../../../inet/tutorials:../../../inet/showcases \
    --image-path=../../../inet/images \
    -l ../../../inet/src/INET \
    omnetpp_bcast_sample_hpn.ini

do_extract mcast_bcast_orca

mkdir logs/orca_sample
cp results/*.out logs/orca_sample/


# PEEL
echo -e "\n\n-------------------------------------------"
echo -e "Running PEEL"

opp_runall -j50 ../../src/dc_simulations \
    -m -u Cmdenv \
    -c peel \
    -n ..:../../src:../../../inet/src:../../../inet/examples:../../../inet/tutorials:../../../inet/showcases \
    --image-path=../../../inet/images \
    -l ../../../inet/src/INET \
    omnetpp_bcast_sample_hpn.ini

do_extract mcast_bcast_peel

mkdir logs/peel_sample
cp results/*.out logs/peel_sample/

