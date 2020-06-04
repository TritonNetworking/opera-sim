// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        

#include "rlbmodule.h"

// ????????

#include <sstream>
#include <math.h>
#include "queue.h"
#include "ndppacket.h"
#include "rlbpacket.h" // added
#include "queue_lossless.h"

#include "rlb.h"

//////////////////////////////////
//          RLB Module          //
//////////////////////////////////

RlbModule::RlbModule(DynExpTopology* top, EventList &eventlist, int node)
  : EventSource(eventlist,"rlbmodule"), _node(node), _top(top)
{
    _H = _top->no_of_nodes(); // number of hosts

    _F = 1; // Tuning parameter: scales back how much RLB traffic to send

    _Ncommit_queues = _top->no_of_hpr(); // number of commit queues = number of hosts per rack
    _mss = 1436; // packet payload length (bytes)
    _hdr = 64; // header length (bytes)
    _link_rate = 10000000000 / 8;
    _slot_time = timeAsSec((_Ncommit_queues - 1) * _top->get_slicetime(3) + _top->get_slicetime(0)); // seconds (was 0.000281;)
    // ^^^ note: we try to send RLB traffic right up to the reconfiguration point (one pkt serialization & propagation before reconfig.)
    // in general, we have two options:
    // 1. design everything conservatively so we never have to retransmit an RLB packet
    // 2. if we have to retransmit an RLB packet for some other reason anyway, might as well "swing for the fences" (i.e. max out sending time)

    // debug:
    //cout << " slot time = " << _slot_time << " seconds" << endl;
    //cout << "   ( = " << (_Ncommit_queues - 1) << " * " << timeAsSec(_top->get_slicetime(3)) << " + " << timeAsSec(_top->get_slicetime(0)) << endl;

    _good_rate = _link_rate * _mss / (_mss + _hdr); // goodput rate, Bytes / second
    _good_rate = _good_rate / _mss; // goodput rate, Packets / seconds
    _pkt_ser_time = 1 / _good_rate;

    // subtract serialization time of _Ncommit_queues packets, in case they all sent at the same time to the same ToR port 
    
    // !! removed to get a little more bandwidth at the rist of dropping packets.
    //_slot_time = _slot_time - (_Ncommit_queues * _pkt_ser_time);

    // test, to try to drop packets
    //_slot_time = 1.2 * _slot_time; // yes, packets were dropped!

    _max_send_rate = (_F / _Ncommit_queues) * _good_rate;
    _max_pkts = _max_send_rate * _slot_time;

    // NOTE: we're working in packets / second
    //cout << "Initialization:" << endl;
    //cout << "  max_send_rate set to " << _max_send_rate << " packets / second." << endl;
    //cout << "  slot_time set to " << _slot_time << " seconds." << endl;
    //cout << "  max_pkts set to " << _max_pkts << " packets." << endl;

    _rlb_queues.resize(2); // allocate
    for (int i = 0; i < 2; i++)
        _rlb_queues[i].resize(_H);

    _rlb_queue_sizes.resize(2); // allocate & init
    for (int i = 0; i < 2; i++) {
        _rlb_queue_sizes[i].resize(_H);
        for (int j = 0; j < _H; j++)
            _rlb_queue_sizes[i][j] = 0;
    }

    _commit_queues.resize(_Ncommit_queues); // allocate

    _have_packets = false; // we don't start with any packets to send
    _skip_empty_commits = 0; // we don't start with any skips

}

void RlbModule::doNextEvent() {
    clean_commit();
}

void RlbModule::receivePacket(Packet& pkt, int flag)
{

    // debug:
    //cout << "RLBmodule - packet received." << endl;

    // this is either from an Rlb sender or from the network
    if (pkt.get_real_src() == _node) { // src matches, it's from the RLB sender (or it's being returned by a ToR)
        // check the "real" dest, and put it in the corresponding LOCAL Rlb queue.
        int queue_ind = pkt.get_real_dst();
        if (flag == 0)
            _rlb_queues[1][queue_ind].push_back(&pkt);
        else if (flag == 1) {
            _rlb_queues[1][queue_ind].push_front(&pkt);

            // debug:
            //cout << "RLBmodule[node" << _node << "] - received a bounced packet at " << timeAsUs(eventlist().now()) << " us" << endl;

            //if (pkt.get_time_sent() == 342944606400 && pkt.get_real_src() == 177 && pkt.get_real_dst() == 423)
            //        cout << "debug @rlbmodule: packet bounced to direct queue at node " << _node << endl;

            
        }
        else
            abort(); // undefined flag
        _rlb_queue_sizes[1][queue_ind]++; // increment number of packets in queue

        // debug:
        //cout << "RLBmodule[node" << _node << "] - put packet in local queue at " << timeAsUs(eventlist().now()) <<
        //    " us for dst = " << queue_ind << endl;
        //cout << "   queuesize = " << _rlb_queue_sizes[1][queue_ind] << " packets." << endl;

        // debug:
        //RlbPacket *p = (RlbPacket*)(&pkt);
        //if (p->seqno() == 1)
        //    cout << "> marked packet local queued at node " << _node << endl;

    } else {
        // if it's from the network, we either need to enqueue or send to Rlb sink
        if (pkt.get_dst() == _node) {
            if (pkt.get_real_dst() == _node) {

                // debug:
                //cout << "RLBmodule[node" << _node << "] - received a packet to sink at " << timeAsUs(eventlist().now()) << " us" << endl;

                // debug:
                //if (pkt.get_time_sent() == 342944606400 && pkt.get_real_src() == 177 && pkt.get_real_dst() == 423) {
                //    cout << ">>> node " << _node << " received the packet!" << endl;
                //    abort();
                //}

                RlbSink* sink = pkt.get_rlbsink();
                assert(sink);
                sink->receivePacket(pkt); // should this be pkt, *pkt, or &pkt ??? !!!
            } else {
                // check the "real" dest, and put it in the corresponding NON-LOCAL Rlb queue.
                int queue_ind = pkt.get_real_dst();
                pkt.set_src(_node); // change the "source" to this node for routing purposes
                _rlb_queues[0][queue_ind].push_back(&pkt);
                _rlb_queue_sizes[0][queue_ind]++; // increment number of packets in queue

                // debug:
                //if (pkt.get_time_sent() == 342944606400 && pkt.get_real_src() == 177 && pkt.get_real_dst() == 423)
                //    cout << "debug @rlbmodule: packet received to indirect queue " << queue_ind << " at node " << _node << endl;

                // debug:
                //cout << "RLBmodule[node" << _node << "] - indirect packet received at " << timeAsUs(eventlist().now()) <<
                //    " us, put in queue " << queue_ind << endl;
                // debug:
                //RlbPacket *p = (RlbPacket*)(&pkt);
                //if (p->seqno() == 1)
                //    cout << "> marked packet nonlocal queued at node " << _node << endl;
                
            }
        } else {
            // a nonlocal packet was returned by the ToR (must've been delayed by high-priority traffic)
            if (pkt.get_src() == _node) {
                // we check pkt.get_src() to at least make sure it was sent from this node
                assert(flag == 1); // we also require the "enqueue_front" flag be set

                // debug:
                //if (pkt.get_time_sent() == 342944606400 && pkt.get_real_src() == 177 && pkt.get_real_dst() == 423)
                //    cout << "debug @rlbmodule: packet was indirect and bounced back to node " << _node << endl;

                // debug:
                //cout << "bounced NON-LOCAL RLB packet returned to RlbModule at node " << _node << endl;
                //cout << "   real_dst = " << pkt.get_real_dst() << endl;
                //cout << "   time = " << timeAsUs(eventlist().now()) << " us" << endl;

                // check the "real" dest, and put it in the corresponding NON-LOCAL Rlb queue.
                int queue_ind = pkt.get_real_dst();
                _rlb_queues[0][queue_ind].push_front(&pkt);
                _rlb_queue_sizes[0][queue_ind]++; // increment number of packets in queue

            } else {
                cout << "RLB packet received at wrong host: get_dst() = " << pkt.get_dst() << ", _node = " << _node << endl;
                cout << "... get_real_dst() = " << pkt.get_real_dst() << ", get_real_src() = " << pkt.get_real_src() << ", get_src() = " << pkt.get_src() << endl;
                abort();
            }
        }
    }
}

