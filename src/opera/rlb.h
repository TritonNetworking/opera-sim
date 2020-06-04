// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        


#ifndef RLB_H
#define RLB_H

/*
 * An RLB source and sink
 */

#include <list>
#include <map>
#include "config.h"
#include "network.h"
#include "rlbpacket.h"
#include "fairpullqueue.h"
#include "eventlist.h"


class RlbSink;

class Queue;

//class RlbSrc : public PacketSink, public EventSource {
class RlbSrc : public EventSource {
    friend class RlbSink;
 public:
    RlbSrc(DynExpTopology* top, NdpLogger* logger, TrafficLogger* pktlogger, EventList &eventlist, int flow_src, int flow_dst);
    //uint32_t get_id(){ return id;}
    virtual void connect(RlbSink& sink, simtime_picosec startTime);
    void startflow();
    void set_flowsize(uint64_t flow_size_in_bytes) {_flow_size = flow_size_in_bytes;}
    uint64_t get_flowsize() {return _flow_size;} // bytes
    inline void set_start_time(simtime_picosec startTime) {_start_time = startTime;}
    inline simtime_picosec get_start_time() {return _start_time;}

    inline int get_flow_src() {return _flow_src;}
    inline int get_flow_dst() {return _flow_dst;}

    virtual void doNextEvent();

    void sendToRlbModule();

    uint64_t _sent; // keep track of how many packets we've sent
    uint16_t _mss; // maximum segment size

    RlbSink* _sink;

    //virtual const string& nodename() { return _nodename; }
    //inline uint32_t flow_id() const { return _flow.flow_id();}

    //static int _global_node_count;
    //int _node_num;

    int _flow_src; // the sender (source) for this flow
    int _flow_dst; // the receiver (sink) for this flow

    int _pkts_sent; // number of packets sent

    DynExpTopology* _top;

 private:

    NdpLogger* _logger;
    TrafficLogger* _pktlogger;
    // Connectivity
    PacketFlow _flow;
    string _nodename;

    simtime_picosec _start_time;
    uint64_t _flow_size;  //The flow size in bytes.  Stop sending after this amount.
};

//class RlbSink : public PacketSink, public DataReceiver {
class RlbSink : public EventSource {
    friend class RlbSrc;
 public:
    RlbSink(DynExpTopology* top, EventList &eventlist, int flow_src, int flow_dst);
 

    //uint32_t get_id(){ return id;} // this is for logging...
    void receivePacket(Packet& pkt);
    
    uint64_t total_received() const { return _total_received;}

    virtual void doNextEvent(); // don't actually use this, but need it to get access to eventlist
    
    //virtual const string& nodename() { return _nodename; }
 
    RlbSrc* _src;

    int _flow_src; // the sender (source) for this flow
    int _flow_dst; // the receiver (sink) for this flow

    int _pkts_received; // number of packets received

    DynExpTopology* _top;

 private:

    void connect(RlbSrc& src);

    //inline uint32_t flow_id() const {
    //    return _src->flow_id();
    //};

    string _nodename;

    uint64_t _total_received;
};

#endif

