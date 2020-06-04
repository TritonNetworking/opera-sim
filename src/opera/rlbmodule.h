// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        
#ifndef RLBMODULE_H
#define RLBMODULE_H

//#include <list>
#include "config.h"
#include "eventlist.h"
#include "network.h"
//#include "loggertypes.h"

#include <vector>
#include <deque>

class RlbModule : public EventSource {
public:

    RlbModule(DynExpTopology* top, EventList &eventlist, int node);

    void doNextEvent();

    void receivePacket(Packet& pkt, int flag); // pass a packet into the RLB module. Flag = 0 -> append queue. Flag = 1 -> prepend queue.
    Packet* NICpull(); // pull a packet out of the RLB module to be sent by NIC
    void check_all_empty(); // check if there aren't any more packets committed to be sent by RLB module
    void NICpush(); // notification from the NIC that a high priority packet went out (blocking one of RLB's packets)
    void commit_push(int num_to_push); // push a committed packet back into the RLB queues in reponse to a NICpush()
    void phase1(vector<int> new_host_dsts, int current_commit_queue); // compute sending rates
    void enqueue_commit(int slice, int current_commit_queue, int Nsenders, vector<int> pkts_to_send, vector<vector<int>> q_inds, vector<int> dst_labels);
    void clean_commit(); // clean up the oldest commit queue, returning enqueued packets back to the rlb queues

    vector<vector<int>> get_queue_sizes() {return _rlb_queue_sizes;} // returns a pointer to queue_sizes matrix

    double get_max_send_rate() {return _max_send_rate;}
    double get_slot_time() {return _slot_time;}
    int get_max_pkts() {return _max_pkts;}

private:

    DynExpTopology* _top; // so we can get the pointer to the NIC and the RlbSink

    int _node; // the ID (number) of the host we're running on

    int _H; // total number of hosts in network

    double _link_capacity;

    double _F; // fraction of goodput rate available to RLB (should be 1-[amount reserved for low priority])
    // i.e. when all RLB_commit_queues are sending at max rate, this is the sum of those rates relative to link goodput
    int _Ncommit_queues; // number of commit queues ( = number of rotors = number of ToR uplinks)
    double _mss; // segment size, bytes
    double _hdr; // header size, bytes
    double _link_rate; // link data rate in Bytes / second
    double _slot_time; // seconds

    double _good_rate; // goodput rate, Bytes / second
    double _pkt_ser_time;

    double _max_send_rate; // max. rate at which EACH commit_queue can send, Packets / second
    int _max_pkts; // maximum number of packets EACH commit_queue can send, packets

    vector<vector<deque<Packet*>>> _rlb_queues; // dimensions: 2 (nonlocal, local) x _H (# hosts) x <?> (# packets)
    vector<vector<int>> _rlb_queue_sizes; // dimensions: 2 (nonlocal, local) x _H (# hosts in network)

    vector<deque<Packet*>> _commit_queues; // dimensions: _Ncommit_queues x <?> (# packets)

    int _pull_cnt; // keep track of which queue we're pulling from
    int _push_cnt; // keep track of which queue we're pushing back from (back into rlb_queues)
    int _skip_empty_commits; // we increment this when the NIC tells us to push a packet.

    bool _have_packets; // do ANY of the commit queues have packets to send?
    
    deque<int> _commit_queue_hist; // keep track of the order of commit queues

};



class RlbMaster : public EventSource {
 public:

    RlbMaster(DynExpTopology* top, EventList &eventlist);

    void start();
    void doNextEvent();
    void newMatching();

    void phase1(vector<int> src_hosts, vector<int> dst_hosts);
    void phase2(vector<int> src_hosts, vector<int> dst_hosts);

    vector<int> fairshare1d(vector<int> input, int cap1, bool extra);
    vector<vector<int>> fairshare2d(vector<vector<int>> input, vector<int> cap0, vector<int> cap1);
    vector<vector<int>> fairshare2d_2(vector<vector<int>> input, int cap0, vector<int> cap1);

    DynExpTopology* _top;
    int _current_commit_queue;

    int _H;
    int _N;
    int _hpr;
    int _max_pkts;

    vector<vector<vector<int>>> _working_queue_sizes;

    vector<int> _link_caps_send; // link capacity (in packets) for each sending node
    vector<int> _link_caps_recv; // link capacity (in packets) for each receiving node
    vector<int> _Nsenders; // number of queues sending for each node

    vector<vector<int>> _pkts_to_send; // dims: (host) x ??? number of senders
    vector<vector<vector<int>>> _q_inds; // dims: (host) x (rlb queue index[i][j])

    vector<vector<int>> _proposals; // dims: (host) x ...
    vector<vector<vector<int>>> _accepts; // dims: (sending host) x (receiving host) x ...

    vector<vector<int>> _dst_labels; // dims: (host) x ...

};

#endif