Packet* RlbModule::NICpull()
{
    // NIC is requesting to pull a packet from the commit queues

    // debug:
    //cout << "   Rlbmodule[node" << _node << "] - the NIC pulled a packet at " << timeAsUs(eventlist().now()) << " us." << endl;

    Packet* pkt;

    //cout << "_pull_cnt0 = " << _pull_cnt << endl;

    assert(_have_packets); // assert that we have packets to send

    // start pulling at commit_queues[_pull_cnt]
    bool found_queue = false;
    while (!found_queue) {
        if (!_commit_queues[_pull_cnt].empty()) {
            // the queue is not empty
            pkt = _commit_queues[_pull_cnt].front();
            _commit_queues[_pull_cnt].pop_front();

            if (!pkt->is_dummy()) {

                // debug:
                //if (_node == 0)
                //    cout << " node 0: pull_cnt = " << _pull_cnt << ", a packet was pulled with dst = " << pkt->get_dst() << endl;

                // it's a "real" packet
                _pull_cnt++;
                _pull_cnt = _pull_cnt % _Ncommit_queues;
                found_queue = true;

                //cout << " real packet" << endl;

                // debug:
                //RlbPacket *p = (RlbPacket*)(pkt);
                //if (p->seqno() == 1)
                //    cout << "< marked packet pulled by NIC at node " << _node << endl;

            } else if (_skip_empty_commits > 0) {

                // debug:
                //if (_node == 0)
                //    cout << " node 0: pull_cnt = " << _pull_cnt << ", skipping a dummy packet" << endl;

                // it's a dummy packet, but we can skip it
                _pull_cnt++;
                _pull_cnt = _pull_cnt % _Ncommit_queues;
                _push_cnt--;
                _push_cnt = _push_cnt % _Ncommit_queues;
                if (_push_cnt < 0)
                    _push_cnt = (-1) * _push_cnt;

                _skip_empty_commits--;

                //cout << " skipped dummy packet" << endl;

            } else {

                // debug:
                //if (_node == 0)
                //    cout << " node 0: pull_cnt = " << _pull_cnt << ", can't skip a dummy packet" << endl;

                // it's a dummy packet, and we can't skip it
                _pull_cnt++;
                _pull_cnt = _pull_cnt % _Ncommit_queues;
                found_queue = true;

                //cout << " dummy packet" << endl;
                
            }
        } else if (_skip_empty_commits > 0) {

            // debug:
            //if (_node == 0)
            //    cout << " node 0: pull_cnt = " << _pull_cnt << ", queue empty, skipping..." << endl;

            // the queue is empty, but we can skip it
            _pull_cnt++;
            _pull_cnt = _pull_cnt % _Ncommit_queues;
            _push_cnt--;
            _push_cnt = _push_cnt % _Ncommit_queues;
            if (_push_cnt < 0)
                _push_cnt = (-1) * _push_cnt;

            _skip_empty_commits--;

            //cout << " skipped empty queue" << endl;

        } else {

            // debug:
            //if (_node == 0)
            //    cout << " node 0: pull_cnt = " << _pull_cnt << ", queue empty, can't skip, giving a dummy packet " << endl;

            // the queue is empty, and we can't skip it
            // make a new dummy packet
            pkt = RlbPacket::newpkt(_mss); // make a dummy packet
            pkt->set_dummy(true);

            _pull_cnt++;
            _pull_cnt = _pull_cnt % _Ncommit_queues;
            found_queue = true;

            //cout << " empty queue = dummy packet" << endl;
        }
    }

    // check if we need to update _have_packets and doorbell the NIC:
    check_all_empty();

    // we can't save _skip_empty_commits, otherwise we'd send too fast later
    // if we have extra, we need to push that many packets back into the rlb queues
    if (_have_packets) {
        if (_skip_empty_commits > 0)
            commit_push(_skip_empty_commits);
    } else { // there already aren't any packets left, so no need to commit_push
        _skip_empty_commits = 0;
    }

    return pkt;

}

