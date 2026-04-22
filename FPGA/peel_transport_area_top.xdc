# Minimal timing-only constraint for the implementation-oriented area top.
# No package pin locations are assigned here; that is intentional.
# For utilization / placed area estimation, a clock definition is usually enough.
create_clock -name clk -period 10.000 [get_ports clk]
