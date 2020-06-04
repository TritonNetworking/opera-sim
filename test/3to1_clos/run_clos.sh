#!/bin/sh

../../src/clos/datacenter/htsim_ndp_fatTree_3to1_k12 -q 46 -cwnd 30 -strat perm -nodes 648 -simtime .100001 -pullrate 1 -flowfile flows_test.htsim
