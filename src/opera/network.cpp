// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-    
#include "network.h"

#define DEFAULTDATASIZE 1500
int Packet::_data_packet_size = DEFAULTDATASIZE;
bool Packet::_packet_size_fixed = false;

// use set_attrs only when we want to do a late binding of the route -
// otherwise use set_route or set_rg
void Packet::set_attrs(PacketFlow& flow, int pkt_size, packetid_t id, int src, int dst){
    _flow = &flow;
    _size = pkt_size;
    _id = id;
    _src = src;
    _dst = dst;

    //_nexthop = 0;
    _is_header = 0;
    _flags = 0;
}

/* these were causing cyclic dependencies, moved them into NDPSrc/NDPSink, Queue, and Pipe classes

Queue* Packet::sendToNIC() {
    Queue* nic = _top->get_queue_serv_tor(_src); // returns pointer to nic queue
    nic->receivePacket(*this); // send this packet to the nic queue
    return nic; // return pointer so NDP source can update send time
}

void Packet::sendFromQueue() {
    Pipe* nextpipe; // the next packet sink will be a pipe
    if (_bounced) {
        // !!!
        cout << "_bounced not implemented" << endl;
        abort();
    }
    else {
        if (_crthop < 0) {
            // we're sending out of the NIC
            nextpipe = _top->get_pipe_serv_tor(_src);
            nextpipe->receivePacket(*this);
        } else {
            // we're sending out of a ToR queue
            int port = _top->get_port(_src, _dst, _slice_sent, _path_index, _crthop);
            if (_top->is_last_hop(port)) {
                set_lasthop(true);
                // if this port is not connected to _dst, then drop the packet
                if (!_top->port_dst_match(port, _crtToR, _dst)) {
                    free() // drop the packet
                    return;
                }
            }
            nextpipe = _top->get_pipe_tor(_crtToR, port);
            nextpipe->receivePacket(*this);
        }
    }
}

void Packet::sendFromPipe() {
    CompositeQueue* nextqueue; // the next packet sink will be a queue, NdpSink, or NdpSrc
    if (_bounced) {
        // !!!
        cout << "_bounced not implemented" << endl;
        abort();
    }
    else {
        if (_lasthop) {
            // we'll be delivering to an NdpSink or NdpSrc based on packet type
            switch (type()) {
            case NDP:
                nextqueue = _sink;
                break;
            case NDPACK:
            case NDPNACK:
            case NDPPULL:
                nextqueue = _src;
                break;
            }
        } else {
            // we'll be delivering to a ToR queue
            inc_crthop();
            int port = _top->get_port(_src, _dst, _slice_sent, _path_index, _crthop);
            if _crtToR < 0
                set_crtToR(_top->get_firstToR(_src));
            else
                set_crtToR(_top->get_nextToR(_slice_sent, _crtToR, port));
            nextqueue = _top->get_queue_tor(_crtToR, port);
        }
    }
    nextqueue->receivePacket(*this);
}
*/

// original version, no longer used with label switching
/*
PacketSink* Packet::sendOn() {
    PacketSink* nextsink;
    if (_bounced) {
        nextsink = _route->reverse()->at(_nexthop);
        _nexthop++;
    }
    else {
        nextsink = _route->at(_nexthop);
        _nexthop++;
    }
    nextsink->receivePacket(*this);
    return nextsink;
}
*/

// AKA, return to sender
void Packet::bounce() { 
    assert(!_bounced); 
    //assert(_route); // we only implement return-to-sender on regular routes
    _bounced = true; 
    _is_header = true;
    _been_bounced = true; // for debuggin only (as of 9/4/18)
    //_nexthop = _route->size() - _nexthop;
    //    _nexthop--;
    // we're now going to use the _route in reverse. The alternative
    // would be to modify the route, but all packets travelling the
    // same route share a single Route, and we won't want have to
    // allocate routes on a per packet basis.
}

void Packet::unbounce(uint16_t pktsize) { 
    assert(_bounced); 
    //assert(_route); // we only implement return-to-sender on regular
    // routes, not route graphs. If we go back to using
    // route graphs at some, we'll need to fix this, but
    // for now we're not using them.

    // clear the packet for retransmission
    _bounced = false; 
    _is_header = false;
    _size = pktsize;
    //_nexthop = 0;
}

void Packet::free() {
}

string Packet::str() const {
    string s;
    switch (_type) {
    case IP:
	s = "IP";
	break;
    case TCP:
	s = "TCP";
	break;
    case TCPACK:
	s = "TCPACK";
	break;
    case TCPNACK:
	s = "TCPNACK";
	break;
    case NDP:
	s = "NDP";
	break;
    case NDPACK:
	s = "NDPACK";
	break;
    case NDPNACK:
	s = "NDPNACK";
	break;
    case NDPPULL:
	s = "NDPPULL";
	break;
    case NDPLITE:
	s = "NDPLITE";
	break;
    case NDPLITEACK:
	s = "NDPLITEACK";
	break;
    case NDPLITERTS:
	s = "NDPLITERTS";
	break;
    case NDPLITEPULL:
	s = "NDPLITEPULL";
	break;
    case ETH_PAUSE:
	s = "ETHPAUSE";
	break;
    case RLB:
    s = "RLB";
    break;
    }
    return s;
}

uint32_t PacketFlow::_max_flow_id = 0;

PacketFlow::PacketFlow(TrafficLogger* logger)
    : Logged("PacketFlow"), _logger(logger)
{
    _flow_id = _max_flow_id++;
}

void PacketFlow::set_logger(TrafficLogger *logger) {
    _logger = logger;
}

void PacketFlow::logTraffic(Packet& pkt, Logged& location, TrafficLogger::TrafficEvent ev) {
    if (_logger)
	_logger->logTraffic(pkt, location, ev);
}

Logged::id_t Logged::LASTIDNUM = 1;
