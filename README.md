# Closing the Bandwidth Gap in AI Datacenters with Scalable Multicast — Paper Artifacts

This repository contains the artifacts associated with our paper, including network simulations, physical testbed experiments, and FPGA implementations.
The repository is organized into multiple components, each with its own `README` containing setup, execution, and validation instructions. In particular:

* `Omnet_Sims/`: Instructions for running the network simulations and extracting results.
* `Benchmark/`: Instructions for configuring the physical testbed, validating PEEL forwarding, and running Gloo-based collective communication benchmarks.
* `FPGA/`: Instructions for opening, simulating, synthesizing, implementing, and evaluating the PEEL FPGA module.

---

## Cloning the Repository and Its Submodules

To work with any component of this repository, first clone the PEEL repository and initialize all of its submodules.

On **Ubuntu-based systems**, run:

```bash
git clone https://github.com/foundational-networks/PEEL.git
cd PEEL
git submodule update --init --remote --recursive
```

After cloning the repository, refer to the `README` file within each component for detailed instructions on configuring, running, and validating that artifact.

---

## Getting Started Instructions

The following instructions are intended for those who want to quickly validate the main artifact components.

### Omnet_Sims

To run a small-scale "Hello World" simulation:

1. Follow **Steps 1–4** in [`Omnet_Sims/README.md`](Omnet_Sims/README.md) to install the simulator and set up the experimental environment.
2. Follow the instructions under **Small-scale simulations: Extra step for evaluation in a short time** to run the provided small-scale experiment.

### Benchmark

To quickly validate PEEL on our physical testbed, refer to the **Example Testbed Experiment** section in [`Benchmark/run_experiment.md`](Benchmark/run_experiment.md).
This section provides the commands required to run a small validation experiment using the preconfigured physical testbed.

> **Note:** The physical-testbed _Getting Started workflow_ requires access to our VPN and preconfigured VMs and needs special credentials provided by the authors. If such credentials are not provided to you, please kindly follow the steps in `Benchmark/` to build the project on your testbed from scratch.

---

## Detailed Instructions

The following sections provide guidance for reproducing the full experiments.

### Omnet_Sims

For large-scale simulations:

1. If you have not already done so, follow **Steps 1–4** in [`Omnet_Sims/README.md`](Omnet_Sims/README.md) to install the simulator and configure the experimental environment.
2. Follow **Step 6: Running the large-scale simulations and extracting the results** to execute the full experiments and extract their results.

### Benchmark

For complete physical-testbed setup and evaluation, refer to the documentation under [`Benchmark/`](Benchmark/) and [`Env_Setup/`](Env_Setup/) that include instructions for:

* Installing and compiling the PEEL-enabled Gloo benchmark.
* Configuring the Ryu OpenFlow controller.
* Configuring the PVE Open vSwitch instances and physical SDN switch.
* Setting up the experimental VMs.
* Validating PEEL forwarding behavior.
* Running collective communication benchmarks and collecting performance results.

### FPGA

Refer to [`FPGA/README.md`](FPGA/README.md) for instructions on:

* Opening the PEEL Vivado project.
* Running the provided testbenches.
* Running synthesis and implementation.
* Inspecting FPGA resource utilization.

> **Note:** The PEEL FPGA module is not tied to a specific FPGA board model, as long as the target device provides sufficient hardware resources.

---

Please refer to our **NSDI 2027 paper** for additional details about PEEL. If you have questions, please contact the authors.
