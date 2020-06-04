// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        
#include "pipe.h"
#include <iostream>
#include <sstream>
#include "ndppacket.h"

Pipe::Pipe(simtime_picosec delay, EventList& eventlist)
: EventSource(eventlist,"pipe"), _delay(delay)
{
    stringstream ss;
    ss << "pipe(" << delay/1000000 << "us)";
    _nodename= ss.str();

    _pipe_is_downlink = false; // added for util
    _B_delivered = 0; // added for util
}

void Pipe::receivePacket(Packet& pkt)
{
    pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_ARRIVE);
    if (_inflight.empty()){
	/* no packets currently inflight; need to notify the eventlist
	   we've an event pending */
	eventlist().sourceIsPendingRel(*this,_delay);
    }
    _inflight.push_front(make_pair(eventlist().now() + _delay, &pkt));
}


uint64_t Pipe::reportBytes() {
    uint64_t temp;
    temp = _B_delivered;
    _B_delivered = 0; // reset the counter
    return temp;
}


void Pipe::doNextEvent() {
    if (_inflight.size() == 0) 
	return;

    Packet *pkt = _inflight.back().second;
    _inflight.pop_back();
    pkt->flow().logTraffic(*pkt, *this,TrafficLogger::PKT_DEPART);


    // if this pipe is a ToR downlink to server
    if (_pipe_is_downlink) {
        // check if we need to count this packet:
        switch (pkt->type()) {
        case NDP:
        {
            //NdpPacket* ndp_pkt = dynamic_cast<NdpPacket*>(pkt);
            if (pkt->size() > 64) // it's not a header
                _B_delivered = _B_delivered + pkt->size(); // increment packet delivered
            break;
        }
        case TCP:
        {
            _B_delivered = _B_delivered + pkt->size(); // increment packet delivered
        }
        case TCPACK:
        case TCPNACK:
        case NDPACK:
        case NDPNACK:
        case NDPPULL:
        case ETH_PAUSE:
            break;
        }
    }

    // tell the packet to move itself on to the next hop
    pkt->sendOn();

    if (!_inflight.empty()) {
	// notify the eventlist we've another event pending
	simtime_picosec nexteventtime = _inflight.back().first;
	_eventlist.sourceIsPending(*this, nexteventtime);
    }
}
