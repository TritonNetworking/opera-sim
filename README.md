# opera-sim
Packet-level simulation code to model Opera and other networks from the 2020 NSDI paper "Expanding across time to deliver bandwidth efficiency and low latency"

## Requirements:

- Matlab files: Matlab 2018b or newer (may work with older Matlab versions or with GNU Octave, but not tested)
- C++ files: g++-7 compiler

## Description:

- The /FigureX directories contain example Matlab scripts to generate traffic and post-process simulation data as well as execution scripts for running simulations.
- The /src directory contains the packet simulator source code. There is a separate simulator for each network type (e.g. Clos, expander, and Opera). The packet simulator is an extension of the htsim NDP simulator (https://github.com/nets-cs-pub-ro/NDP/tree/master/sim)

## Build instructions:

- To build the Opera simulator, for example, from the top level directory run:
  ```
  cd /src/opera
  make
  cd /datacenter
  make
  ```
- The executable will be found in the /datacenter directory and named htsim_...

## Typical workflow:

- Compile the simulator as described above (for Clos, expander, and Opera).
- Build a topology file (e.g. run /topologies/opera_dynexp_topo_gen/MAIN.m). This must be done once for expander and Opera networks, is not needed for Clos networks.  This process takes a long time.  If you'd prefer to download the version of this file used to create the NSDI 2020 paper, you can download it from this link (it is too big to host on github): https://www.dropbox.com/s/az6ju4oiwvsljat/dynexp_N%3D108_k%3D12_5paths.txt?dl=0
- Generate a file specifying the traffic (e.g. run /Figure7_datamining/opera/traffic_gen/generate_traffic.m). The file format is (where src_host and dst_host are indexed from zero):
  ```
  <src_host> <dst_host> <flow_size_bytes> <flow_start_time_nanosec> /newline
  ```
- Specify the simulation parameters and run (e.g. run /Figure7_datamining/opera/sim/run.sh).
- Post-process the simulation data (e.g. run /Figure7_datamining/opera/plot/process_FCT_and_UTIL.m).
- Plot the post-processed data (e.g. run /Figure7_datamining/opera/plot/plotter.m)
