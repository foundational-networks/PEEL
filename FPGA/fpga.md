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