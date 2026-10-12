# RDMC codebase

First, follow the initial setup instructions in the `Omnet_Sims/README.md` of the main branch to install the required dependencies and OMNeT++.

Once the initial setup is complete, switch to this branch and run the following commands from the root of the `PEEL` repository to build the simulation files and run the experiments:

---

### Building the project

We have provided a ```build.sh``` shell script to simplify this process. To build the project modules and download the distribution files, run the following script:

```bash
cd PEEL/Omnet_Sims/
bash build.sh
```

---

### Small-scale simulations: Extra step for evaluation in a short time

The config files for large-scale simulations can take a long time to finish. Accordingly, we provide a small-scale simulation for quickly validating the RDMC implementation. The sample runs an 8 MB Broadcast collective among 64 participants using RDMC.

To run the small-scale simulation, first make sure that you are in the right directory (```PEEL/Omnet_Sims/dc_simulations/simulations/sims```) and then use the following commands to download the distribution files, run the simulation, extract the results, and plot the figures:

```bash
bash extract_dist_files_sample.sh
./run_sample.sh
cp data_extraction_codes/* extracted_results/
cd extracted_results
./print_results_sample.sh
```

The commands above run the RDMC Broadcast sample and store the results in ```PEEL/Omnet_Sims/dc_simulations/simulations/sims/extracted_results/archive``` and figures in ```PEEL/Omnet_Sims/dc_simulations/simulations/sims/extracted_results/figs```.

For the provided sample configuration, the expected Collective Completion Time (CCT) is approximately:

```text
RDMC Broadcast (8 MB): Mean CCT = 0.001726 s, p99 CCT = 0.001726 s
```

Since the sample contains a single repetition, the mean and p99 CCT are identical.

---

### Running the large-scale simulations and extracting the results

The config files for large-scale simulations can be used for evaluating RDMC Broadcast. To run the large-scale simulations, first make sure that you are in the right directory (```PEEL/Omnet_Sims/dc_simulations/simulations/sims```) and then use the following commands to download the distribution files, run the simulations, extract the results, and plot the figures:

```bash
bash $DIST_DOWNLOWDER.sh
./$EXPERIMENT_RUN_FILE.sh
cp data_extraction_codes/* extracted_results/
cd extracted_results
./$EXPERIMENT_PRINT_FILE.sh
```

The table below summarizes the appropriate ```$DIST_DOWNLOWDER```, ```$EXPERIMENT_RUN_FILE```, and ```$EXPERIMENT_PRINT_FILE``` for the RDMC Broadcast experiments:

Experiment | $DIST_DOWNLOWDER | $EXPERIMENT_RUN_FILE | $EXPERIMENT_PRINT_FILE
--- | --- | --- | ---
RDMC Broadcast in leaf-spine | extract_dist_files_ls_bcast_dfsize_50 | run_bcast_ls_dfsize_50 | print_results_ls_bcast_dfsize_50
RDMC Broadcast in 8-ary fat-tree | extract_dist_files_ft_bcast_dfsize_50 | run_bcast_ft_dfsize_50 | print_results_ft_bcast_dfsize_50

The commands above run each experiment and store the results in ```PEEL/Omnet_Sims/dc_simulations/simulations/sims/extracted_results/archive``` and figures in ```PEEL/Omnet_Sims/dc_simulations/simulations/sims/extracted_results/figs```. We refer you to the **small-scale simulations** section if you would like to validate the RDMC setup and see results in a short time.

The provided run scripts use the `rdmc` configuration by default.