void RlbModule::check_all_empty()
{
    bool any_have = false;
    for (int i = 0; i < _Ncommit_queues; i++) {
        if (!_commit_queues[i].empty()){
            any_have = true;
            break;
        }
    }
    if (!any_have) {
        _have_packets = false;

        //cout << "All queues empty." << endl;

        // !! doorbell the NIC here that there are no packets
        PriorityQueue* nic = dynamic_cast<PriorityQueue*>(_top->get_queue_serv_tor(_node)); // returns pointer to nic queue
        nic->doorbell(false);
    }
}

void RlbModule::NICpush()
{
    if (_have_packets) {
        _skip_empty_commits++;
        // only keep up to _Ncommit_queues - 1 skips (this is all we need)
        int extra_skips = _skip_empty_commits - (_Ncommit_queues - 1);
        if (extra_skips > 0)
            commit_push(extra_skips);
    }
}

void RlbModule::commit_push(int num_to_push)
{
    // push packets back into rlb queues at _commit_queues[_push_cnt]
    for (int i = 0; i < num_to_push; i++) {
        if (!_commit_queues[_push_cnt].empty()) {
            Packet* pkt = _commit_queues[_push_cnt].back();
            if (!pkt->is_dummy()) { // it's a "real" packet

                // figure out if it's a local packet or nonlocal packet
                // and push back to the appropriate queue

                if(pkt->get_real_src() == _node) { // it's a local packet
                    _rlb_queues[1][pkt->get_real_dst()].push_front(pkt);
                    _rlb_queue_sizes[1][pkt->get_real_dst()] ++;
                    _commit_queues[_push_cnt].pop_back();

                    // debug:
                    //if (pkt->get_time_sent() == 342944606400 && pkt->get_real_src() == 177 && pkt->get_real_dst() == 423) {
                    //    cout << "debug @ rlbmodule commit_push:" << endl;
                    //    cout << " returned packet to local queue " << pkt->get_real_dst() << endl;
                    //}

                } else { // it's a nonlocal packet
                    _rlb_queues[0][pkt->get_real_dst()].push_front(pkt);
                    _rlb_queue_sizes[0][pkt->get_real_dst()] ++;
                    _commit_queues[_push_cnt].pop_back();

                    // debug:
                    //if (pkt->get_time_sent() == 342944606400 && pkt->get_real_src() == 177 && pkt->get_real_dst() == 423) {
                    //    cout << "debug @ rlbmodule commit_push:" << endl;
                    //    cout << " returned packet to nonlocal queue " << pkt->get_real_dst() << endl;
                    //}

                }

            } else { // it's a dummy packet
                _commit_queues[_push_cnt].pop_back();
                pkt->free(); // delete the dummy packet
            }
        }
        _skip_empty_commits--;
        _push_cnt--;
        _push_cnt = _push_cnt % _Ncommit_queues;
        if (_push_cnt < 0)
            _push_cnt = (-1) * _push_cnt;
    }
    check_all_empty();
}

void RlbModule::enqueue_commit(int slice, int current_commit_queue, int Nsenders, vector<int> pkts_to_send, vector<vector<int>> q_inds, vector<int> dst_labels)
{

    // convert #packets_to_be_sent into sending_rates:

    vector<double> sending_rates;
    for (int i = 0; i < Nsenders; i++) {
        sending_rates.push_back(pkts_to_send[i] / _slot_time);

        // debug:
        //if (_node == 0)
        //    cout << "_node " << _node << ", sender " << i << ", " << pkts_to_send[i] << " packets to send in " << _slot_time << " seconds" << endl;
    }

    // debug:
    //cout << "RLBmodule[node" << _node << "] - enqueuing commit queue[" << current_commit_queue << "] at " << timeAsUs(eventlist().now()) << " us." << endl;

    // push this commit_queue to the history
    _commit_queue_hist.push_back(current_commit_queue);

    // if we previously didn't have any packets, reset the _pull_cnt & _push_cnt accordingly
    // also, doorbell the NIC
    if (!_have_packets) {
        _have_packets = true;
        _pull_cnt = current_commit_queue;
        _push_cnt = current_commit_queue-1;
        _push_cnt = _push_cnt % _Ncommit_queues; // wrap around
        if (_push_cnt < 0)
            _push_cnt = (-1) * _push_cnt;

        // !!! doorbell to the priority queue (NIC) that we have packets now.
        PriorityQueue* nic = dynamic_cast<PriorityQueue*>(_top->get_queue_serv_tor(_node)); // returns pointer to nic queue
        nic->doorbell(true);

    }

    // use rates to get send times:
    vector<std::deque<double>> send_times; // vector of queues containing send times (seconds)
    send_times.resize(Nsenders);
    for (int i = 0; i < Nsenders; i++) {
        double inter_time = 1 / sending_rates[i];
        int cnt = 0;
        double time = cnt * inter_time; // seconds
        while (time < _slot_time) {
            send_times[i].push_back(time);
            cnt++;
            time = cnt * inter_time;
        }
    }
    // make sure we have at least one time past the end time:
    // this ensures we never pop the last send time, which would cause problems during enqueue
    for (int i = 0; i < Nsenders; i++)
        send_times[i].push_back(2 * _slot_time);


    // debug:
    //cout << "The queue is:" << endl;

    // use send times to fill the queue
    for (int i = 0; i < _max_pkts; i++){
        bool found_queue = false;
        while (!found_queue) {
            double end_slot_time = (i + 1) * (_Ncommit_queues * _pkt_ser_time); // send_time must be less than this to send in this slot
            int fst_sndr = -1; // find the queue that sends first
            for (int j = 0; j < Nsenders; j++) {
                if (send_times[j].front() < end_slot_time) {
                    fst_sndr = j;
                    end_slot_time = send_times[j].front(); // new time to beat
                }
            }
            if (fst_sndr != -1) { // there was a queue that can send
                // now check if the queue has packets
                if (_rlb_queue_sizes[q_inds[0][fst_sndr]][q_inds[1][fst_sndr]] > 0) {
                    // not empty, pop the send_time, move the packet, and break
                    send_times[fst_sndr].pop_front();

                    // debug:
                    //cout << fst_sndr << " ";

                    // debug:
                    //cout << " Rlbmodule - committing from queue[" << q_inds[0][fst_sndr] << "][" << q_inds[1][fst_sndr] << "], chaning destination to " << dst_labels[fst_sndr] << endl;

                    // get the packet, and set its destination:
                    Packet* pkt;
                    pkt = _rlb_queues[q_inds[0][fst_sndr]][q_inds[1][fst_sndr]].front();
                    pkt->set_dst(dst_labels[fst_sndr]);

                    // debug:
                    //if (pkt->get_time_sent() == 342944606400 && pkt->get_real_src() == 177 && pkt->get_real_dst() == 423) {
                    //    cout << "debug @ rlbmodule commit:" << endl;
                    //    cout << " _node = " << _node << endl;
                    //    cout << " pkt->get_dst() = " << pkt->get_dst() << endl;
                    //    cout << " slice = " << slice << endl;
                    //}

                    // we need to timestamp the packet for routing when it is committed to be sent:
                    pkt->set_slice_sent(slice); // "timestamp" the packet

                    // debug:
                    //if (_node == 0)
                    //    cout << "   packet committed from sending queue: " << fst_sndr << ", for dest: " << dst_labels[fst_sndr] << endl;

                    // debug:
                    //RlbPacket *p = (RlbPacket*)(pkt);
                    //if (p->seqno() == 1)
                    //    cout << "* marked packet committed at node: " << _node << " for dst: " << pkt->get_dst() << " in slice: " << slice << endl;


                    // put the packet in the commit queue
                    _commit_queues[current_commit_queue].push_back( pkt );
                    _rlb_queues[q_inds[0][fst_sndr]][q_inds[1][fst_sndr]].pop_front(); // remove the packet from the rlb queue
                    _rlb_queue_sizes[q_inds[0][fst_sndr]][q_inds[1][fst_sndr]]--; // decrement queue size by one packet
                    found_queue = true;
                } else // the queue was empty, pop the send time and keep looking
                    send_times[fst_sndr].pop_front();
            } else { // no queues can send right now; put a dummy packet into the commit queue and go to next commit_queue slot

                // debug:
                //cout << "_ ";

                Packet* p = RlbPacket::newpkt(_mss); // make a dummy packet
                p->set_dummy(true);

                _commit_queues[current_commit_queue].push_back(p);
                found_queue = true;
            }
        }
    }

    // debug:
    //cout << endl;

    // debug:
    //if (_node == 0){
    //    for (int i = 0; i < _Ncommit_queues; i++)
    //        cout << " _node 0: commit_queue: " << i << ". Length = " << _commit_queues[i].size() << " packets" << endl;
    //}

    // create a delayed event that cleans this queue after the slot ends
    eventlist().sourceIsPendingRel(*this, timeFromSec(_slot_time));
}

