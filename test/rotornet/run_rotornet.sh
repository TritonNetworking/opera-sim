#!/bin/sh

# For RotorNet, we set -cutoff 0 and -rlbflow 0

../../src/opera/datacenter/htsim_ndp_dynexpTopology -cutoff 0 -rlbflow 0 -cwnd 30 -q 8 -simtime .100001 -pullrate 1 -topfile ../../topologies/dynexp_N=108_k=12_1path.txt -flowfile flows_test.htsim