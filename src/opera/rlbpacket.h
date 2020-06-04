// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-
#ifndef RLBPACKET_H
#define RLBPACKET_H

#include <list>
#include "network.h"

class RlbSink;
class RlbSrc;

// Subclass of Packet.
// Incorporate a packet database, to reuse packet objects that are no longer needed.
// Note: you never construct a new RlbPacket directly; 
// rather you use the static method newpkt() which knows to reuse old packets from the database.

#define HEADER 64

class RlbPacket : public Packet {
 public:

    inline static RlbPacket* newpkt(DynExpTopology* top, PacketFlow &flow, int src, int dst, RlbSink* rlbsink, int size, int seqno) {
	RlbPacket* p = _packetdb.allocPacket();
	p->set_attrs(flow, size+HEADER, seqno, src, dst); // set sequence number to zero, not used in RLB
	p->set_topology(top);
    p->_type = RLB;
    p->_seqno = seqno;
	p->_is_header = false;
	p->_bounced = false;
    p->_rlbsink = rlbsink;
	return p;
    }

    // this is an overloaded function for constructing dummy packets:
    inline static RlbPacket* newpkt(int size) {
        RlbPacket* p = _packetdb.allocPacket();
        p->set_size(size);
        p->_type = RLB;
        return p;
    }
  
    virtual inline void  strip_payload() {
	   Packet::strip_payload(); _size = HEADER;
        cout << "Sripping payload of RLB packet - shouldn't happen!" << endl;
    };
	
    void free() {_packetdb.freePacket(this);}
    virtual ~RlbPacket(){}
    inline int seqno() const {return _seqno;}
    inline simtime_picosec ts() const {return _ts;}
    inline void set_ts(simtime_picosec ts) {_ts = ts;}
    inline int32_t no_of_paths() const {return _no_of_paths;}

    virtual inline RlbSink* get_rlbsink(){return _rlbsink;}

 protected:

    RlbSink* _rlbsink;
    int _seqno;
    simtime_picosec _ts;
    int32_t _no_of_paths;  // how many paths are in the sender's
			    // list.  A real implementation would not
			    // send this in every packet, but this is
			    // simulation, and this is easiest to
			    // implement
    static PacketDB<RlbPacket> _packetdb;
    
};

#endif
