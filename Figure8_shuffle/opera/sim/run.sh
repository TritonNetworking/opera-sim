#!/bin/sh

../../../src/opera/htsim_ndp_dynexpTopology -cwnd 20 -q 8 -utiltime .001 -simtime .100001 -pullrate 1 -topfile ../../../topologies/dynexp_N=108_k=12_1path.txt -flowfile ../traffic_gen/flows_100kB_648hosts.htsim >> output_100kB_opera_1path_1hopOnly.txt
../../../src/opera/htsim_ndp_dynexpTopology -cwnd 20 -q 8 -utiltime .001 -simtime .100001 -pullrate 1 -topfile ../../../topologies/dynexp_N=108_k=12_5paths.txt -flowfile ../traffic_gen/flows_100kB_648hosts.htsim >> output_100kB_opera_5paths_1hopOnly.txt