void RlbModule::clean_commit()
{
    // clean up the oldest commit queue
    int oldest = _commit_queue_hist.front();

    // debug:
    //cout << "Cleaning commit queue[" << oldest << "] at " << timeAsUs(eventlist().now()) << " us." << endl;

    // while there are packets in the queue
    // return them to the FRONT of the rlb queues
    while (!_commit_queues[oldest].empty()) {
        Packet* pkt = _commit_queues[oldest].back();
        if (!pkt->is_dummy()) {
            // it's a "real" packet
            // !!! note - hardcoded to push to a local queue
            //_rlb_queues[1][pkt->get_dst()].push_front( _commit_queues[oldest].back() );
            //_rlb_queue_sizes[1][pkt->get_dst()] ++;
            //_commit_queues[oldest].pop_back();

            // debug:
            //if (pkt->get_time_sent() == 342944606400 && pkt->get_real_src() == 177 && pkt->get_real_dst() == 423) {
            //    cout << "debug @ rlbmodule clean_commit:" << endl;
            //    cout << " wanted to return packet to local queue " << pkt->get_dst() << endl;
            //    cout << " fixed to return packet to nonlocal queue " << pkt->get_real_dst() << endl;
            //}

            if (pkt->get_real_src() == _node) {
                // put it back in the local queue
                _rlb_queues[1][pkt->get_real_dst()].push_front( _commit_queues[oldest].back() );
                _rlb_queue_sizes[1][pkt->get_real_dst()] ++;
                _commit_queues[oldest].pop_back();
            } else {
                // put it back in the nonlocal queue
                _rlb_queues[0][pkt->get_real_dst()].push_front( _commit_queues[oldest].back() );
                _rlb_queue_sizes[0][pkt->get_real_dst()] ++;
                _commit_queues[oldest].pop_back();
            }

        } else {
            // it's a dummy packet
            _commit_queues[oldest].pop_back();
            pkt->free(); // delete the dummy packet
        }
    }

    // check if we need to doorbell NIC that there are no more packets
    // we only do this if we just had some packets, to prevent doorbelling the NIC twice
    if (_have_packets)
        check_all_empty();

    // pop this commit_queue from the history
    _commit_queue_hist.pop_front();
}





//////////////////////////////////
//          RLB Master          //
//////////////////////////////////

// this is what drives the RLB module


RlbMaster::RlbMaster(DynExpTopology* top, EventList &eventlist)
  : EventSource(eventlist,"rlbmaster"), _top(top)
{

    _current_commit_queue = 0; // the topology is defined to start here (Rotor switch 0)

    _H = _top->no_of_nodes(); // number of hosts
    _N = _top->no_of_tors(); // number of racks
    _hpr = _top->no_of_hpr(); // number of hosts per rack

    _working_queue_sizes.resize(_H);
    for (int i = 0; i < _H; i++) {
        _working_queue_sizes[i].resize(2);
        for (int j = 0; j < 2; j++) {
            _working_queue_sizes[i][j].resize(_H);
        }
    }

    _q_inds.resize(_H);
    for (int i = 0; i < _H; i++)
        _q_inds[i].resize(2);

    _link_caps_send.resize(_H);
    _link_caps_recv.resize(_H);

    _Nsenders.resize(_H);

    _pkts_to_send.resize(_H);

    _dst_labels.resize(_H);

    _proposals.resize(_H);

    _accepts.resize(_H);
    for (int i = 0; i < _H; i++)
        _accepts[i].resize(_H);


}

void RlbMaster::start() {
    // set it up to start "ticking" at time 0 (the first rotor reconfiguration)
    eventlist().sourceIsPending(*this, timeFromSec(0));
}

void RlbMaster::doNextEvent() {
    newMatching();
}

