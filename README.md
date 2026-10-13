# HPN codebase

This branch is intended for simulating PEEL and its baselines in a hierarchical rail-optimized datacenter topology (HPN).

## Getting Started

First, clone the repository (skip this step if you have already cloned it):

**Note:** Some workload submodules use Git LFS. Make sure Git LFS is installed before initializing the submodules:


```bash
git clone https://github.com/foundational-networks/PEEL.git
cd PEEL
git checkout hpn
git submodule update --init --remote --recursive
```

After initializing the submodules, refer to the [Omnet_Sims/README](Omnet_Sims/README.md) for instructions on setting up and running the simulations.