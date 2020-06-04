#!/bin/sh

../../../src/clos/datacenter/htsim_ndp_fatTree_3to1_k12 -q 60 -cwnd 40 -strat perm -nodes 648 -simtime .300001 -utiltime .001 -pullrate 1 -flowfile ../traffic_gen/flows_100kB_3to1_648hosts.htsim > output_100kB_3to1_pull1_cwnd40_q60.txt