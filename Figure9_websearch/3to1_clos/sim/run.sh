#!/bin/sh

../../../src/clos/datacenter/htsim_ndp_fatTree_3to1_k12 -simtime 1.00001 -cwnd 30 -strat perm -nodes 648 -q 46 -pullrate 1 -flowfile ../traffic_gen/flows_3to1_1percLLload_1sec_648hosts.htsim >> FCT_3to1_cwnd30_1perc.txt
../../../src/clos/datacenter/htsim_ndp_fatTree_3to1_k12 -simtime 1.00001 -cwnd 30 -strat perm -nodes 648 -q 46 -pullrate 1 -flowfile ../traffic_gen/flows_3to1_5percLLload_1sec_648hosts.htsim >> FCT_3to1_cwnd30_5perc.txt
../../../src/clos/datacenter/htsim_ndp_fatTree_3to1_k12 -simtime 1.00001 -cwnd 30 -strat perm -nodes 648 -q 46 -pullrate 1 -flowfile ../traffic_gen/flows_3to1_10percLLload_1sec_648hosts.htsim >> FCT_3to1_cwnd30_10perc.txt
../../../src/clos/datacenter/htsim_ndp_fatTree_3to1_k12 -simtime 1.00001 -cwnd 30 -strat perm -nodes 648 -q 46 -pullrate 1 -flowfile ../traffic_gen/flows_3to1_15percLLload_1sec_648hosts.htsim >> FCT_3to1_cwnd30_15perc.txt
../../../src/clos/datacenter/htsim_ndp_fatTree_3to1_k12 -simtime 1.00001 -cwnd 30 -strat perm -nodes 648 -q 46 -pullrate 1 -flowfile ../traffic_gen/flows_3to1_20percLLload_1sec_648hosts.htsim >> FCT_3to1_cwnd30_20perc.txt
../../../src/clos/datacenter/htsim_ndp_fatTree_3to1_k12 -simtime 1.00001 -cwnd 30 -strat perm -nodes 648 -q 46 -pullrate 1 -flowfile ../traffic_gen/flows_3to1_25percLLload_1sec_648hosts.htsim >> FCT_3to1_cwnd30_25perc.txt