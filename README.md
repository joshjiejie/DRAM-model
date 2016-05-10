# DRAM-model

The tool is based on a realistic DRAM access model. It can be used to estimate performance for edge-centirc graph processing using FPGA and DRAM.

The input file contains all the edges of the graph.
Each edge is presented as "src vertex id,  dst vertex id".

The static.c simulator assumes static memory scheduling policy.
The dynamic.c simulator assumes dynamic memory scheduling policy (First Ready-First Come First Serve).

The outputs include the total latency and # of FPGA pipeline stalls.