void RlbMaster::newMatching() {

    // get the current slice:
    // first, get the current "superslice"
    int64_t superslice = (eventlist().now() / _top->get_slicetime(3)) %
    _top->get_nsuperslice();
    // next, get the relative time from the beginning of that superslice
    int64_t reltime = eventlist().now() - superslice*_top->get_slicetime(3) -
        (eventlist().now() / (_top->get_nsuperslice()*_top->get_slicetime(3))) * 
        (_top->get_nsuperslice()*_top->get_slicetime(3));
    int slice; // the current slice
    if (reltime < _top->get_slicetime(0))
        slice = 0 + superslice*3;
    else if (reltime < _top->get_slicetime(0) + _top->get_slicetime(1))
        slice = 1 + superslice*3;
    else
        slice = 2 + superslice*3;


    // debug:
    //cout << "-*-*- New matching at slice = " << slice << ", time = " << timeAsUs(eventlist().now()) << " us -*-*-" << endl;

    // copy queue sizes from each host into a datastructure that covers all hosts
    RlbModule* mod;
    for (int host = 0; host < _H; host++) {
        mod = _top->get_rlb_module(host); // get pointer to module
        vector<vector<int>> _queue_sizes_temp = mod->get_queue_sizes(); // get queue sizes from module
        for (int j = 0; j < 2; j++) {
            for (int k = 0; k < _H; k++)
                _working_queue_sizes[host][j][k] = _queue_sizes_temp[j][k];
        }
    }

    // copy some other parameters over:
    mod = _top->get_rlb_module(0);
    _max_pkts = mod->get_max_pkts();

    // clear any previous queue indices
    for (int i = 0; i < _H; i++)
        for (int j = 0; j < 2; j++)
            _q_inds[i][j].resize(0);

    // clear any previous packets-to-send counters
    for (int i = 0; i < _H; i++)
        _pkts_to_send[i].resize(0);

    // clear any previous _dst_labels
    for (int i = 0; i < _H; i++)
        _dst_labels[i].resize(0);

    // clear any previous _proposals
    for (int i = 0; i < _H; i++)
        _proposals[i].resize(0);

    // clear any previous _accpets
    for (int i = 0; i < _H; i++)
        for (int j = 0; j < _H; j++)
            _accepts[i][j].resize(0);
    
    // initialize link capacities to full capacity:
    for (int i = 0; i < _H; i++) {
        _link_caps_send[i] = _max_pkts;
        _link_caps_recv[i] = _max_pkts;
    }



    // ---------- phase 1 ---------- //

    for (int crtToR = 0; crtToR < _N; crtToR++) {

        // get the list of new dst hosts (that every host in this rack is now connected to)
        int dstToR = _top->get_nextToR(slice, crtToR, _current_commit_queue + _hpr);
        
        if (crtToR != dstToR) { // necessary because of the way we define the topology

            vector<int> dst_hosts;
            int basehost = dstToR * _hpr;
            for (int j = 0; j < _hpr; j++) {
                dst_hosts.push_back(basehost + j);
            }

            // get the list of src hosts in this rack
            vector<int> src_hosts;
            basehost = crtToR * _hpr;
            for (int j = 0; j < _hpr; j++) {
                src_hosts.push_back(basehost + j);
            }

            // get:
            // 1. the 2nd hop "rates"
            // 2. the 1 hop "rates"
            // 3. the 1st hop proposed "rates"
            // all "rates" are in units of packets

            phase1(src_hosts, dst_hosts);
        }
    }


    // ---------- phase 2 ---------- //

    for (int crtToR = 0; crtToR < _N; crtToR++) {

        // get the list of new dst hosts (that every host in this rack is now connected to)
        int dstToR = _top->get_nextToR(slice, crtToR, _current_commit_queue + _hpr);
        
        if (crtToR != dstToR) {

            vector<int> dst_hosts;
            int basehost = dstToR * _hpr;
            for (int j = 0; j < _hpr; j++) {
                dst_hosts.push_back(basehost + j);
            }

            // get the list of src hosts in this rack
            vector<int> src_hosts;
            basehost = crtToR * _hpr;
            for (int j = 0; j < _hpr; j++) {
                src_hosts.push_back(basehost + j);
            }

            // given the proposals, generate the accepts

            phase2(src_hosts, dst_hosts); // the dst_hosts compute how much to accept from the src_hosts
        }
    }


    // ---------- phase 3 ---------- //

    for (int crtToR = 0; crtToR < _N; crtToR++) {

        // get the list of new dst hosts (that every host in this rack is now connected to)
        int dstToR = _top->get_nextToR(slice, crtToR, _current_commit_queue + _hpr);
        
        if (crtToR != dstToR) {

            vector<int> dst_hosts;
            int basehost = dstToR * _hpr;
            for (int j = 0; j < _hpr; j++) {
                dst_hosts.push_back(basehost + j);
            }

            // get the list of src hosts in this rack
            vector<int> src_hosts;
            basehost = crtToR * _hpr;
            for (int j = 0; j < _hpr; j++) {
                src_hosts.push_back(basehost + j);
            }

            // given the accepts, finish getting how many packets to send and such
            // and communicate this to the RlbModule::enqueue_commit()

            for (int j = 0; j < src_hosts.size(); j++){
                for (int k = 0; k < dst_hosts.size(); k++) {

                    for (int l = 0; l < _H; l++) {
                        int temp = _accepts[src_hosts[j]][dst_hosts[k]][l];
                        if (temp > 0) {
                            _Nsenders[src_hosts[j]] ++; // increment sender count
                            _pkts_to_send[src_hosts[j]].push_back(temp); // how many packets we should send
                            _q_inds[src_hosts[j]][0].push_back(1); // first index (0 = nonlocal, 1 = local)
                            _q_inds[src_hosts[j]][1].push_back(l); // second index (which queue)
                            _dst_labels[src_hosts[j]].push_back(dst_hosts[k]);

                            // debug:
                            //cout << "RlbMaster - src: " << src_hosts[j] << ", dst: " << dst_hosts[k] << endl;
                            //cout << "   send " << _pkts_to_send[src_hosts[j]].back() << " packets from local queue " << l << endl;
                        
                            //if (src_hosts[j] == 0 && timeAsUs(eventlist().now()) == 5044) {
                            //    cout << "   *Master: src_host: " << src_hosts[j] << " at " << timeAsUs(eventlist().now()) << " us." << endl;
                            //    cout << "    > send to dst_host = " << dst_hosts[k] << endl;
                            //    cout << "      > from queue = [" << _q_inds[src_hosts[j]][0].back() << "][" << _q_inds[src_hosts[j]][1].back() << "]" << endl;
                            //    cout << "      > # packets = " << _pkts_to_send[src_hosts[j]].back() << endl;
                            //}


                        }
                    }
                }

                // only start if there's something to send...
                if (_Nsenders[src_hosts[j]] > 0) {
                    mod = _top->get_rlb_module(src_hosts[j]);
                    mod->enqueue_commit(slice, _current_commit_queue, _Nsenders[src_hosts[j]], _pkts_to_send[src_hosts[j]], _q_inds[src_hosts[j]], _dst_labels[src_hosts[j]]);
                }
            }
        }

    }


    // --------- set up next new matching --------- //

    _current_commit_queue++;
    _current_commit_queue = _current_commit_queue % _hpr;

    // set up the next reconfiguration event:
    eventlist().sourceIsPendingRel(*this, _top->get_slicetime(3));
}


