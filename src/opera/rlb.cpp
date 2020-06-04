// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-    
#include <math.h>
#include <iostream>
#include "rlb.h"
#include "queue.h"
#include <stdio.h>

#include "rlbmodule.h"

////////////////////////////////////////////////////////////////
//  RLB SOURCE
////////////////////////////////////////////////////////////////

RlbSrc::RlbSrc(DynExpTopology* top, NdpLogger* logger, TrafficLogger* pktlogger, EventList &eventlist, int flow_src, int flow_dst)
    : EventSource(eventlist,"rlbsrc"), _logger(logger), _flow(pktlogger), _flow_src(flow_src), _flow_dst(flow_dst), _top(top) {
    
    _mss = 1436; // Packet::data_packet_size(); // maximum segment size (mss)
    _sink = 0;
    //_flow_size = ((uint64_t)1)<<63; // this should always get set after `new RlbSrc()` gets called
    _pkts_sent = 0;

    //_node_num = _global_node_count++;
    //_nodename = "rlbsrc" + to_string(_node_num);
}

void RlbSrc::connect(RlbSink& sink, simtime_picosec starttime) {
    
    _sink = &sink;
    _flow.id = id; // identify the packet flow with the source that generated it
    _flow._name = _name;
    _sink->connect(*this);

    set_start_time(starttime); // record the start time in _start_time
    eventlist().sourceIsPending(*this,starttime);
}

void RlbSrc::startflow() {
    _sent = 0;

    // debug:
    //cout << "flow size = " << _flow_size << " bytes" << endl;

    while (_sent < _flow_size) {
        sendToRlbModule();
    }
}

void RlbSrc::sendToRlbModule() {
    RlbPacket* p = RlbPacket::newpkt(_top, _flow, _flow_src, _flow_dst, _sink, _mss, _pkts_sent);
    // ^^^ this sets the current source and destination (used for routing)
    // RLB module uses the "real source" and "real destination" to make decisions
    p->set_dummy(false);
    p->set_real_dst(_flow_dst); // set the "real" destination
    p->set_real_src(_flow_src); // set the "real" source
    p->set_ts(eventlist().now()); // time sent, not really needed...
    RlbModule* module = _top->get_rlb_module(_flow_src); // returns pointer to Rlb module
    module->receivePacket(*p, 0);
    _sent = _sent + _mss; // increment how many packets we've sent
    _pkts_sent++;

    // debug:
    //cout << "RlbSrc[" << _flow_src << "] has sent " << _pkts_sent << " packets" << endl;
    //cout << "Sent " << _sent << " bytes out of " << _flow_size << " bytes." << endl;
}

void RlbSrc::doNextEvent() {

    /*
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
    */

    // debug:
    //cout << "Starting flow at " << timeAsUs(eventlist().now()) << " us (current slice = " << slice << ")" << endl;

    startflow();
}


////////////////////////////////////////////////////////////////
//  RLB SINK
////////////////////////////////////////////////////////////////


RlbSink::RlbSink(DynExpTopology* top, EventList &eventlist, int flow_src, int flow_dst)
    : EventSource(eventlist,"rlbsnk"), _total_received(0), _flow_src(flow_src), _flow_dst(flow_dst), _top(top)
{
    _src = 0;
    _nodename = "rlbsink";
    _total_received = 0;
    _pkts_received = 0;
}

void RlbSink::doNextEvent() {
    // just a hack to get access to eventlist
}

void RlbSink::connect(RlbSrc& src)
{
    _src = &src;
}

// Receive a packet.
void RlbSink::receivePacket(Packet& pkt) {
    RlbPacket *p = (RlbPacket*)(&pkt);

    // debug:
    //if (p->seqno() == 1)
    //    cout << "v marked packet received" << endl;

    //simtime_picosec ts = p->ts();

    switch (pkt.type()) {
    case NDP:
    case NDPACK:
    case NDPNACK:
    case NDPPULL:
        cout << "RLB receiver received an NDP packet!" << endl;
        abort();
    case RLB:
        break;
    }

    // debug:
    _pkts_received++;
    //cout << " RlbSink[" << _flow_dst << "] has received " << _pkts_received << " packets" << endl;
    //cout << ">>>Sink: pkt# = " << _pkts_received << ", seqno = " << p->seqno() << " received." << endl;
    //cout << "   received at: " << timeAsMs(eventlist().now()) << " ms" << endl;

    int size = p->size()-HEADER;
    _total_received += size;

    p->free();

    if (_total_received >= _src->get_flowsize()) {

        // debug:
        //cout << ">>> Received everything from RLB flow [" << _src->get_flow_src() << "->" << _src->get_flow_dst() << "]" << endl;

        // FCT output for processing: (src dst bytes fct_ms timestarted_ms)

        cout << "FCT " << _src->get_flow_src() << " " << _src->get_flow_dst() << " " << _src->get_flowsize() <<
            " " << timeAsMs(eventlist().now() - _src->get_start_time()) << " " << fixed << timeAsMs(_src->get_start_time()) << endl;
    }
}
