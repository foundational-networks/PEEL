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

# This script runs RDMC by default.
# For hierarchical RDMC, use "-c rdmc_hier" instead of "-c rdmc" below and
# change "do_extract rdmc_bcast" to "do_extract rdmc_hier_bcast".

# RDMC
echo -e "\n\n-------------------------------------------"
echo -e "Running RDMC"
opp_runall -j50 ../../src/dc_simulations -m -u Cmdenv -c rdmc -n ..:../../src:../../../inet/src:../../../inet/examples:../../../inet/tutorials:../../../inet/showcases --image-path=../../../inet/images -l ../../../inet/src/INET omnetpp_bcast_baseLS_50.ini
do_extract rdmc_bcast
mkdir logs/rdmc_bcast_ls_50
cp results/*.out logs/rdmc_bcast_ls_50/
