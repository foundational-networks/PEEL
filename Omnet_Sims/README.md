# Omnet++ simulation files

This repository provides the instructions and files required to run the simulations and extract the results. We use the [Omnet++ simulator](https://omnetpp.org/) and INET framework to run the simulations. The [Omnet++ manual](https://doc.omnetpp.org/omnetpp/manual/) and its examples are good references for an introduction to the simulator.

This branch contains the simulation setup for evaluating PEEL in a hierarchical rail-optimized datacenter topology (HPN).

We ran our simulations on Ubuntu machines (version: 18.04), so all commands below are for Ubuntu.

To run the simulations, follow these steps:

* [Step 1: Install dependencies](#step-1-installing-dependencies)
* [Step 2: Install Omnet++](#step-2-installing-omnet)
* [Step 3: Build the project](#step-3-building-the-project)
* [Small-scale simulations](#small-scale-simulations-extra-step-for-evaluation-in-a-short-time)
* [Large-scale simulations](#step-4-running-key-large-scale-simulations-and-extracting-the-results)

---

### Step 1: Installing dependencies

To successfully install Omnet++ and run the simulations, execute the following commands:

```bash
sudo apt update
sudo apt-get install -y build-essential
sudo apt-get install -y flex bison
sudo apt-get install -y zlib1g-dev
sudo apt-get install -y libxml2-dev
sudo apt install -y sqlite3
sudo apt-get install -y libsqlite3-dev
sudo apt-get install -y zip unzip
```

Make sure you have Python 3 installed. If not, run:

```bash
sudo apt-get install -y python3
```

If Python 3 is installed and accessible at `/usr/bin/python3`, run:

```bash
sudo rm -rf /usr/bin/python
sudo ln -s /usr/bin/python3 /usr/bin/python
sudo apt install -y python3-numpy
sudo apt-get install -y python3-matplotlib
```

---

### Step 2: Installing Omnet++

The complete instructions for installing Omnet++ can be found in the [Omnet++ installation guide](https://doc.omnetpp.org/omnetpp/InstallGuide.pdf).

**If you already have the simulator installed, you can skip this step.**

To install Omnet++ 5.6.2 on Ubuntu:

```bash
wget https://github.com/omnetpp/omnetpp/releases/download/omnetpp-5.6.2/omnetpp-5.6.2-src-linux.tgz
tar xvfz omnetpp-5.6.2-src-linux.tgz
cd omnetpp-5.6.2/
. setenv
```

For the next command, we assume that Omnet++ was extracted under `$HOME`. If this is not the case, replace `$HOME` with the appropriate path.

```bash
echo "export PATH=$HOME/omnetpp-5.6.2/bin:\$PATH" >> ~/.bashrc
```

Then run:

```bash
source ~/.bashrc
./configure WITH_QTENV=no WITH_OSG=no WITH_OSGEARTH=no
```

If the configure command reports that `omnetpp-5.6.2/bin` is not in your `PATH`, close and reopen the terminal and run the configure command again.

Finally:

```bash
make
```

---

### Step 3: Building the project

The HPN sample workload is included as a Git submodule. From the root of the PEEL repository, initialize the submodules:

```bash
git submodule update --init --recursive
```

We provide a `build.sh` script to build the simulator modules.

```bash
cd PEEL/Omnet_Sims/
bash build.sh
```

---

### Small-scale simulations: Extra step for evaluation in a short time

We provide a small HPN Broadcast experiment for evaluating the simulator without running the complete large-scale experiments.

The sample compares the following communication algorithms:

* Ring
* Binary Tree
* Optimal multicast
* Orca
* PEEL

The sample workload is provided through the `peel_workload_sample_bcast_hpn` Git submodule.

First, move to the simulation directory:

```bash
cd PEEL/Omnet_Sims/dc_simulations/simulations/sims
```

Prepare the HPN workload files:

```bash
bash extract_dist_files_sample_hpn.sh
```

Run all five sample configurations and extract the raw simulation statistics:

```bash
./run_sample_hpn.sh
```

After the simulations complete, copy the result-processing scripts into the extracted-results directory:

```bash
cp data_extraction_codes/* extracted_results/
cd extracted_results
```

Then compute and print the Collective Completion Time (CCT):

```bash
./print_results_sample_hpn.sh
```

The HPN sample uses the configurations defined in:

```text
omnetpp_bcast_sample_hpn.ini
```

The five configurations correspond to:

| Configuration | Algorithm |
| --- | --- |
| `ring` | Ring |
| `binary_tree` | Binary Tree |
| `optimal` | Optimal multicast |
| `orca` | Orca |
| `peel` | PEEL |

The processed results are stored in:

```text
PEEL/Omnet_Sims/dc_simulations/simulations/sims/extracted_results/archive/sample_hpn.txt
```

The output should look similar to the following:

```text
ring_bcast
Mean CCT (s): 0.000356191
p99 CCT (s): 0.000356191


tree_bcast
Mean CCT (s): 0.000619191
p99 CCT (s): 0.000619191


mcast_optimal_bcast
Mean CCT (s): 0.000088555
p99 CCT (s): 0.000088555


mcast_bcast_orca
Mean CCT (s): 0.000199077
p99 CCT (s): 0.000199077


mcast_bcast_peel
Mean CCT (s): 0.000067544
p99 CCT (s): 0.000067544
```

The sample contains a single Broadcast operation for each configuration, so the mean and p99 CCT are identical.

---

### Step 4: Running key large-scale simulations and extracting the results

The config files for large-scale simulations can be used for evaluating various collective operations and algorithms in the HPN topology. To run the large-scale simulations under distinct collective algorithms for a specific collective operation, first make sure that you are in the right directory (`PEEL/Omnet_Sims/dc_simulations/simulations/sims`) and then use the following commands to extract the distribution files, run the simulations, extract the results, and plot the figures:

```bash
bash $DIST_EXTRACTOR.sh
./$EXPERIMENT_RUN_FILE.sh
cp data_extraction_codes/* extracted_results/
cd extracted_results
./$EXPERIMENT_PRINT_FILE.sh
```

The table below summarizes the appropriate `$DIST_EXTRACTOR`, `$EXPERIMENT_RUN_FILE`, and `$EXPERIMENT_PRINT_FILE` for each experiment:

Experiment | $DIST_EXTRACTOR | $EXPERIMENT_RUN_FILE | $EXPERIMENT_PRINT_FILE
--- | --- | --- | ---
Broadcast operations in HPN | extract_dist_files_hpn_bcast_dfsize_50 | run_bcast_hpn_dfsize_50 | print_results_hpn_bcast_dfsize_50
All-Gather operations in HPN | extract_dist_files_hpn_allgather_allreduce_dfsize | run_allgather_hpn_dfsize | print_results_hpn_allgather_dfsize
All-Reduce operations in HPN | extract_dist_files_hpn_allgather_allreduce_dfsize | run_allreduce_hpn_dfsize | print_results_hpn_allreduce_dfsize

The commands above run each experiment and store the results in `PEEL/Omnet_Sims/dc_simulations/simulations/sims/extracted_results/archive` and figures in `PEEL/Omnet_Sims/dc_simulations/simulations/sims/extracted_results/figs`. Large-scale simulations can take significantly longer than the sample experiment. We refer you to the **small-scale simulations** section if you would like to evaluate the simulator in a shorter amount of time.

---