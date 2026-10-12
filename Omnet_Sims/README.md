# PEEL with Ring-Based Loss Recovery

This branch supports simulations of PEEL alongside ring-based loss recovery.

First, follow the initial setup instructions in the `Omnet_Sims/README.md` of the main branch to install the required dependencies and OMNeT++, and build the project.

Once the initial setup is complete, switch to this branch and run the following commands from the root of the `PEEL` repository to build the simulation files and run the experiments:

```bash
cd PEEL/Omnet_Sims/
bash build.sh

cd dc_simulations/simulations/sims/
bash extract_dist_files_ls_bcast_dfsize_50.sh
./run_bcast_ls_lossrecovery_50.sh
```

The commands above build the simulation files, download the required distribution files, and run the leaf-spine Broadcast simulations with PEEL and ring-based loss recovery.
