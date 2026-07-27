#!/bin/bash


do_extract () {
    python3 /home/sepehr/omnetpp-5.5.1/samples/dc_simulations/simulations/Two_Layer_Leaf_Spine/extractor_shell_creator.py $1
    pushd /home/sepehr/omnetpp-5.5.1/samples/dc_simulations/simulations/Two_Layer_Leaf_Spine/results/
    parallel < extractor.sh
    popd
    sleep 5
}

# echo "Running DCTCP_ECMP"
# /home/sepehr/omnetpp-5.5.1/bin/opp_runall -j1 /home/sepehr/omnetpp-5.5.1/samples/dc_simulations/src/dc_simulations -m -u Cmdenv -c DCTCP_ECMP -n ..:../../src:../../../../../Desktop/inet/src:../../../../../Desktop/inet/examples:../../../../../Desktop/inet/tutorials:../../../../../Desktop/inet/showcases --image-path=../../../../../Desktop/inet/images -l ../../../../../Desktop/inet/src/INET omnetpp_different_scales_pFabric.ini
# do_extract ecmp

echo "Running DCTCP_DRILL"
/home/sepehr/omnetpp-5.5.1/bin/opp_runall -j1 /home/sepehr/omnetpp-5.5.1/samples/dc_simulations/src/dc_simulations -m -u Cmdenv -c DCTCP_DRILL -n ..:../../src:../../../../../Desktop/inet/src:../../../../../Desktop/inet/examples:../../../../../Desktop/inet/tutorials:../../../../../Desktop/inet/showcases --image-path=../../../../../Desktop/inet/images -l ../../../../../Desktop/inet/src/INET omnetpp_different_scales_pFabric.ini
do_extract drill

echo "Running DCTCP_pFabric_ECMP"
/home/sepehr/omnetpp-5.5.1/bin/opp_runall -j1 /home/sepehr/omnetpp-5.5.1/samples/dc_simulations/src/dc_simulations -m -u Cmdenv -c DCTCP_pFabric_ECMP -n ..:../../src:../../../../../Desktop/inet/src:../../../../../Desktop/inet/examples:../../../../../Desktop/inet/tutorials:../../../../../Desktop/inet/showcases --image-path=../../../../../Desktop/inet/images -l ../../../../../Desktop/inet/src/INET omnetpp_different_scales_pFabric.ini
do_extract pFabric_ecmp

echo "Running DCTCP_pFabric_DRILL"
/home/sepehr/omnetpp-5.5.1/bin/opp_runall -j1 /home/sepehr/omnetpp-5.5.1/samples/dc_simulations/src/dc_simulations -m -u Cmdenv -c DCTCP_pFabric_DRILL -n ..:../../src:../../../../../Desktop/inet/src:../../../../../Desktop/inet/examples:../../../../../Desktop/inet/tutorials:../../../../../Desktop/inet/showcases --image-path=../../../../../Desktop/inet/images -l ../../../../../Desktop/inet/src/INET omnetpp_different_scales_pFabric.ini
do_extract pFabric

echo "Running DCTCP_V_SRPT_SCH_SRPT_ORD_ECMP"
/home/sepehr/omnetpp-5.5.1/bin/opp_runall -j1 /home/sepehr/omnetpp-5.5.1/samples/dc_simulations/src/dc_simulations -m -u Cmdenv -c DCTCP_V_SRPT_SCH_SRPT_ORD_ECMP -n ..:../../src:../../../../../Desktop/inet/src:../../../../../Desktop/inet/examples:../../../../../Desktop/inet/tutorials:../../../../../Desktop/inet/showcases --image-path=../../../../../Desktop/inet/images -l ../../../../../Desktop/inet/src/INET omnetpp_different_scales_pFabric.ini
do_extract srpt_sch_srpt_ord_ecmp

echo "Running DCTCP_V_SRPT_SCH_SRPT_ORD_DRILL"
/home/sepehr/omnetpp-5.5.1/bin/opp_runall -j1 /home/sepehr/omnetpp-5.5.1/samples/dc_simulations/src/dc_simulations -m -u Cmdenv -c DCTCP_V_SRPT_SCH_SRPT_ORD_DRILL -n ..:../../src:../../../../../Desktop/inet/src:../../../../../Desktop/inet/examples:../../../../../Desktop/inet/tutorials:../../../../../Desktop/inet/showcases --image-path=../../../../../Desktop/inet/images -l ../../../../../Desktop/inet/src/INET omnetpp_different_scales_pFabric.ini
do_extract srpt_sch_srpt_ord

echo "Running DCTCP_V_SRPT_BOU_SRPT_ORD_ECMP"
/home/sepehr/omnetpp-5.5.1/bin/opp_runall -j1 /home/sepehr/omnetpp-5.5.1/samples/dc_simulations/src/dc_simulations -m -u Cmdenv -c DCTCP_V_SRPT_BOU_SRPT_ORD_ECMP -n ..:../../src:../../../../../Desktop/inet/src:../../../../../Desktop/inet/examples:../../../../../Desktop/inet/tutorials:../../../../../Desktop/inet/showcases --image-path=../../../../../Desktop/inet/images -l ../../../../../Desktop/inet/src/INET omnetpp_different_scales_pFabric.ini
do_extract srpt_bou_srpt_ord_ecmp

