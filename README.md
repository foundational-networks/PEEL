# Closing the Bandwidth Gap in AI Datacenters with Scalable Multicast - paper artifacts

This repository contains the artifacts for network simulations and hardware implementations used for our paper. 

Please refer to the README file in the `Omnet_Sims/` directory for instructions on running the simulations and extracting the results. Additionally, the README file in the `FPGA/` directory provides instructions for ... TBD.

## Cloning the repository and its submodules

To work with any component of this repository, first clone the PEEL repository and initialize all of its submodules. On **Ubuntu-based systems**, run:

```
git clone https://github.com/foundational-networks/PEEL.git
cd PEEL
git submodule update --init --remote --recursive
```

You can now refer to the `README` file within each component (e.g., `Omnet_Sims`) for detailed instructions on setting up, running, and testing that component.

## Getting started instructions

**Omnet_Sims:** As also noted in the `Omnet_Sims` directory, to run a small-scale "Hello World" experiment, first follow **Steps 1–4** in the `Omnet_Sims/README` to install the simulator and set up the experimentation environment. Then, follow the instructions under **Small-scale simulations: Extra step for evaluation in a short time** to run the small-scale experiment. The entire process, including setup and simulation, is expected to take **less than 30 minutes**.

**FPGA:** TBD


## Detailed instructions

**Omnet_Sims:** For large-scale simulations, if you haven't already done so, you should first follow **Steps 1–4** in the `Omnet_Sims/README` to install the simulator and set up the experimentation environment. Then, follow the instructions under **Step 6: Running the large-scale simulations and extracting the results** to run the large-scale experiments. Unlike the small experiment, large-scale experiments are expected to take days up to weeks.

**FPGA:** TBD


_Please refer to our NSDI '2027 paper for more information about PEEL. Also, feel free to contact the authors if you have any questions regarding the artifacts._
