# Omnet++ simulation files

This repository provides the instructions and files required to run the simulations and extract the results. We use [Omnet++ simulator](https://omnetpp.org/) and INET framework to run the simulations. [Omnet++ manual](https://doc.omnetpp.org/omnetpp/manual/) and its examples are good references for an introduction to the simulator. We ran our simulations on Ubuntu machines (version: 18.04), so all the commands are for Ubuntu. To run the simulations, you should follow the following steps:

> **Note:** This `rdmc` branch is configured to evaluate RDMC for Broadcast collectives. The sample and large-scale workflows in this branch run RDMC only.

* [Step 1: Install dependencies](#step-1-installing-dependencies) (~ 5 minutes)
* [Step 2: Install Omnet++](#step-2-installing-omnet)  (~ 5 minutes)
* [Step 3: Build the project](#step-3-building-the-project) (~ 5 minutes)
* [Step 4: Run the simulations and extract the results](#small-scale-simulations-extra-step-for-evaluation-in-a-short-time) (This step can take from a few minutes to a few weeks depending on the simulations that you run)

> **Note:** For both the small-scale “Hello World” example and the complete large-scale simulations, follow Steps 1–4 to install the simulator and set up the experimental environment. After completing these steps, you can either run the [small-scale simulation commands](#small-scale-simulations-extra-step-for-evaluation-in-a-short-time) for quick testing or [execute the large-scale simulation files](#step-4-running-the-large-scale-simulations-and-extracting-the-results) for a comprehensive experimentation.

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

Make sure you have python3 installed (if not, run ```sudo apt-get install -y python3```). If it is installed and accessible in "/usr/bin/python3" run the following: 

```bash
sudo rm -rf /usr/bin/python
sudo ln -s /usr/bin/python3 /usr/bin/python
sudo apt install -y python3-numpy
sudo apt-get install -y python3-matplotlib
```

---

### Step 2: Installing Omnet++

The complete instructions for installing Omnet++ can be found in [Omnet++ installation guide](https://doc.omnetpp.org/omnetpp/InstallGuide.pdf). **In case you already have the simulator installed, you can skip this step.** To install Omnet++ on an Ubuntu OS, you should run the following commands:

```bash
wget https://github.com/omnetpp/omnetpp/releases/download/omnetpp-5.6.2/omnetpp-5.6.2-src-linux.tgz
tar xvfz omnetpp-5.6.2-src-linux.tgz
cd omnetpp-5.6.2/
. setenv
```

For the next line we are assuming that you have extracted omnet in $HOME. If this is not true, replace $HOME with the path you have extracted the omnet files to.

```bash
echo "export PATH=$HOME/omnetpp-5.6.2/bin:\$PATH" >> ~/.bashrc
```

```bash
source ~/.bashrc
./configure WITH_QTENV=no WITH_OSG=no WITH_OSGEARTH=no
```

In case the configure command printed out that "omnetpp-5.6.2/bin" is not added to your path, close and re-open your terminal and run the configure command again. If you are not using GUI, reboot your system using ```sudo reboot```. Make sure you get the **"Your PATH contains /opt/omnetpp-5.6.2/bin. Good!"** message in the configuration process before moving forward.

```bash
make
```

---

### Step 3: Building the project

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

### Step 4: Running the large-scale simulations and extracting the results

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

The provided run scripts use the `rdmc` configuration by default. The configuration files also include `rdmc_hier` for hierarchical RDMC. To run hierarchical RDMC, refer to comment in the run scripts.