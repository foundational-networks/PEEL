# PEEL Gloo Benchmark

This directory contains all the information required to compile the PEEL-enabled Gloo benchmark, validate PEEL's forwarding behavior, and run experiments on a physical testbed.
The following documentation is provided:

* `gloo_install.md`: Instructions for installing the required dependencies and compiling the PEEL-enabled Gloo benchmark from scratch.
* `peel_forwarding_validation.md`: Instructions for verifying that PEEL's OpenFlow forwarding rules and switch mappings are configured correctly before running experiments.
* `run_experiment.md`: Instructions for running supported collective communication benchmarks and collecting performance results after the environment has been configured.

> **Note:** If you only want to quickly validate the benchmark and have been provided credentials for the authors' preconfigured VMs, you can skip the setup process and go directly to the **Example Testbed Experiment** section in `run_experiment.md`.