void RlbMaster::phase1(vector<int> src_hosts, vector<int> dst_hosts)
{

    // -------------------------------------------------------------------- //

    // STEP 1 - decide how many 2nd hop packets to send.
    // this is done on a host level:

    int fixed_pkts = _max_pkts / _hpr; // maximum "safe" number of packets we can send

    for (int i = 0; i < src_hosts.size(); i++) { // sweep senders

        _Nsenders[src_hosts[i]] = 0; // initialize number of sending queues to zero
    
        for (int j = 0; j < dst_hosts.size(); j++) { // sweep destinations
            
            int temp_pkts = _working_queue_sizes[src_hosts[i]][0][dst_hosts[j]]; // 0 = nonlocal
            if (temp_pkts > fixed_pkts)
                temp_pkts = fixed_pkts; // limit to the maximum amount we can safely send

            _link_caps_send[src_hosts[i]] -= temp_pkts; // uses some of sender's capacity
            _link_caps_recv[dst_hosts[j]] -= temp_pkts; // uses some of receiver's capacity
            _working_queue_sizes[src_hosts[i]][0][dst_hosts[j]] -= temp_pkts;

            if (temp_pkts > 0) {
                _Nsenders[src_hosts[i]] ++; // increment sender count
                _pkts_to_send[src_hosts[i]].push_back(temp_pkts); // how many packets we should send

                _q_inds[src_hosts[i]][0].push_back(0); // first index (0 = nonlocal, 1 = local)
                _q_inds[src_hosts[i]][1].push_back(dst_hosts[j]); // second index (which queue)

                _dst_labels[src_hosts[i]].push_back(dst_hosts[j]);
            }
        }
    }

    // -------------------------------------------------------------------- //

    // STEP 2 - decide how many 1 hop packets to send.
    // this is done at a rack level:

    // get the local queue matrix:
    vector<vector<int>> local_pkts_rack;
    local_pkts_rack.resize(src_hosts.size());
    for (int i = 0; i < src_hosts.size(); i++)
        local_pkts_rack[i].resize(dst_hosts.size());

    for (int i = 0; i < src_hosts.size(); i++) {
        for (int j = 0; j < dst_hosts.size(); j++) {
            local_pkts_rack[i][j] = _working_queue_sizes[src_hosts[i]][1][dst_hosts[j]]; // 1 = local
        }
    }

    // get vectors of the capacities:
    vector<int> src_caps;
    for (int i = 0; i < src_hosts.size(); i++)
        src_caps.push_back(_link_caps_send[src_hosts[i]]);

    // get vectors of the capacities:
    vector<int> dst_caps;
    for (int i = 0; i < dst_hosts.size(); i++)
        dst_caps.push_back(_link_caps_recv[dst_hosts[i]]);


    /*
    // debug:
    if (src_hosts[0] == 0 && dst_hosts[0] == 276) { // the first rack is sending
        cout << "src_caps = ";
        for (int i = 0; i < src_hosts.size(); i++)
            cout << src_caps[i] << " ";
        cout << endl;
        cout << "dst_caps = ";
        for (int i = 0; i < src_hosts.size(); i++)
            cout << dst_caps[i] << " ";
        cout << endl << "input[src_host, dst_host] =" << endl;
        for (int i = 0; i < src_hosts.size(); i++) {
            for (int j = 0; j < dst_hosts.size(); j++)
                cout << local_pkts_rack[i][j] << " ";
            cout << endl;
        }
    }
    */

    // fairshare across both dimensions
    local_pkts_rack = fairshare2d(local_pkts_rack, src_caps, dst_caps);

    /*
    // ------------------------------------------------
    // debug:
    if (src_hosts[0] == 0 && dst_hosts[0] == 276) { // the first rack is sending
        cout << "dest_hosts: ";
        for (int i = 0; i < dst_hosts.size(); i++)
            cout << " " << dst_hosts[i];
        cout << endl;
        for (int i = 0; i < src_hosts.size(); i++) {
            cout << "src_host " << src_hosts[i] << ":";
            for (int j = 0; j < dst_hosts.size(); j++)
                cout << " " << local_pkts_rack[i][j];
            cout << endl;
        }
    }
    // ------------------------------------------------
    */

    // update sending capacities
    for (int i = 0; i < src_hosts.size(); i++) {
        int temp = 0;
        for (int j = 0; j < dst_hosts.size(); j++)
            temp += local_pkts_rack[i][j];
        _link_caps_send[src_hosts[i]] -= temp;
    }

    // update receiving capacities
    for (int i = 0; i < dst_hosts.size(); i++) {
        int temp = 0;
        for (int j = 0; j < src_hosts.size(); j++)
            temp += local_pkts_rack[j][i];
        _link_caps_recv[dst_hosts[i]] -= temp;
    }

    // update sender count, packets sent, queue indices, and decrement queue sizes
    for (int i = 0; i < src_hosts.size(); i++) {
        for (int j = 0; j < dst_hosts.size(); j++) {
            int temp_pkts = local_pkts_rack[i][j];
            if (temp_pkts > 0) {
                // some packets can be sent:
                _Nsenders[src_hosts[i]] ++; // increment sender count
                _pkts_to_send[src_hosts[i]].push_back(temp_pkts); // how many packets we should send
                _q_inds[src_hosts[i]][0].push_back(1); // first index (0 = nonlocal, 1 = local)
                _q_inds[src_hosts[i]][1].push_back(dst_hosts[j]); // second index (which queue)
                _working_queue_sizes[src_hosts[i]][1][dst_hosts[j]] -= temp_pkts;
                _dst_labels[src_hosts[i]].push_back(dst_hosts[j]);
            }
        }
    }

    // -------------------------------------------------------------------- //

    // STEP 3 - propose traffic

    for (int i = 0; i < src_hosts.size(); i++) {
        vector<int> temp = _working_queue_sizes[src_hosts[i]][1];
        for (int j = 0; j < dst_hosts.size(); j++) {
            temp[dst_hosts[j]] = 0; // remove any elements corresponding to hosts in the dest. rack
        }
        temp = fairshare1d(temp, _link_caps_send[src_hosts[i]], true); // fairshare according to remaining link capacity
        _proposals[src_hosts[i]] = temp;
    }

}


