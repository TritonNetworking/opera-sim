#!/bin/sh

../../../src/expander/datacenter/htsim_ndp_expander -utiltime .001 -simtime .300001 -cwnd 40 -q 46 -pullrate 1 -VLB 0 -topfile ../../../topologies/expander_N=130_u=7_ecmp.txt -flowfile ../traffic_gen/flows_100kB_650hosts_u7.htsim >> output_100kB_pull1_cwnd40_q46.txt