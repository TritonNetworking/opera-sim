#!/bin/sh

# for expanders, we can set the -VLB flag to 0 or 1

../../src/expander/datacenter/htsim_ndp_expander -cwnd 30 -q 46 -simtime .100001 -pullrate 1 -VLB 0 -topfile ../../topologies/expander_N=130_u=7_ecmp.txt -flowfile flows_test.htsim