void RlbMaster::phase2(vector<int> src_hosts, vector<int> dst_hosts)
{
    // the dst_hosts compute how much to accept from the src_hosts

    // NOTE: MODIFICATION: can add a factor of two, which helps with throughput (a little)
    // This extra factor of two might delay traffic by 1 cycle though...
    int fixed_pkts = 2 * _max_pkts / _hpr; // maximum "safe" number of packets we can have per destination
    //int fixed_pkts = _max_pkts / _hpr; // maximum "safe" number of packets we can have per destination

    for (int i = 0; i < dst_hosts.size(); i++) { // sweep destinations

        vector<int> temp = _working_queue_sizes[dst_hosts[i]][0];
        for (int j = 0; j < _H; j++) {
            temp[j] += _working_queue_sizes[dst_hosts[i]][1][j];
            temp[j] = fixed_pkts - temp[j];
            if (temp[j] < 0) {
                temp[j] = 0;
            }
        }

        vector<vector<int>> all_proposals;
        all_proposals.resize(src_hosts.size());
        for (int j = 0; j < src_hosts.size(); j++)
            all_proposals[j] = _proposals[src_hosts[j]];

        // fairshare, according to queue availability + receiving link capacity
        all_proposals = fairshare2d_2(all_proposals, _link_caps_recv[dst_hosts[i]], temp);

        for (int j = 0; j < src_hosts.size(); j++)
            _accepts[src_hosts[j]][dst_hosts[i]] = all_proposals[j];
    }
}



vector<int> RlbMaster::fairshare1d(vector<int> input, int cap1, bool extra) {

    vector<int> sent;
    sent.resize(input.size());
    for (int i = 0; i < input.size(); i++)
        sent[i] = input[i];

    int nelem = 0;
    for (int i = 0; i < input.size(); i++)
        if (input[i] > 0)
            nelem++;

    if (nelem != 0) {
        bool cont = true;
        while (cont) {
            int f = cap1 / nelem; // compute the fair share
            int min = 0;
            for (int i = 0; i < input.size(); i++) {
                if (input[i] > 0) {
                    input[i] -= f;
                    if (input[i] < 0)
                        min = input[i];
                }
            }
            cap1 -= f * nelem;
            if (min < 0) { // some elements got overserved
                //cap1 = 0;
                for (int i = 0; i < input.size(); i++)
                    if (input[i] < 0) {
                        cap1 += (-1) * input[i];
                        input[i] = 0;
                    }
                nelem = 0;
                for (int i = 0; i < input.size(); i++)
                    if (input[i] > 0)
                        nelem++;
                if (nelem == 0) {
                    cont = false;
                }
            } else {
                cont = false;
            }
        }
    }

    nelem = 0;
    for (int i = 0; i < input.size(); i++)
        if (input[i] > 0)
            nelem++;

    if (nelem != 0 && cap1 > 0 && extra) {

        // debug:
        /*cout << "nelem = " << nelem << ", cap1 = " << cap1 << ", input.size() = " << input.size() << endl;
        cout << "input @ extra =";
        for (int i = 0; i < input.size(); i++)
            cout << " " << input[i];
        cout << endl;*/

        // randomly assign any remainders
        while (cap1 > 0) {
            int nelem = 0;
            vector <int> inds; // queue indices that still want packets
            for (int i = 0; i < input.size(); i++)
                if (input[i] > 0) {
                    inds.push_back(i);
                    nelem++;
                }
            if (nelem == 0)
                break; // exit the while loop if we had too much extra capacity (cap1)
            int ind = rand() % nelem;
            input[inds[ind]]--;
            cap1--;
        }
    }
    
    
    for (int i = 0; i < input.size(); i++)
        sent[i] -= input[i];

    return sent; // return what was sent
    
    //return input; // return what's left
}

