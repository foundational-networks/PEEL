# Omnet++ simulation files

This repository provides the instructions and files required to run the simulations and extract the results. We use [Omnet++ simulator](https://omnetpp.org/) and INET framework to run the simulations. [Omnet++ manual](https://doc.omnetpp.org/omnetpp/manual/) and its examples are good references for an introduction to the simulator. We ran our simulations on Ubuntu machines (version: 18.04), so all the commands are for Ubuntu. To run the simulations, you should follow the following steps:

* [Step 1: Install dependencies](#step-1-installing-dependencies) (~ 5 minutes)
* [Step 2: Install Omnet++](#step-2-installing-omnet)  (~ 5 minutes)
* [Step 3: Clone the repository](#step-3-cloning-the-repository) (~ 5 minutes)
* [Step 4: Build the project](#step-4-building-the-project) (~ 5 minutes)
* [Step 5: Run the simulations and extract the results](#small-scale-simulations-extra-step-for-evaluation-in-a-short-time) (This step can take from a few minutes to a few weeks depending on the simulations that you run)

**Note:** For both the small-scale “Hello World” example and the complete large-scale simulations, follow Steps 1–5 to install the simulator and set up the experimental environment. After completing these steps, you can either run the [small-scale simulation commands](#small-scale-simulations-extra-step-for-evaluation-in-a-short-time) for quick testing or [execute the large-scale simulation files](#step-6-running-the-large-scale-simulations-and-extracting-the-results) for a comprehensive experimentation.

---

### Step 1: Installing dependencies

To successfully install Omnet++ and run the simulations, execute the following commands:

```
sudo apt update
sudo apt-get install -y build-essential
sudo apt-get install -y flex bison
sudo apt-get install -y zlib1g-dev
sudo apt-get install -y libxml2-dev
sudo apt install -y sqlite3
sudo apt-get install -y libsqlite3-dev
sudo apt-get install -y zip unzip
```

Make sure you have python3 installed (if not, run ```sudo apt-get install -y python3```). If it is installed and accessible in "/usr/bin/python3" run the following: 

```
sudo rm -rf /usr/bin/python
sudo ln -s /usr/bin/python3 /usr/bin/python
sudo apt install -y python3-numpy
sudo apt-get install -y python3-matplotlib
```

---

### Step 2: Installing Omnet++

The complete instructions for installing Omnet++ can be found in [Omnet++ installation guide](https://doc.omnetpp.org/omnetpp/InstallGuide.pdf). **In case you already have the simulator installed, you can skip this step.** To install Omnet++ on an Ubuntu OS, you should run the following commands:

```
wget https://github.com/omnetpp/omnetpp/releases/download/omnetpp-5.6.2/omnetpp-5.6.2-src-linux.tgz
tar xvfz omnetpp-5.6.2-src-linux.tgz
cd omnetpp-5.6.2/
. setenv
```

For the next line we are assuming that you have extracted omnet in $HOME. If this is not true, replace $HOME with the path you have extracted the omnet files to.

```
echo "export PATH=$HOME/omnetpp-5.6.2/bin:\$PATH" >> ~/.bashrc
```

```
source ~/.bashrc
./configure WITH_QTENV=no WITH_OSG=no WITH_OSGEARTH=no
```

In case the configure command printed out that "omnetpp-5.6.2/bin" is not added to your path, close and re-open your terminal and run the configure command again. If you are not using GUI, reboot your system using ```sudo reboot```. Make sure you get the **"Your PATH contains /opt/omnetpp-5.6.2/bin. Good!"** message in the configuration process before moving forward.

```
make
```

---

### Step 3: Cloning the repository

To clone the repository, run the following script:

```
git clone https://github.com/foundational-networks/PEEL.git
```

---

### Step 4: Building the project

We have provided a ```build.sh``` shell script to simplify this process. To build the project modules and download the distribution files, run the following script:

```
cd PEEL/Omnet_Sims/
bash build.sh
```

---

### Small-scale simulations: Extra step for evaluation in a short time

Every scenario with [large-scale simulation](#step-6-running-the-large-scale-simulations-and-extracting-the-results) configurations takes days or even weeks to complete. Accordingly, we are providing a small-scale sample in which we run a single Broadcast collective with 8 MB messages among 64 nodes and compare the results of various techniques for those interested in evaluating the code in a short time. First, make sure that you are in the right directory ("PEEL/Omnet_Sims/dc_simulations/simulations/sims") and then use the following commands to extract the distribution files, run the simulations, and extract the results:

```
cd dc_simulations/simulations/sims
bash extract_dist_files_sample.sh
./run_sample.sh
cp data_extraction_codes/* extracted_results/
cd extracted_results
./print_results_sample.sh
```

The commands above download the distribution files for the sample simulations and simulate the following collective algorithms:

* Ring
* Binary Tree
* Optimal multicast
* Orca
* Elmo
* PEEL

Because the sample experiment simulates only a single Broadcast operation, each scenario should complete in less than two minutes. After running the commands above, the Collective Completion Time (CCT) results for all evaluated techniques will be stored in: ```PEEL/Omnet_Sims/dc_simulations/simulations/sims/extracted_results/archive```. Also, the figures are plotted and stored in ```PEEL/Omnet_Sims/dc_simulations/simulations/sims/extracted_results/figs```.

The output should look similar to the following:

```

ring_bcast
Mean CCT (s): 0.00140

tree_bcast
Mean CCT (s): 0.00476

mcast_optimal_bcast
Mean CCT (s): 0.00017

mcast_bcast_orca
Mean CCT (s): 0.00080

mcast_bcast_elmo
Mean CCT (s): 0.00072

mcast_bcast_peel
Mean CCT (s): 0.00027

```

The results above show that PEEL achieves performance closest to the optimal multicast baseline while outperforming all other techniques.

---

### Step 5: Running the large-scale simulations and extracting the results

The config files for large-scale simulations can be used for evaluating various collective operations and algorithms. To run the large-scale simulations under distinct collective algorithms for a specific collective operation, first make sure that you are in the right directory (```PEEL/Omnet_Sims/dc_simulations/simulations/sims```) and then use the following commands to download the distribution files, run the simulations, extract the results, and plot the figures:

```
bash $DIST_DOWNLOWDER.sh
./$EXPERIMENT_RUN_FILE.sh
cp data_extraction_codes/* extracted_results/
cd extracted_results
./$EXPERIMENT_PRINT_FILE.sh
```

The table below summarizes the appropriate ```$DIST_DOWNLOWDER```, ```$EXPERIMENT_RUN_FILE```, and ```$EXPERIMENT_PRINT_FILE``` for distinct experiments:

Experiment | $DIST_DOWNLOWDER | $EXPERIMENT_RUN_FILE | $EXPERIMENT_PRINT_FILE
--- | --- | ---  | --- 
Broadcast operations in leaf-spine | extract_dist_files_ls_bcast_dfsize_50 | run_bcast_ls_dfsize_50 | print_results_ls_bcast_dfsize_50
Broadcast operations in 8-ary fat-tree | extract_dist_files_ft_bcast_dfsize_50 | run_bcast_ft_dfsize_50 | print_results_ft_bcast_dfsize_50
All-Gather operations in leaf-spine | extract_dist_files_ls_allgather_allreduce_dfsize | run_allgather_ls_dfsize | print_results_ls_allgather_dfsize
All-Gather operations in 8-ary fat-tree | extract_dist_files_ft_allgather_allreduce_dfsize | run_allgather_ft_dfsize | print_results_ft_allgather_dfsize
All-Reduce operations in leaf-spine | extract_dist_files_ls_allgather_allreduce_dfsize | run_allreduce_ls_dfsize | print_results_ls_allreduce_dfsize
Real-life datacenter trace experiment | extract_dist_files_ls_traffic_trace_50 | run_traffic_trace_ls_50 | print_results_ls_traffic_trace_50


The commands above run each experiment and store the results in ```PEEL/Omnet_Sims/dc_simulations/simulations/sims/extracted_results/archive``` and figures in ```PEEL/Omnet_Sims/dc_simulations/simulations/sims/extracted_results/figs```. Simulating most algorithms is expected to take less than 4 days. However, simulating a collective operation with various algorithms would require weeks. We refer you to the **small-scale simulations** section if you would like to see some results in a short time.
