#!/bin/sh


# priority traffic goes over the 6:1 oversubscribed Clos:

../../../src/clos/datacenter/htsim_ndp_fatTree_6to1_hybrid -simtime 10.00001 -cwnd 30 -strat perm -nodes 648 -q 46 -pullrate 1 -flowfile ../traffic_gen_prio/flows_1percLoad_10sec_648hosts_6to1_prio.htsim >> FCT_6to1_cwnd30_1perc_prio_1.txt
../../../src/clos/datacenter/htsim_ndp_fatTree_6to1_hybrid -simtime 10.00001 -cwnd 30 -strat perm -nodes 648 -q 46 -pullrate 1 -flowfile ../traffic_gen_prio/flows_10percLoad_10sec_648hosts_6to1_prio.htsim >> FCT_6to1_cwnd30_10perc_prio_1.txt
../../../src/clos/datacenter/htsim_ndp_fatTree_6to1_hybrid -simtime 10.00001 -cwnd 30 -strat perm -nodes 648 -q 46 -pullrate 1 -flowfile ../traffic_gen_prio/flows_25percLoad_10sec_648hosts_6to1_prio.htsim >> FCT_6to1_cwnd30_25perc_prio_1.txt
../../../src/clos/datacenter/htsim_ndp_fatTree_6to1_hybrid -simtime 10.00001 -cwnd 30 -strat perm -nodes 648 -q 46 -pullrate 1 -flowfile ../traffic_gen_prio/flows_30percLoad_10sec_648hosts_6to1_prio.htsim >> FCT_6to1_cwnd30_30perc_prio_1.txt
../../../src/clos/datacenter/htsim_ndp_fatTree_6to1_hybrid -simtime 10.00001 -cwnd 30 -strat perm -nodes 648 -q 46 -pullrate 1 -flowfile ../traffic_gen_prio/flows_40percLoad_10sec_648hosts_6to1_prio.htsim >> FCT_6to1_cwnd30_40perc_prio_1.txt

# bulk traffic goes over Rotornet (enforce by setting -cutoff 0):

../../../src/opera/datacenter/htsim_ndp_dynexpTopology -simtime 10.00001 -cutoff 0 -rlbflow 0 -cwnd 20 -q 8 -pullrate 1 -topfile ../../../topologies/dynexp_N=108_k=12_1path.txt -flowfile ../traffic_gen/flows_1percLoad_10sec_648hosts.htsim >> FCT_pfab_cwnd20_1perc_RLBonly.txt
../../../src/opera/datacenter/htsim_ndp_dynexpTopology -simtime 10.00001 -cutoff 0 -rlbflow 0 -cwnd 20 -q 8 -pullrate 1 -topfile ../../../topologies/dynexp_N=108_k=12_1path.txt -flowfile ../traffic_gen/flows_10percLoad_10sec_648hosts.htsim >> FCT_pfab_cwnd20_10perc_RLBonly.txt
../../../src/opera/datacenter/htsim_ndp_dynexpTopology -simtime 10.00001 -cutoff 0 -rlbflow 0 -cwnd 20 -q 8 -pullrate 1 -topfile ../../../topologies/dynexp_N=108_k=12_1path.txt -flowfile ../traffic_gen/flows_25percLoad_10sec_648hosts.htsim >> FCT_pfab_cwnd20_25perc_RLBonly.txt
../../../src/opera/datacenter/htsim_ndp_dynexpTopology -simtime 10.00001 -cutoff 0 -rlbflow 0 -cwnd 20 -q 8 -pullrate 1 -topfile ../../../topologies/dynexp_N=108_k=12_1path.txt -flowfile ../traffic_gen/flows_30percLoad_10sec_648hosts.htsim >> FCT_pfab_cwnd20_30perc_RLBonly.txt
../../../src/opera/datacenter/htsim_ndp_dynexpTopology -simtime 10.00001 -cutoff 0 -rlbflow 0 -cwnd 20 -q 8 -pullrate 1 -topfile ../../../topologies/dynexp_N=108_k=12_1path.txt -flowfile ../traffic_gen/flows_40percLoad_10sec_648hosts.htsim >> FCT_pfab_cwnd20_40perc_RLBonly.txt