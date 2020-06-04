#!/bin/sh

# for Opera, we set -cutoff 15 MB (specific to N=108 rack topology)
# we can also force flows of a certain size to use RLB using the -rlbflow flag (used in the shuffle experiment for 100 kB flows)

../../src/opera/datacenter/htsim_ndp_dynexpTopology -cutoff 15000000 -rlbflow 0 -cwnd 30 -q 8 -simtime .100001 -pullrate 1 -topfile ../../topologies/dynexp_N=108_k=12_1path.txt -flowfile flows_test.htsim

#../../src/opera/datacenter/htsim_ndp_dynexpTopology -cutoff 15000000 -rlbflow 100000 -cwnd 30 -q 8 -simtime .100001 -pullrate 1 -topfile ../../topologies/dynexp_N=108_k=12_5paths.txt -flowfile flows_test.htsim