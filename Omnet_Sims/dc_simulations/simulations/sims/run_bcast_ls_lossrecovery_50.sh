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
opp_runall -j50 ../../src/dc_simulations -m -u Cmdenv -c ring -n ..:../../src:../../../inet/src:../../../inet/examples:../../../inet/tutorials:../../../inet/showcases --image-path=../../../inet/images -l ../../../inet/src/INET omnetpp_bcast_base_loss_recovery.ini
do_extract ring_bcast
mkdir logs/ring_bcast_ls_50_lossrecovery
cp results/*.out logs/ring_bcast_ls_50_lossrecovery/



# Peel
echo -e "\n\n-------------------------------------------"
echo -e "Running Peel"
opp_runall -j50 ../../src/dc_simulations -m -u Cmdenv -c peel -n ..:../../src:../../../inet/src:../../../inet/examples:../../../inet/tutorials:../../../inet/showcases --image-path=../../../inet/images -l ../../../inet/src/INET omnetpp_bcast_base_loss_recovery.ini
do_extract mcast_bcast_peel
mkdir logs/peel_bcast_ls_50_lossrecovery
cp results/*.out logs/peel_bcast_ls_50_lossrecovery/