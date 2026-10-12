# HPN codebase

First, follow the initial setup instructions in the `Omnet_Sims/README.md` of the main branch to install the required dependencies and Omnet++.

Once the initial setup is complete, switch to this branch and run the following commands from the root of the `PEEL` repository to build the simulation files and run the experiments.

---

### Building the project

We have provided a `build.sh` shell script to simplify this process. To build the project modules, run:

```bash
cd PEEL/Omnet_Sims/
bash build.sh
```

---

### Small-scale simulations: Extra step for evaluation in a short time

The config files for large-scale simulations can take a long time to finish. Accordingly, we provide a small-scale simulation for quickly validating the HPN setup. The sample runs a 2 MB Broadcast collective among 64 participants and compares Ring, Binary Tree, Optimal multicast, Orca, and PEEL.

To run the small-scale simulation, first make sure that you are in the right directory (`PEEL/Omnet_Sims/dc_simulations/simulations/sims`) and then use the following commands to extract the distribution files, run the simulations, and extract the results:

```bash
bash extract_dist_files_sample_hpn.sh
./run_sample_hpn.sh
cp data_extraction_codes/* extracted_results/
cd extracted_results
./print_results_sample_hpn.sh
```

The commands above run the HPN Broadcast sample and store the results in `PEEL/Omnet_Sims/dc_simulations/simulations/sims/extracted_results/archive`.

For the provided sample configuration, the expected Collective Completion Time (CCT) is approximately:

```text
Ring:              Mean CCT = 0.000356191 s, p99 CCT = 0.000356191 s
Binary Tree:       Mean CCT = 0.000619191 s, p99 CCT = 0.000619191 s
Optimal multicast: Mean CCT = 0.000088555 s, p99 CCT = 0.000088555 s
Orca:              Mean CCT = 0.000199077 s, p99 CCT = 0.000199077 s
PEEL:              Mean CCT = 0.000067544 s, p99 CCT = 0.000067544 s
```

Since the sample contains a single Broadcast operation for each configuration, the mean and p99 CCT are identical.

---

### Running the large-scale simulations and extracting the results

The config files for large-scale simulations can be used for evaluating Broadcast, All-Gather, and All-Reduce operations in HPN.

To run the large-scale simulations, first make sure that you are in the right directory (`PEEL/Omnet_Sims/dc_simulations/simulations/sims`) and then use the following commands to extract the distribution files, run the simulations, extract the results, and plot the figures:

```bash
bash $DIST_DOWNLOADER.sh
./$EXPERIMENT_RUN_FILE.sh
cp data_extraction_codes/* extracted_results/
cd extracted_results
./$EXPERIMENT_PRINT_FILE.sh
```

The table below summarizes the appropriate `$DIST_DOWNLOADER`, `$EXPERIMENT_RUN_FILE`, and `$EXPERIMENT_PRINT_FILE` for the HPN experiments:

Experiment | $DIST_DOWNLOADER | $EXPERIMENT_RUN_FILE | $EXPERIMENT_PRINT_FILE
--- | --- | --- | ---
HPN Broadcast | extract_dist_files_hpn_bcast_dfsize_50 | run_bcast_hpn_dfsize_50 | print_results_hpn_bcast_dfsize_50
HPN All-Gather | extract_dist_files_hpn_allgather_allreduce_dfsize | run_allgather_hpn_dfsize | print_results_hpn_allgather_dfsize
HPN All-Reduce | extract_dist_files_hpn_allgather_allreduce_dfsize | run_allreduce_hpn_dfsize | print_results_hpn_allreduce_dfsize

The commands above run each experiment and store the results in `PEEL/Omnet_Sims/dc_simulations/simulations/sims/extracted_results/archive` and figures in `PEEL/Omnet_Sims/dc_simulations/simulations/sims/extracted_results/figs`.

We refer you to the **small-scale simulations** section if you would like to validate the HPN setup and see results in a short time.

The provided run scripts compare Ring, Binary Tree, Optimal multicast, Orca, and PEEL.