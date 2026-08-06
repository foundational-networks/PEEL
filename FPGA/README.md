# PEEL FPGA Module

The PEEL FPGA module is not tied to a specific FPGA board model, as long as the target device provides sufficient hardware resources.
For our implementation and evaluation, we use the following FPGA device: `Kintex UltraScale+ XCKU5P-2FFVB676E`.
The design was developed and tested using `AMD Vivado 2025.2`.
This document explains how to open the provided Vivado project, run the included testbenches, synthesize and implement the design, and inspect FPGA resource utilization.

---

## Step 1: Prepare the Source Files

The complete Vivado project is provided as:

```text
PEEL/FPGA/peel_module.zip
```

Download or copy `peel_module.zip` to the machine running Vivado and extract the archive.

For example, in our Windows environment, the extracted project is located at:

```text
Desktop/exp/peel/project_1
```

The exact location is not important as long as Vivado can access the project files.

---

## Step 2: Open the Vivado Project

Launch **AMD Vivado 2025.2** and select **Open Project**.

[pic1]

Navigate to the extracted project directory and select:

```text
project_1.xpr
```

Then click **Open**.

[pic2]

Once the project is loaded, the **Sources** panel displays the project's design sources and simulation sources.

[pic3]

---

## Step 3: Select a Testbench

Several testbench modules are included with the project for validating different parts of the PEEL FPGA design.

By default, the selected simulation top module is:

```text
tb_peel_transport_end_to_end
```

To select or verify the testbench:

1. Locate the **SIMULATION** section in the Flow Navigator.
2. Right-click **Simulation** and select **Simulation Settings**.

[pic4]

Under the simulation settings, verify that **Simulation top module name** corresponds to the testbench you want to run.

[pic5]

Different testbench modules can be selected to validate different stages or components of the PEEL hardware pipeline.

---

## Step 4: Run Behavioral Simulation

From the **SIMULATION** section, select:

**Run Simulation → Run Behavioral Simulation**

[pic6]

Wait for Vivado to compile the simulation sources and launch the simulator.

After the simulation starts, the waveform viewer can be used to inspect the behavior of the selected testbench.

[pic7]

Depending on the testbench, different internal signals can be examined to verify the corresponding PEEL functionality.

For example, signals such as:

```text
tx_rx_valid
net_mcast_valid_d
```

can be monitored to identify when multicast packets are successfully received and parsed by the PEEL hardware pipeline.

---

## Step 5: Run Synthesis

To synthesize the FPGA design, navigate to the **SYNTHESIS** section in the Vivado Flow Navigator.

[pic8]

The provided project already contains synthesis results for the current design. You can either:

* Open the existing synthesized design, or
* Run synthesis again using **Run Synthesis**.

Running synthesis again is useful when modifying the RTL or targeting a different FPGA device.

---

## Step 6: Run Implementation

After synthesis completes, proceed to the **IMPLEMENTATION** section.

[pic9]

You can either:

* Open the provided implemented design, or
* Select **Run Implementation** to regenerate the implementation results.

Implementation performs the placement and routing of the synthesized design for the selected FPGA device.

---

## Step 7: Inspect Resource Utilization

To inspect the FPGA resource usage, open the **Tcl Console** in Vivado and run:

```tcl
report_utilization
```

[pic10]

Vivado will generate a detailed resource-utilization report showing the FPGA resources consumed by the PEEL design, including resources such as:

* LUTs
* Flip-flops
* BRAMs
* DSPs
* Other device-specific resources

The report can be used to evaluate the hardware footprint of the PEEL module and determine whether the design fits within the available resources of a target FPGA device.

---

## Using a Different FPGA Device

The PEEL module itself does not depend on the specific `XCKU5P-2FFVB676E` device used in our testbed.

To target a different FPGA, select the appropriate device or board in Vivado and rerun synthesis and implementation.

The target FPGA must provide sufficient resources for the PEEL design, and device-specific timing, interface, and resource constraints should be verified before deployment.


















As long as resource suffice, our FPGA module does not bind to any specifc board model.
We use the model for example here.
Kintex UltraScale+ XCKU5P-2FFVB676E

# The IDE we used is AMD Vivado 2025.2

# 0. prepare the source
download peel.zip from PEEL/FPGA/peel_module.zip
unzip the archive.

In our case, on a Windows hosts, we put it in the Desktop/exp/peel/project_1 dir

# 1. Open the source as project.
start Vivado 2025.2 and click on open project.
[pic1]
navigate to the project dir, and select project_1.xpr file, then open it
[pic2]

# Once opened, you will find design sources and simulation sources from the "Sources" panel.
[pic3]

# By default, we have a few test bench files available to test the code,
the default selected is "tb_peel_transport_end_to_end"

# Right click the "SIMULATION" tab and select "Simulation Settings"
[pic4]

# make sure the "Simulation top module name" is the one you want.
[pic5]

# Left click "Run Simulation" and "Run Behavioral Simulation" and wait for it to complete
[pic6]

# With different testbench, we will be able to examine the the result of each step
[pic7]

For example we can see the debug signal "tx_rx_valid" or "net_mcast_valid_d" each time a multicast packet was successfully received and parsed.


# In order to get a chip area graph and resource utilization, continue to the "SYNTHESIS" tab below
[pic8]
we already have the current design synthesized, you may choose to run it again or simply open the current design.

# Once finished, proceed to the "IMPLEMENTATION" section to run implementation or open our implemented Design
[pic9]

# In order to check resource utilization
Navigate to the "Tcl Console" and enter "report_utilization"
[pic10]

a detailed report will be printed to the console
