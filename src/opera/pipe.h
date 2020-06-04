// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        
#ifndef PIPE_H
#define PIPE_H

/*
 * A pipe is a dumb device which simply delays all incoming packets
 */

#include <list>
#include <utility>
#include "config.h"
#include "eventlist.h"
#include "network.h"
#include "loggertypes.h"


class Pipe : public EventSource, public PacketSink {
 public:

    Pipe(simtime_picosec delay, EventList& eventlist);
    void receivePacket(Packet& pkt); // inherited from PacketSink
    void doNextEvent(); // inherited from EventSource

    void sendFromPipe(Packet *pkt);

    uint64_t reportBytes(); // reports to the UtilMonitor
    uint64_t _bytes_delivered; // keep track of how many (non-hdr,ACK,NACK,PULL,RTX) packets were delivered to hosts

    simtime_picosec delay() { return _delay; }
    const string& nodename() { return _nodename; }
 private:
    simtime_picosec _delay;
    typedef pair<simtime_picosec,Packet*> pktrecord_t;
    list<pktrecord_t> _inflight; // the packets in flight (or being serialized)
    string _nodename;
};

class UtilMonitor : public EventSource {
 public:

    UtilMonitor(DynExpTopology* top, EventList &eventlist);

    void start(simtime_picosec period);
    void doNextEvent();
    void printAggUtil();

    DynExpTopology* _top;
    simtime_picosec _period; // picoseconds between utilization reports
    uint64_t _max_agg_Bps; // delivered to endhosts, across the whole network
    uint64_t _max_B_in_period;
    int _H; // number of hosts
    int _N; // number of racks
    int _hpr; // number of hosts per rack
};


#endif