vector<vector<int>> RlbMaster::fairshare2d(vector<vector<int>> input, vector<int> cap0, vector<int> cap1) {

    // if we take `input` as an N x M matrix (N rows, M columns)
    // then cap0[i] is the capacity of the sum of the i-th row
    // and cap1[i] is the capacity of the sum of the i-th column

    // build output
    vector<vector<int>> sent;
    sent.resize(input.size());
    for (int i = 0; i < input.size(); i++) {
        sent[i].resize(input[0].size());
        for (int j=0; j < input[0].size(); j++)
            sent[i][j] = 0;
    }

    int maxiter = 5;
    int iter = 0;

    int nelem = 0;
    for (int i = 0; i < input.size(); i++)
        for (int j=0; j < input[0].size(); j++)
            if (input[i][j] > 0)
                nelem++;

    while (nelem != 0 && iter < maxiter) {

        // temporary matrix:
        vector<vector<int>> sent_temp;
        sent_temp.resize(input.size());
        for (int i = 0; i < input.size(); i++) {
            sent_temp[i].resize(input[0].size());
            for (int j=0; j < input[0].size(); j++)
                sent_temp[i][j] = 0;
        }

        // sweep rows (i): (cols j)
        for (int i = 0; i < input.size(); i++) {
            int prev_alloc = 0;
            for (int j = 0; j < input[0].size(); j++)
                prev_alloc += sent[i][j];
            sent_temp[i] = fairshare1d(input[i], cap0[i] - prev_alloc, true);
        }

        // sweep columns (i): (rows j)
        for (int i = 0; i < input[0].size(); i++) {
            int prev_alloc = 0;
            vector<int> temp_vect;
            for (int j = 0; j < input.size(); j++) {
                prev_alloc += sent[j][i];
                temp_vect.push_back(sent_temp[j][i]);
            }
            temp_vect = fairshare1d(temp_vect, cap1[i] - prev_alloc, true);
            for (int j = 0; j < input.size(); j++)
                sent_temp[j][i] = temp_vect[j];
        }


        // update the `sent` matrix with the `sent_temp` matrix:
        for (int i = 0; i < input.size(); i++)
            for (int j=0; j < input[0].size(); j++)
                sent[i][j] += sent_temp[i][j];

        // update the input matrix:
        for (int i = 0; i < input.size(); i++) {
            for (int j=0; j < input[0].size(); j++) {
                input[i][j] -= sent_temp[i][j];
                if (input[i][j] < 0)
                    input[i][j] = 0;
            }
        }

        // work our way "backwards", checking if cap1[] and cap0[] have been used up

        // cap1[] used up? if so, set the column to zero
        // (sweep columns = i)
        for (int i = 0; i < input[0].size(); i++) {
            int remain = cap1[i];
            for (int j = 0; j < input.size(); j++)
                remain -= sent[j][i];

            if (remain <= 0) {
                for (int j = 0; j < input.size(); j++)
                    input[j][i] = 0;
            }
        }

        // cap0[] used up? if so, set the row to zero
        // (sweep rows = i)
        for (int i = 0; i < input.size(); i++) {
            int remain = cap0[i];
            for (int j = 0; j < input[0].size(); j++)
                remain -= sent[i][j];

            if (remain <= 0) {
                for (int j = 0; j < input.size(); j++)
                    input[i][j] = 0;
            }
        }

        // get number of remaining elements:
        nelem = 0;
        for (int i = 0; i < input.size(); i++)
            for (int j=0; j < input[0].size(); j++)
                if (input[i][j] > 0)
                    nelem++;

        iter++;
    }

    return sent; // return what was sent

}



vector<vector<int>> RlbMaster::fairshare2d_2(vector<vector<int>> input, int cap0, vector<int> cap1) {

    // if we take `input` as an N x M matrix (N rows, M columns)
    // then cap0 is the capacity shared across all elements
    // and cap1[i] is the capacity of the sum of the i-th column

    // build output
    vector<vector<int>> sent;
    sent.resize(input.size());
    for (int i = 0; i < input.size(); i++) {
        sent[i].resize(input[0].size());
        for (int j=0; j < input[0].size(); j++)
            sent[i][j] = 0;
    }

    int maxiter = 5;
    int iter = 0;

    int nelem = 0;
    for (int i = 0; i < input.size(); i++)
        for (int j=0; j < input[0].size(); j++)
            if (input[i][j] > 0)
                nelem++;

    while (nelem != 0 && iter < maxiter) {

        int prev_alloc;

        // temporary matrix:
        vector<vector<int>> sent_temp;
        sent_temp.resize(input.size());
        for (int i = 0; i < input.size(); i++) {
            sent_temp[i].resize(input[0].size());
            for (int j=0; j < input[0].size(); j++)
                sent_temp[i][j] = 0;
        }

        // sweep all elements
        // sum prev alloc & make into 1d vector
        vector<int> temp_all;
        temp_all.resize(input.size() * input[0].size());
        int cnt = 0;
        prev_alloc = 0;

        for (int i = 0; i < input.size(); i++) 
            for (int j = 0; j < input[0].size(); j++) {
                prev_alloc += sent[i][j];
                temp_all[cnt] = input[i][j];
                cnt++;
            }
        temp_all = fairshare1d(temp_all, cap0 - prev_alloc, true);
        cnt = 0;
        for (int i = 0; i < input.size(); i++) 
            for (int j = 0; j < input[0].size(); j++) {
                sent_temp[i][j] = temp_all[cnt];
                cnt++;
            }
    

        // sweep columns (i): (rows j)
        for (int i = 0; i < input[0].size(); i++) {
            prev_alloc = 0;
            vector<int> temp_vect;
            for (int j = 0; j < input.size(); j++) {
                prev_alloc += sent[j][i];
                temp_vect.push_back(sent_temp[j][i]);
            }
            temp_vect = fairshare1d(temp_vect, cap1[i] - prev_alloc, true);
            for (int j = 0; j < input.size(); j++)
                sent_temp[j][i] = temp_vect[j];
        }


        // update the `sent` matrix with the `sent_temp` matrix:
        for (int i = 0; i < input.size(); i++)
            for (int j=0; j < input[0].size(); j++)
                sent[i][j] += sent_temp[i][j];

        // update the input matrix:
        for (int i = 0; i < input.size(); i++) {
            for (int j=0; j < input[0].size(); j++) {
                input[i][j] -= sent_temp[i][j];
                if (input[i][j] < 0)
                    input[i][j] = 0;
            }
        }

        // work our way "backwards", checking if cap1[] and cap0 have been used up

        // cap1[] used up? if so, set the column to zero
        // (sweep columns = i)
        for (int i = 0; i < input[0].size(); i++) {
            int remain = cap1[i];
            for (int j = 0; j < input.size(); j++)
                remain -= sent[j][i];

            if (remain <= 0) {
                for (int j = 0; j < input.size(); j++)
                    input[j][i] = 0;
            }
        }

        // cap0 used up? if so, break (i.e. set the entire matrix to zero)
        int remain = cap0;
        for (int i = 0; i < input.size(); i++)
            for (int j = 0; j < input[0].size(); j++)
                remain -= sent[i][j];
        if (remain <= 0)
            break;

        // get number of remaining elements:
        nelem = 0;
        for (int i = 0; i < input.size(); i++)
            for (int j=0; j < input[0].size(); j++)
                if (input[i][j] > 0)
                    nelem++;

        iter++;
    }

    return sent; // return what was sent

}
