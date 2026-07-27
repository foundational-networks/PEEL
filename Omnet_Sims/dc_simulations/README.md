# Instructions on how to build and run PEEL

This repository provides the instructions and files required to run the simulations and extract the results. We use [Omnet++ simulator](https://omnetpp.org/) and INET framework to run the simulations. [Omnet++ manual](https://doc.omnetpp.org/omnetpp/manual/) and its examples are good references for introduction to the simulator. We ran our simulations on Ubuntu machines (version: 18.04) so all the commands are for Ubuntu. To run the simulations, you should follow the following steps:

## Step 1) Installing dependencies

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

## Step 2) Installing Omnet++

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

In case, the configure command printed out that "omnetpp-5.6.2/bin" is not added to your path, close and re-open your terminal and run the configure command again. If you are not using GUI, reboot your system using ```sudo reboot```. Make sure you get the **"Your PATH contains /opt/omnetpp-5.6.2/bin. Good!"** message in the configuration process before moving forward.

```
make
```

## Step 3) Cloning and building the simulation libraries

You need to clone the INET and PEEL code separately. In a project directory of your choice (**projdir/** such as ```omnetpp-5.6.2/samples/```), run the scripts below:


```
git clone https://sepehrabdous96@bitbucket.org/sepehrabdous96/inet_backup.git
git clone https://github.com/sepehrabdous/dc_simulations.git
```

```
mv inet_backup/ inet
cd inet
git checkout collective_operations
make clean
make makefiles
make MODE=release -j20 all 
```

```
cd ../dc_simulations/
git checkout collective_operations
nano src/Makefile
```

In the Makefile that opens there is a line like ```INET_PROJ=/home/sepehr/Desktop/inet```. Replace it with ```INET_PROJ=../../inet``` and run:

```
make clean
make MODE=release -j20 all 
```

**Note: In the next sections, we describe how to download traffic files, configure simulations, run them, and extract the results. If you want quick tests, at the end of the document, we provide a section that walks you through running All-Reduce simulations on Cloudlab.**

## Step 4) Downloading traffic distribution files

Before starting the simulations, you must download the traffic distribution files. In ```dc_simulations/simulations/Two_Layer_Leaf_Spine/for_cloud_lab/dist_downloaders``` we have included bash files that download different traffic distribution files and store them in ```/mnt/data/OneDrive/mburst_simulations/forwarding_dbs/distributions```. You have to change this if you like the files to be downloaded somewhere else. **Make sure to have enough storage capacity wherever you download your traffic files.** You can download the traffic files by running:

```
bash X.sh
```

X is the name of the distribution file. Below, is a list of key distribution files you can use:

**Fat-tree Broadcast files with 30%, 50%, and 70% loads and different flow sizes**
* ft_collectives_broadcast_1pergpu_dfsize_constant_load_30.sh
* ft_collectives_broadcast_1pergpu_dfsize_constant_load_50.sh
* ft_collectives_broadcast_1pergpu_dfsize_constant_load_70.sh

**Leaf-spine Broadcast files with 30%, 50%, and 70% loads and different flow sizes**
* ls_collectives_broadcast_1pergpu_dfsize_constant_load_30.sh
* ls_collectives_broadcast_1pergpu_dfsize_constant_load_50.sh
* ls_collectives_broadcast_1pergpu_dfsize_constant_load_70.sh

**Leaf-spine All-Gather/All-Reduce files with 50\% load and different flow sizes**
* ls_collectives_allgather_1pergpu_dfsize_constant_load_50.sh

**Workload created from Meta's traces**
* ls_collectives_combination_1pergpu_8MB_64_50.sh

## Step 5) Running the simulations

In ```dc_simulations/simulations/Two_Layer_Leaf_Spine/``` we have included configuration files such as ```.ned``` topology files in addition to the ```.ini``` files, which are the main files that you have to deal with when running simulations. All ```.ini``` files that are used in our simulations are stored in ```dc_simulations/simulations/Two_Layer_Leaf_Spine/for_cloud_lab/ini_files/```. The key files are listed below:

**Fat-tree and leaf-spine Broadcast files**
* omnetpp_bcast_baseFT.ini
* omnetpp_bcast_baseLS.ini

**Leaf-spine All-Gather files**
* omnetpp_allgather_base.ini

**Leaf-spine All-Reduce files**
* omnetpp_allreduce_base.ini

**Leaf-spine trace-driven file**
* omnetpp_combination_base.ini

Each of these files includes the following baselines (Format --> _The common name of the baseline: The name of the config_):

**Broadcast files**
* Ring: UDP_ECMP_ring
* Double binary tree: UDP_ECMP_binary_tree_capped_bw
* Orca: UDP_ECMP_ace_capped_bw_orca
* PEEL: UDP_ECMP_cidr_chunk_data_spray
* Optimal: UDP_ECMP_ace_capped_bw

**All-Gather files**
* Ring: UDP_ECMP_ring
* Double binary tree: UDP_ECMP_binary_tree_capped_bw
* Orca: UDP_ECMP_ace_capped_bw_orca
* PEEL: UDP_ECMP_cidr_chunk_data_spray
* Optimal: UDP_ECMP_ace_capped_bw

**All-Reduce files**
* Ring: UDP_ECMP_ring
* Double binary tree: UDP_ECMP_binary_tree_capped_bw
* Orca: UDP_ECMP_ace_capped_bw_orca
* PEEL: UDP_ECMP_cidr_chunk_data_spray
* Optimal: UDP_ECMP_ace
* Opti-Reduce: UDP_ECMP_optireduce
* Opti-Reduce + PEEL: UDP_ECMP_optireduce_cidr

When running a simulation, you should make sure to set the following configurations appropriately:

### Traffic distribution configs

For proper traffic generation, three variables should be set correctly:
* ```num_flows_per_ml_query``` which sets the scale of the collective -- Default value: 64
* ```flow_size``` which sets the size of each message -- Default value: 8 MB
* ```inter_arrival_time_multiplier``` which indicates the collective inter-arrival times -- No default value

Below is the list of proper configurations for every set of simulations above:

**Broadcast (Fat-tree)**

_30% load_

* mlflowSize = 2000000, 8000000, 32000000, 128000000, 512000000, 2048000000
* MLInterArrivalMultiplier = 0.24414, 0.9765625, 3.90625, 15.625, 62.5, 250.0 ! mlflowSize

_50% load_

* mlflowSize = 2000000, 8000000, 32000000, 128000000, 512000000, 2048000000
* MLInterArrivalMultiplier = 0.14654125, 0.58685375, 2.35849, 9.61538375, 41.66666625, 166.666665 ! mlflowSize

_70% load_

* mlflowSize = 2000000, 8000000, 32000000, 128000000, 512000000, 2048000000
* MLInterArrivalMultiplier = 0.10469, 0.4194625, 1.68918875, 6.94444375, 31.25, 125.0 ! mlflowSize

**Broadcast (Leaf-spine)**

_30% load_

* mlflowSize = 2000000, 8000000, 32000000, 128000000, 512000000, 2048000000
* MLInterArrivalMultiplier = 0.06666, 0.26664, 1.06656, 4.26624, 17.06496, 68.25984 ! mlflowSize

_50% load_

* mlflowSize = 2000000, 8000000, 32000000, 128000000, 512000000, 2048000000
* MLInterArrivalMultiplier = 0.039996, 0.159984, 0.639936, 2.559744, 10.238976, 40.955904 ! mlflowSize

_70% load_

* mlflowSize = 2000000, 8000000, 32000000, 128000000, 512000000, 2048000000
* MLInterArrivalMultiplier = 0.02857, 0.11427, 0.45709, 1.82838, 7.31355, 29.25421 ! mlflowSize

**All-Gather/All-Reduce (Leaf-spine)**

* mlflowSize = 2000000, 8000000, 32000000, 128000000, 512000000
* MLInterArrivalMultiplier = 2.51889, 10.10101, 40.0, 166.66666, 500.0 ! mlflowSize

**Trace-driven (Leaf-spine)**

* mlflowSize = 8000000
* MLInterArrivalMultiplier = 10.0

### PEEL config

PEEL has strict/loose aggregation. In PEEL config files, there is a line: ```**.cidr_type_str = "bw_gain"```. If this line is included, then loos aggregation is activated; otherwise, strict aggregation is enabled.

### Running simulations

After finalizing your config files, you can run the simulations using the following command:


```
sudo /opt/omnetpp-5.6.2/bin/opp_runall -j3 /opt/omnetpp-5.6.2/samples/dc_simulations/src/dc_simulations -m -u Cmdenv -c X -n ..:../../src:../../../inet/src:../../../inet/examples:../../../inet/tutorials:../../../inet/showcases --image-path=../../../inet/images -l ../../../inet/src/INET Y.ini
```

**There is an X and Y in the command above. X is the name of the config (e.g., UDP_ECMP_ring) and Y is the name of the config file (e.g., allreduce_test.ini).**

## Step 6) Extracting the results

After the simulations are done, we need to extract the results. For this purpose, we must first create the directories and sub-directories in which the results are stored. We have created a file (for_cloud_lab/ini_files/dir_creator.sh) that creates the required directory tree in _ResultDir_. By default, _ResultDir_ is set to _/mnt/data/_. You should change it if you want the results to be stored somewhere else. You can create the directory tree by running the following command:

```
bash for_cloud_lab/ini_files/dir_creator.sh
cp data_extraction_codes/*.py ResultDir/leaf\ spine/Constant\ Network\ Load/
```

The omnet output files are stored in _OutputDir/results/_. By default, similar to _ResultDir_, _OutputDir_ is also set to _/mnt/data/_. ```extractor_shell_creator.py``` is in charge of extracting meaningful information (such as flow start and end times) from Omnet's output files. For this purpose, you should edit the file and replace ```OUTPUT_FILE_DIRECTORY_BASE = '/home/sepehr/Desktop/leaf\\ spine'``` with ```ResultDir/leaf\\ spine``` and then run the following commands:

```
cp extractor_shell_creator.py ResultDir/
cd ResultDir/
python3 extractor_shell_creator.py NAME
cd results
parallel -j 20 < extractor.sh
```

After running the commands above, the extracted information will be stored in _ResultDir_. The **NAME** above plays a critical role in our result extraction mechanism so it should be properly set. Below, we present how the name should be set for every technique in every scenario:

**Broadcast:**

Baseline | NAME
--- | --- 
Ring | ring_bcast 
Binary tree | tree_bcast
Orca | ace_bcast_orca
Optimal | ace_bcast
PEEL | ace_bcast_peel

**All-Gather:**

Baseline | NAME
--- | --- 
Ring | ring
Binary tree | tree
Orca | ace_orca
Optimal | ace
PEEL | ace_peel

**All-Reduce:**

Baseline | NAME
--- | --- 
Ring | ring_allreduce
Binary tree | tree_allreduce
Orca | ace_srcreduce_orca
Optimal | ace_srcreduce
PEEL | ace_srcreduce_peel
OptiReduce | optireduce
OptiReduce+PEEL | ace_optireduce_peel

**Trace-driven:**

Baseline | NAME
--- | --- 
Baseline (ring and tree) | baseline
Optimal | ace
PEEL | ace_peel

## Step 7) Computing performance metrics

Now that the meaningful information, such as flow start and end times, is extracted, we must use it to compute performance metrics such as collective completion times (CCTs). For this purpose, we should use the Python files previously copied to ```ResultDir/leaf\ spine/Constant\ Network\ Load/```. First, run the following command to compress the results:

```
python3 Compressor.py 
```

The ```response_time_constant_load.py``` file is in charge of extracting CCT information and uses ```Variables.py``` file to read the correct files. Therefore, you must make sure that the following variables in ```Variables.py``` are properly set:

* ML_QUERY_SCALE_LIST: representing the collective scale in the simulations (e.g., 64)
* ML_QUERY_INTER_MULT_LIST: representing collective inter-arrival times used in the simulations (e.g., 0.06666, 0.26664, 1.06656, 4.26624, 17.06496, 68.25984)
* ML_FLOW_SIZE_LIST: representing the message sizes used in the simulations (e.g., 2000000, 8000000, 32000000, 128000000, 512000000, 2048000000)
* CATEGORIES: represents the list of the technique NAMES (tables above) for which you want to extract CCTs (e.g., ring_bcast or tree_bcast)
* TOPOLOGY: representing whether the results are for LEAF_SPINE or FAT_TREE

After properly setting the variables above, you can extract CCT information using the command below:

```
python3 response_time_constant_load.py
```

## Example) Running example All-Reduce tests

This part provides example commands for running simulations and extracting results of All-Reduce simulations in the two-tier leaf-spine topology we used in our paper. These commands are for CloudLab, so we are assuming that two 1 TB partitions are available in /mnt/data and /mnt/data2. If you want to change things based on your system, please read the instructions in the previous sections. The commands are run in ```dc_simulations/simulations/Two_Layer_Leaf_Spine``` directory:

```
bash for_cloud_lab/dist_downloaders/ls_collectives_allgather_1pergpu_dfsize_constant_load_50.sh
cp for_cloud_lab/ini_files/allreduce_test.ini .
bash for_cloud_lab/ini_files/dir_creator.sh
cp data_extraction_codes/*.py /mnt/data/leaf\ spine/Constant\ Network\ Load/
cp extractor_shell_creator.py mnt/data/  # MAKE SURE extractor_shell_creator.py IS EDITTED AS EXPLAINED ABOVE

/opt/omnetpp-5.6.2/bin/opp_runall -j3 /opt/omnetpp-5.6.2/samples/dc_simulations/src/dc_simulations -m -u Cmdenv -c CONFIG_NAME -n ..:../../src:../../../inet/src:../../../inet/examples:../../../inet/tutorials:../../../inet/showcases --image-path=../../../inet/images -l ../../../inet/src/INET allreduce_test.ini
cd /mnt/data/
python3 extractor_shell_creator.py OUTPUT_NAME
cd results
parallel -j 20 < extractor.sh
cd /mnt/data/leaf\ spine/Constant\ Network\ Load/
python3 Compressor.py
# EDIT VARIABLES ACCORDING TO THE SETUP
python3 response_time_constant_load.py
```

The table below provides how you can replace CONFIG_NAME and OUTPUT_NAME in the commands above for every baseline.

| Baseline | CONFIG_NAME | OUTPUT_NAME
--- | --- | --- 
Ring | UDP_ECMP_ring | ring_allreduce
Binary tree | UDP_ECMP_binary_tree_capped_bw | tree_allreduce
Orca | UDP_ECMP_ace_capped_bw_orca | ace_srcreduce_orca
Optimal | UDP_ECMP_ace | ace_srcreduce
PEEL | UDP_ECMP_cidr_chunk_data_spray | ace_srcreduce_peel
OptiReduce | UDP_ECMP_optireduce | optireduce