echo "Running DCTCP_V_SRPT_BOU_SRPT_ORD_DRILL"
/home/sepehr/omnetpp-5.5.1/bin/opp_runall -j1 /home/sepehr/omnetpp-5.5.1/samples/dc_simulations/src/dc_simulations -m -u Cmdenv -c DCTCP_V_SRPT_BOU_SRPT_ORD_DRILL -n ..:../../src:../../../../../Desktop/inet/src:../../../../../Desktop/inet/examples:../../../../../Desktop/inet/tutorials:../../../../../Desktop/inet/showcases --image-path=../../../../../Desktop/inet/images -l ../../../../../Desktop/inet/src/INET omnetpp_different_scales_pFabric.ini
do_extract srpt_bou_srpt_ord_drill

echo "Running TCP_pFabric_ECMP"
/home/sepehr/omnetpp-5.5.1/bin/opp_runall -j1 /home/sepehr/omnetpp-5.5.1/samples/dc_simulations/src/dc_simulations -m -u Cmdenv -c TCP_pFabric_ECMP -n ..:../../src:../../../../../Desktop/inet/src:../../../../../Desktop/inet/examples:../../../../../Desktop/inet/tutorials:../../../../../Desktop/inet/showcases --image-path=../../../../../Desktop/inet/images -l ../../../../../Desktop/inet/src/INET omnetpp_different_scales_pFabric.ini
do_extract tcp_pFabric_ecmp

echo "Running TCP_pFabric_DRILL"
/home/sepehr/omnetpp-5.5.1/bin/opp_runall -j1 /home/sepehr/omnetpp-5.5.1/samples/dc_simulations/src/dc_simulations -m -u Cmdenv -c TCP_pFabric_DRILL -n ..:../../src:../../../../../Desktop/inet/src:../../../../../Desktop/inet/examples:../../../../../Desktop/inet/tutorials:../../../../../Desktop/inet/showcases --image-path=../../../../../Desktop/inet/images -l ../../../../../Desktop/inet/src/INET omnetpp_different_scales_pFabric.ini
do_extract tcp_pFabric_drill

# echo "Running TCP_V_SRPT_SCH_SRPT_ORD_ECMP"
# /home/sepehr/omnetpp-5.5.1/bin/opp_runall -j1 /home/sepehr/omnetpp-5.5.1/samples/dc_simulations/src/dc_simulations -m -u Cmdenv -c TCP_V_SRPT_SCH_SRPT_ORD_ECMP -n ..:../../src:../../../../../Desktop/inet/src:../../../../../Desktop/inet/examples:../../../../../Desktop/inet/tutorials:../../../../../Desktop/inet/showcases --image-path=../../../../../Desktop/inet/images -l ../../../../../Desktop/inet/src/INET omnetpp_different_scales_pFabric.ini
# do_extract ecmp

# echo "Running TCP_V_SRPT_SCH_SRPT_ORD_DRILL"
# /home/sepehr/omnetpp-5.5.1/bin/opp_runall -j1 /home/sepehr/omnetpp-5.5.1/samples/dc_simulations/src/dc_simulations -m -u Cmdenv -c TCP_V_SRPT_SCH_SRPT_ORD_DRILL -n ..:../../src:../../../../../Desktop/inet/src:../../../../../Desktop/inet/examples:../../../../../Desktop/inet/tutorials:../../../../../Desktop/inet/showcases --image-path=../../../../../Desktop/inet/images -l ../../../../../Desktop/inet/src/INET omnetpp_different_scales_pFabric.ini
# do_extract ecmp

echo "Running TCP_V_SRPT_BOU_SRPT_ORD_ECMP"
/home/sepehr/omnetpp-5.5.1/bin/opp_runall -j1 /home/sepehr/omnetpp-5.5.1/samples/dc_simulations/src/dc_simulations -m -u Cmdenv -c TCP_V_SRPT_BOU_SRPT_ORD_ECMP -n ..:../../src:../../../../../Desktop/inet/src:../../../../../Desktop/inet/examples:../../../../../Desktop/inet/tutorials:../../../../../Desktop/inet/showcases --image-path=../../../../../Desktop/inet/images -l ../../../../../Desktop/inet/src/INET omnetpp_different_scales_pFabric.ini
do_extract tcp_srpt_bou_srpt_ord_ecmp

echo "Running TCP_V_SRPT_BOU_SRPT_ORD_DRILL"
/home/sepehr/omnetpp-5.5.1/bin/opp_runall -j1 /home/sepehr/omnetpp-5.5.1/samples/dc_simulations/src/dc_simulations -m -u Cmdenv -c TCP_V_SRPT_BOU_SRPT_ORD_DRILL -n ..:../../src:../../../../../Desktop/inet/src:../../../../../Desktop/inet/examples:../../../../../Desktop/inet/tutorials:../../../../../Desktop/inet/showcases --image-path=../../../../../Desktop/inet/images -l ../../../../../Desktop/inet/src/INET omnetpp_different_scales_pFabric.ini
do_extract tcp_srpt_bou_srpt_ord_drill

rm -rf /home/sepehr/omnetpp-5.5.1/samples/dc_simulations/simulations/Two_Layer_Leaf_Spine/results/

echo "Experiments Finished!"
