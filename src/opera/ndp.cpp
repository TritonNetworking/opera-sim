// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-    
#include <math.h>
#include <iostream>
#include "ndp.h"
#include "queue.h"
#include <stdio.h>

////////////////////////////////////////////////////////////////
//  NDP SOURCE
////////////////////////////////////////////////////////////////

/* When you're debugging, sometimes it's useful to enable debugging on
   a single NDP receiver, rather than on all of them.  Set this to the
   node ID and recompile if you need this; otherwise leave it
   alone. */
//#define LOGSINK 2332
#define LOGSINK 0

/* We experimented with adding extra pulls to cope with scenarios
   where you've got a bad link and pulls get dropped.  Generally you
   don't want to do this though, so best leave RCV_CWND set to
   zero. Lost pulls are well handled by the cumulative pull number. */
//#define RCV_CWND 15
#define RCV_CWND 0

int NdpSrc::_global_node_count = 0;
/* _rtt_hist is used to build a histogram of RTTs.  The index is in
   units of microseconds, and RTT is from when a packet is first sent
   til when it is ACKed, including any retransmissions.  You can read
   this out after the sim has finished if you care about this. */
int NdpSrc::_rtt_hist[10000000] = {0};

/* keep track of RTOs.  Generally, we shouldn't see RTOs if
   return-to-sender is enabled.  Otherwise we'll see them with very
   large incasts. */
uint32_t NdpSrc::_global_rto_count = 0;

/* _min_rto can be tuned using SetMinRTO. Don't change it here.  */
simtime_picosec NdpSrc::_min_rto = timeFromUs((uint32_t)DEFAULT_RTO_MIN);

// You MUST set a route strategy.  The default is to abort without
// running - this is deliberate!
RouteStrategy NdpSrc::_route_strategy = NOT_SET;
RouteStrategy NdpSink::_route_strategy = NOT_SET;

NdpSrc::NdpSrc(DynExpTopology* top, NdpLogger* logger, TrafficLogger* pktlogger, EventList &eventlist, int flow_src, int flow_dst)
    : EventSource(eventlist,"ndp"),  _logger(logger), _flow(pktlogger), _flow_src(flow_src), _flow_dst(flow_dst), _top(top) {
    
    _mss = Packet::data_packet_size(); // maximum segment size (mss)

    _base_rtt = timeInf;
    _acked_packets = 0;
    _packets_sent = 0;
    _new_packets_sent = 0;
    _rtx_packets_sent = 0;
    _acks_received = 0;
    _nacks_received = 0;
    _pulls_received = 0;
    _implicit_pulls = 0;
    _bounces_received = 0;

    _flight_size = 0;

    _highest_sent = 0;
    _last_acked = 0;

    _sink = 0;

    _rtt = 0;
    _rto = timeFromMs(1); // was 20
    _cwnd = 15 * Packet::data_packet_size();
    _mdev = 0;
    _drops = 0;
    _flow_size = ((uint64_t)1)<<63;
    _last_pull = 0;
    _pull_window = 0;
  
    //_crt_path = 0; // used for SCATTER_PERMUTE route strategy

    _feedback_count = 0;
    for(int i = 0; i < HIST_LEN; i++)
        _feedback_history[i] = UNKNOWN;

    _rtx_timeout_pending = false;
    _rtx_timeout = timeInf;
    _node_num = _global_node_count++;
    _nodename = "ndpsrc" + to_string(_node_num);

    // debugging hack
    _log_me = false;
}

void NdpSrc::set_flowsize(uint64_t flow_size_in_bytes) {

    _flow_size = flow_size_in_bytes;
    if (_flow_size < _mss)
        _pkt_size = _flow_size;
    else
        _pkt_size = _mss;
}

void NdpSrc::set_traffic_logger(TrafficLogger* pktlogger) {
    _flow.set_logger(pktlogger);
}

void NdpSrc::log_me() {
    // avoid looping
    if (_log_me == true)
        return;
    cout << "Enabling logging on NdpSrc " << _nodename << endl;
    _log_me = true;
    if (_sink)
        _sink->log_me();
}

void NdpSrc::startflow() {
    _highest_sent = 0;
    _last_acked = 0;
    
    _acked_packets = 0;
    _packets_sent = 0;
    _rtx_timeout_pending = false;
    _rtx_timeout = timeInf;
    _pull_window = 0;
    
    _flight_size = 0;
    _first_window_count = 0;
    while (_flight_size < _cwnd && _flight_size < _flow_size) {
        send_packet(0);
        _first_window_count++;
    }
}

void NdpSrc::connect(NdpSink& sink, simtime_picosec starttime) {
    
    _sink = &sink;
    _flow.id = id; // identify the packet flow with the NDP source that generated it
    _flow._name = _name;
    _sink->connect(*this);

    set_start_time(starttime); // record the start time in _start_time
    eventlist().sourceIsPending(*this,starttime);
}

#define ABS(X) ((X)>0?(X):-(X))

/* Process a return-to-sender packet */
void NdpSrc::processRTS(NdpPacket& pkt){
    assert(pkt.bounced());
    //pkt.unbounce(ACKSIZE + _mss);
    pkt.unbounce(ACKSIZE + _pkt_size); // modified for < mss packets

    // need to reset the sounrce and destination:
    pkt.set_src(_flow_src);
    pkt.set_dst(_flow_dst);
    
    _sent_times.erase(pkt.seqno());
    //resend from front of RTX
    //queue on any other path than the one we tried last time
    pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_CREATE);
    _rtx_queue.push_front(&pkt);

    // debug:
    //cout << ">>> Node " << _flow_src << " received an RTS packet" << endl;

    //count_bounce(pkt.route()->path_id()); // don't need

    /* When we get a return-to-sender packet, we could immediately
       resend it, but this leads to a larger-than-necessary second
       incast at the receiver.  So generally the best strategy is to
       swallow the RTS packet.  There are two exceptions: 1.  It's the
       only packet left, so the receiver doesn't even know we're
       trying to send.  2.  The packet was sent on a known-bad path.
       In this cases we immediately resend.  Comment out the #define
       below if you want to always resend immediately. */
#define SWALLOW
#ifdef SWALLOW
    if (_pull_window == 0 && _first_window_count <= 1) {
	//Only immediately resend if we're not expecting any more
	//pulls.  Otherwise wait
	//for a pull.  Waiting reduces the effective window by one.

        // debug:
        //cout << "    bounce send" << endl;

	   send_packet(0);
	   if (_log_me) {
	       printf("    bounce send pw=%d\n", _pull_window);
	   } else {
	       //printf("bounce send\n");
	   }
    } else {
        // debug:
        //cout << "    bounce swallow" << endl;
	   if (_log_me) {
	       printf("    bounce swallow pw=%d\n", _pull_window);
	   } else {
	    //printf("bounce swallow\n");
	   }
    }
#else

    // debug:
    //cout << "    immediate send" << endl;

    send_packet(0);
    //printf("bounce send\n");
#endif
}

/* Process a NACK.  Generally this involves queuing the NACKed packet
   for retransmission, but then waiting for a PULL to actually resend
   it.  However, sometimes the NACK has the PULL bit set, and then we
   resend immediately */
void NdpSrc::processNack(const NdpNack& nack){
    NdpPacket* p;
    
    bool last_packet = (nack.ackno() + _pkt_size - 1) >= _flow_size;
    _sent_times.erase(nack.ackno());

    // debug:
    //if (get_flow_src()==84 && timeAsMs(eventlist().now())>=37 && timeAsMs(eventlist().now())<38) {
    //    cout << "   -> processing NACK" << endl;
    //    cout << "   -> lastpacket = " << last_packet << endl;
    //}
    // debug:
    //cout << "XXX Node " << _flow_src << " received a NACK" << endl;
    //cout << "    re-send" << endl;

    // Note: use `this` to add the Ndp_src (used for RTS)
    p = NdpPacket::newpkt(_top, _flow, _flow_src, _flow_dst, this, _sink, nack.ackno(),
        0, _pkt_size, true, last_packet);

    // debug:
    //if (p->been_bounced() == true)
    //    cout << "    * * * 'new' packet shows that it's been bounced" << endl;

    // need to add packet to rtx queue
    p->flow().logTraffic(*p,*this,TrafficLogger::PKT_CREATE);
    _rtx_queue.push_back(p);

    // "Fix"
    /*
    if (get_flow_src()==84 && timeAsMs(eventlist().now())>=37 && timeAsMs(eventlist().now())<38) {
        cout << "\n   -> Force-sending RTX" << endl;
        cout << "      (ackno() = " << nack.ackno() << ")" << endl;
        //pull_packets(nack.pullno(), nack.pacerno()); // this won't work because pullno & pacerno == 0
        send_packet(nack.pacerno());
        return;
    } */

    // General "Fix"
    if (_highest_sent >= _flow_size) {
        //cout << "\n-> Force-sending RTX" << endl;
        //cout << "   (ackno() = " << nack.ackno() << ")" << endl;
        send_packet(nack.pacerno());
        return;
    }

    if (nack.pull()) {

        // debug:
        //if (get_flow_src()==84 && timeAsMs(eventlist().now())>=37 && timeAsMs(eventlist().now())<38) {
        //    cout << "   -> NACK has implicit pull" << endl;
        //}

        _implicit_pulls++;
        pull_packets(nack.pullno(), nack.pacerno());
    }
}

/* Process an ACK.  Mostly just housekeeping, but if the ACK also has
   the PULL bit set, we also send a new packet immediately */
void NdpSrc::processAck(const NdpAck& ack) {
    NdpAck::seq_t ackno = ack.ackno();
    NdpAck::seq_t pacerno = ack.pacerno();
    NdpAck::seq_t pullno = ack.pullno();
    NdpAck::seq_t cum_ackno = ack.cumulative_ack();
    bool pull = ack.pull();
    if (pull) {
        if (_log_me)
            cout << "PULLACK\n";
        _pull_window--;
    }
    simtime_picosec ts = ack.ts();
    //int32_t path_id = ack.path_id();

    /*
      if (pull)
      printf("Receive ACK (pull): %s\n", ack.pull_bitmap().to_string().c_str());
      else
      printf("Receive ACK (----): %s\n", ack.pull_bitmap().to_string().c_str());
    */
    log_rtt(_first_sent_times[ackno]);
    _first_sent_times.erase(ackno);
    _sent_times.erase(ackno);

    //count_ack(path_id); // don't need
  
    // Compute rtt.  This comes originally from TCP, and may not be optimal for NDP */
    uint64_t m = eventlist().now()-ts;

    if (m!=0){
	if (_rtt>0){
	    uint64_t abs;
	    if (m>_rtt)
		abs = m - _rtt;
	    else
		abs = _rtt - m;

	    _mdev = 3 * _mdev / 4 + abs/4;
	    _rtt = 7*_rtt/8 + m/8;

	    _rto = _rtt + 4*_mdev;
	} else {
	    _rtt = m;
	    _mdev = m/2;

	    _rto = _rtt + 4*_mdev;
	}
	if (_base_rtt==timeInf || _base_rtt > m)
	    _base_rtt = m;
    }

    if (_rto < _min_rto)
	_rto = _min_rto * ((drand() * 0.5) + 0.75);

    if (cum_ackno > _last_acked) { // a brand new ack    
        // we should probably cancel the rtx timer for any acked by
        // the cumulative ack, but we'll get an ACK or NACK anyway in
        // due course.
        _last_acked = cum_ackno;
    }

    if (_logger)
        _logger->logNdp(*this, NdpLogger::NDP_RCV);

    _flight_size -= _pkt_size;
    assert(_flight_size>=0);

    // debug:
    //if (get_flow_src()==84 && timeAsMs(eventlist().now())>=37 && timeAsMs(eventlist().now())<38) {
    //    cout << "  cum_ackno =  " << cum_ackno << ", _flow_size =  " << _flow_size << endl;
    //}

    if (cum_ackno >= _flow_size){
	   //cout << "Flow " << nodename() << " finished at " << timeAsMs(eventlist().now()) << endl;
        //cout << "flowID " << get_id() << " finished_ms " << timeAsMs(eventlist().now()) << endl;
        //cout << "flowID " << get_id() << " bytes " << get_flowsize() << " started_ms " << timeAsMs(get_start_time()) << " finished_ms " << timeAsMs(eventlist().now()) << endl;
        
        //cout << "flowID " << get_id() << " bytes " << get_flowsize() <<
        //    " FCT_ms " << timeAsMs(eventlist().now() - get_start_time()) <<
        //    "    current_time = " << timeAsMs(eventlist().now()) << endl;

        // FCT output for processing: (src dst bytes fct_ms timestarted_ms)
        
        cout << "FCT " << get_flow_src() << " " << get_flow_dst() << " " << get_flowsize() <<
            " " << timeAsMs(eventlist().now() - get_start_time()) << " " << fixed << timeAsMs(get_start_time()) << endl;

        // debug a certain connection:
        //if ( get_flow_src() == 84 && get_flow_dst() == 0) {
        //cout << "FCT " << get_flow_src() << " " << get_flow_dst() << " " << get_flowsize() <<
        //    " " << timeAsMs(eventlist().now() - get_start_time()) << " " << get_id() << endl;
        //}
    }

    update_rtx_time();

    /* if the PULL bit is set, send some new data packets */
    if (pull) {
        _implicit_pulls++;
        pull_packets(pullno, pacerno);
    }
}

void NdpSrc::receivePacket(Packet& pkt) 
{
    pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_RCVDESTROY);

    switch (pkt.type()) {
    case NDP:
	{
	    _bounces_received++;
	    _first_window_count--;
	    processRTS((NdpPacket&)pkt);
	    return;
	}
    case NDPNACK: 
	{
        // debug:
        //if (get_flow_src()==84 && timeAsMs(eventlist().now())>=37 && timeAsMs(eventlist().now())<38) {
        //    cout << "Sender 84 received a NACK at " << timeAsUs(eventlist().now()) << " us." << endl;
        //}

	    _nacks_received++;
	    _pull_window++;
	    _first_window_count--;
	    /*if (_log_me) {
		printf("NACK, pw=%d\n", _pull_window);
	    } else {
		printf("NACK\n");
	    }*/
	    processNack((const NdpNack&)pkt);
	    pkt.free();
	    return;
	} 
    case NDPPULL: 
	{

        // debug:
        //if (get_flow_src()==84 && timeAsMs(eventlist().now())>=37 && timeAsMs(eventlist().now())<38) {
        //    cout << "Sender 84 received a PULL at " << timeAsUs(eventlist().now()) << " us." << endl;
        //}

	    _pulls_received++; // these don't matter...
	    _pull_window--;

	    //if (_log_me) {
        //    printf("PULL, pw=%d\n", _pull_window);
	    //}
	    NdpPull *p = (NdpPull*)(&pkt);
	    NdpPull::seq_t cum_ackno = p->cumulative_ack();
	    if (cum_ackno > _last_acked) { // a brand new ack    
            // we should probably cancel the rtx timer for any acked by
            // the cumulative ack, but we'll get an ACK or NACK anyway in
            // due course.
            _last_acked = cum_ackno;
	  
	    }
	    //printf("Receive PULL: %s\n", p->pull_bitmap().to_string().c_str());

	    pull_packets(p->pullno(), p->pacerno());

	    return;
	}
    case NDPACK:
	{
        // debug:
        //if (get_flow_src()==84 && timeAsMs(eventlist().now())>=37 && timeAsMs(eventlist().now())<38) {
        //    cout << "Sender 84 received an ACK at " << timeAsUs(eventlist().now()) << " us." << endl;
        //}

	    _acks_received++;
	    _pull_window++;
	    _first_window_count--;
	    //	    if (_log_me) {
	    //	printf("ACK, pw=%d\n", _pull_window);
	    //}
	    processAck((const NdpAck&)pkt);
	    pkt.free();
	    return;
	}
    }
}

void NdpSrc::pull_packets(NdpPull::seq_t pull_no, NdpPull::seq_t pacer_no) {
    // Pull number is cumulative both to allow for lost pulls and to
    // reduce reverse-path RTT - if one pull is delayed on one path, a
    // pull that gets there faster on another path can supercede it

    // debug:
    //if (get_flow_src()==84 && timeAsMs(eventlist().now())>=37 && timeAsMs(eventlist().now())<38) {
    //    cout << "   pull_packets: pull_no = " << pull_no << ", pacer_no = " << pacer_no << endl;
    //}

    while (_last_pull < pull_no) {

        // debug:
        //if (get_flow_src()==84 && timeAsMs(eventlist().now())>=37 && timeAsMs(eventlist().now())<38) {
        //    cout << "      while loop: _last_pull = " << _last_pull << ", pull_no = " << pull_no << endl;
        //}

        send_packet(pacer_no);
        _last_pull++;
    }
}

Queue* NdpSrc::sendToNIC(NdpPacket* pkt) {
    DynExpTopology* top = pkt->get_topology();
    Queue* nic = top->get_queue_serv_tor(pkt->get_src()); // returns pointer to nic queue
    nic->receivePacket(*pkt); // send this packet to the nic queue
    return nic; // return pointer so NDP source can update send time
}

// Note: the data sequence number is the number of Byte1 of the packet, not the last byte.
void NdpSrc::send_packet(NdpPull::seq_t pacer_no) {
    NdpPacket* p;
    if (!_rtx_queue.empty()) {

        // debug:
        //if (get_flow_src()==84 && timeAsMs(eventlist().now())>=37 && timeAsMs(eventlist().now())<38) {
        //    cout << "*** Sending RTX packet ***" << endl;
        //}

        // There are packets in the RTX queue for us to send
        p = _rtx_queue.front();

        // debug:
        //if (p->been_bounced() == true)
        //    cout << "    Node " << _flow_src << ", RTXing previously bounced RTX packet" << endl;
        //else
        //    cout << "    Node " << _flow_src << ", RTXing a 'new' packet" << endl;
        //cout << "    (RTX queue size = " << _rtx_queue.size() << ")" << endl;

        _rtx_queue.pop_front();
        p->flow().logTraffic(*p,*this,TrafficLogger::PKT_SEND);
        p->set_ts(eventlist().now()); // set transmission time
        p->set_pacerno(pacer_no);

        //PacketSink* sink = p->sendOn();
        //PacketSink* sink = p->sendToNIC();
        Queue* sink = sendToNIC(p);
        PriorityQueue *q = dynamic_cast<PriorityQueue*>(sink);
        assert(q);
        //Figure out how long before the feeder queue sends this
        //packet, and add it to the sent time. Packets can spend quite
        //a bit of time in the feeder queue.  It would be better to
        //have the feeder queue update the sent time, because the
        //feeder queue isn't a FIFO but that would be hard to
        //implement in a real system, so this is a rough proxy.
        uint32_t service_time = q->serviceTime(*p);  
        _sent_times[p->seqno()] = eventlist().now() + service_time;
        _packets_sent ++;
        _rtx_packets_sent++;
        update_rtx_time();
        if (_rtx_timeout == timeInf) {
            _rtx_timeout = eventlist().now() + _rto;
        }

    } else {
        // there are no packets in the RTX queue, so we'll send a new one
        bool last_packet = false;
        if (_flow_size) {
            if (_highest_sent >= _flow_size) {

                // debug:
                //if (get_flow_src()==84 && timeAsMs(eventlist().now())>=37 && timeAsMs(eventlist().now())<38) {
                //    cout << "         No more packets to send..." << endl;
                //}

                /* we've sent enough new data. */
                /* xxx should really make the last packet sent be the right size
                * if _flow_size is not a multiple of _mss */
                return;
            }
            if (_highest_sent + _pkt_size >= _flow_size) {
                last_packet = true;
            }
        }

        p = NdpPacket::newpkt(_top, _flow, _flow_src, _flow_dst, this, _sink, _highest_sent+1,
            pacer_no, _pkt_size, false, last_packet);


        p->flow().logTraffic(*p,*this,TrafficLogger::PKT_CREATESEND);
        p->set_ts(eventlist().now());
    
        _flight_size += _pkt_size;
        _highest_sent += _pkt_size;  //XX beware wrapping
        _packets_sent++;
        _new_packets_sent++;

        //PacketSink* sink = p->sendOn();
        //PacketSink* sink = p->sendToNIC();
        Queue* sink = sendToNIC(p);
        PriorityQueue *q = dynamic_cast<PriorityQueue*>(sink);
        assert(q);
        //Figure out how long before the feeder queue sends this
        //packet, and add it to the sent time. Packets can spend quite
        //a bit of time in the feeder queue.  It would be better to
        //have the feeder queue update the sent time, because the
        //feeder queue isn't a FIFO but that would be hard to
        //implement in a real system, so this is a rough proxy.
        uint32_t service_time = q->serviceTime(*p);  
        //cout << "service_time2: " << service_time << endl;
        _sent_times[p->seqno()] = eventlist().now() + service_time;
        _first_sent_times[p->seqno()] = eventlist().now();

        if (_rtx_timeout == timeInf) {
            _rtx_timeout = eventlist().now() + _rto;
        }
    }
}

void NdpSrc::update_rtx_time() {
    //simtime_picosec now = eventlist().now();
    if (_sent_times.empty()) {
        _rtx_timeout = timeInf;
        return;
    }
    map<NdpPacket::seq_t, simtime_picosec>::iterator i;
    simtime_picosec first_senttime = timeInf;
    int c = 0;
    for (i = _sent_times.begin(); i != _sent_times.end(); i++) {
        simtime_picosec sent = i->second;
        if (sent < first_senttime || first_senttime == timeInf) {
            first_senttime = sent;
        }
        c++;
    }
    _rtx_timeout = first_senttime + _rto;
}
 
void NdpSrc::process_cumulative_ack(NdpPacket::seq_t cum_ackno) {
    map<NdpPacket::seq_t, simtime_picosec>::iterator i, i_next;
    i = _sent_times.begin();
    while (i != _sent_times.end()) {
        if (i->first <= cum_ackno) {
            i_next = i; //juggling to keep i valid
            i_next++;
            _sent_times.erase(i);
            i = i_next;
        } else {
            return;
        }
    }
    //need to call update_rtx_time right after this!
}

void NdpSrc::retransmit_packet() {
    //cout << "starting retransmit_packet\n";
    NdpPacket* p;
    map<NdpPacket::seq_t, simtime_picosec>::iterator i, i_next;
    i = _sent_times.begin();
    list <NdpPacket::seq_t> rtx_list;
    // we build a list first because otherwise we're adding to and
    // removing from _sent_times and the iterator gets confused
    while (i != _sent_times.end()) {
        if (i->second + _rto <= eventlist().now()) {
            //cout << "_sent_time: " << timeAsUs(i->second) << "us rto " << timeAsUs(_rto) << "us now " << timeAsUs(eventlist().now()) << "us\n";
            //this one is due for retransmission
            rtx_list.push_back(i->first);
            i_next = i; //we're about to invalidate i when we call erase
            i_next++;
            _sent_times.erase(i);
            i = i_next;
        } else {
            i++;
        }
    }
    list <NdpPacket::seq_t>::iterator j;
    for (j = rtx_list.begin(); j != rtx_list.end(); j++) {
        NdpPacket::seq_t seqno = *j;
        bool last_packet = (seqno + _pkt_size - 1) >= _flow_size;

        p = NdpPacket::newpkt(_top, _flow, _flow_src, _flow_dst, this, _sink, seqno,
            0, _pkt_size, true, last_packet);
	
        p->flow().logTraffic(*p,*this,TrafficLogger::PKT_CREATESEND);
        p->set_ts(eventlist().now());
        _global_rto_count++;
        cout << "Total RTOs: " << _global_rto_count << endl;
        //p->sendOn();
        //p->sendToNIC();
        sendToNIC(p);
        _packets_sent++;
        _rtx_packets_sent++;
    }
    update_rtx_time();
}

void NdpSrc::rtx_timer_hook(simtime_picosec now, simtime_picosec period) {
#ifndef RESEND_ON_TIMEOUT
    return;  // if we're using RTS, we shouldn't need to also use
	     // timeouts, at least in simulation where we don't see
	     // corrupted packets
#endif

    if (_highest_sent == 0) return;
    if (_rtx_timeout==timeInf || now + period < _rtx_timeout) return;

    // this should never happen in Opera...
    cout <<"At " << timeAsUs(now) << "us RTO " << timeAsUs(_rto) << "us MDEV " << timeAsUs(_mdev) << "us RTT "<< timeAsUs(_rtt) << "us SEQ " << _last_acked / _mss << " CWND "<< _cwnd/_mss << " Flow ID " << str()  << endl;
    /*
    if (_log_me) {
	cout << "Flow " << LOGSINK << "scheduled for RTX\n";
    }
    */

    // here we can run into phase effects because the timer is checked
    // only periodically for ALL flows but if we keep the difference
    // between scanning time and real timeout time when restarting the
    // flows we should minimize them !
    if(!_rtx_timeout_pending) {
	_rtx_timeout_pending = true;

	
	// check the timer difference between the event and the real value
	simtime_picosec too_early = _rtx_timeout - now;
	if (now > _rtx_timeout) {
	    // this shouldn't happen
	    cout << "late_rtx_timeout: " << _rtx_timeout << " now: " << now << " now+rto: " << now + _rto << " rto: " << _rto << endl;
	    too_early = 0;
	}
	eventlist().sourceIsPendingRel(*this, too_early);
    }
}

void NdpSrc::log_rtt(simtime_picosec sent_time) {
    int64_t rtt = eventlist().now() - sent_time;
    if (rtt >= 0) 
	_rtt_hist[(int)timeAsUs(rtt)]++;
    else
	cout << "Negative RTT: " << rtt << endl;
}

void NdpSrc::doNextEvent() {
    if (_rtx_timeout_pending) {
        _rtx_timeout_pending = false;
        if (_logger)
            _logger->logNdp(*this, NdpLogger::NDP_TIMEOUT);
        retransmit_packet();
    } else {

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
        //if ( get_flow_src() == 84 && get_flow_dst() == 0) {
        //cout << "FST " << get_flow_src() << " " << get_flow_dst() << " " << get_flowsize() <<
        //    " " << timeAsMs(eventlist().now()) << " " << get_id() << endl;
        //}

        //cout << "Starting flowID " << get_id() << " at " << timeAsUs(eventlist().now()) <<
        //    " us (current slice = " << slice << ")" << endl;

        startflow();
    }
}



////////////////////////////////////////////////////////////////
//  NDP SINK
////////////////////////////////////////////////////////////////


/* Only use this constructor when there is only one flow to this receiver */
/*
NdpSink::NdpSink(EventList& event, double pull_rate_modifier)
    : Logged("ndp_sink"),_cumulative_ack(0) , _total_received(0) 
{
    _src = 0;
    _pacer = new NdpPullPacer(event, pull_rate_modifier);
    //_pacer = new NdpPullPacer(event, "/Users/localadmin/poli/new-datacenter-protocol/data/1500.recv.cdf.pretty");
    
    _nodename = "ndpsink";
    _pull_no = 0;
    _last_packet_seqno = 0;
    _log_me = false;
    _total_received = 0;
    _path_hist_index = -1;
    _path_hist_first = -1;
#ifdef RECORD_PATH_LENS
    _path_lens.resize(MAX_PATH_LEN+1);
    _trimmed_path_lens.resize(MAX_PATH_LEN+1);
    for (int i = 0; i < MAX_PATH_LEN+1; i++) {
	_path_lens[i]=0;
	_trimmed_path_lens[i]=0;
    }
#endif
}
*/

/* Use this constructor when there are multiple flows to one receiver
   - all the flows to one receiver need to share the same NdpPullPacer */
NdpSink::NdpSink(DynExpTopology* top, NdpPullPacer* pacer, int flow_src, int flow_dst)
    : Logged("ndp_sink"),_cumulative_ack(0) , _total_received(0), _flow_src(flow_src), _flow_dst(flow_dst), _top(top)
{
    _src = 0;
    _pacer = pacer;
    _nodename = "ndpsink";
    _pull_no = 0;
    _last_packet_seqno = 0;
    _log_me = false;
    _total_received = 0;
}

void NdpSink::log_me() {
    // avoid looping
    if (_log_me == true)
        return;

    _log_me = true;
    if (_src)
        _src->log_me();
    _pacer->log_me();
    
}

/* Connect a src to this sink.  We normally won't use this route if
   we're sending across multiple paths - call set_paths() after
   connect to configure the set of paths to be used. */
/*void NdpSink::connect(NdpSrc& src, Route& route)
{
    _src = &src;
    switch (_route_strategy) {
    case SINGLE_PATH:
        abort();
        //_route = &route;
	break;
    default:
	// do nothing we shouldn't be using this route - call
	// set_paths() to set routing information
	_route = NULL;
	break;
    }
	
    _cumulative_ack = 0;
    _drops = 0;

    // debugging hack
    if (get_id() == LOGSINK) {
	cout << "Found sink for " << LOGSINK << "\n";
	_log_me = true;
	_pacer->log_me();
    }
}
*/

void NdpSink::connect(NdpSrc& src)
{
    _src = &src;
    
    _cumulative_ack = 0;
    _drops = 0;

    // debugging hack
    if (get_id() == LOGSINK) {
        cout << "Found sink for " << LOGSINK << "\n";
        _log_me = true;
        _pacer->log_me();
    }
}

// Receive a packet.
// Note: _cumulative_ack is the last byte we've ACKed.
// seqno is the first byte of the new packet.
void NdpSink::receivePacket(Packet& pkt) {
    NdpPacket *p = (NdpPacket*)(&pkt);
    NdpPacket::seq_t seqno = p->seqno();
    NdpPacket::seq_t pacer_no = p->pacerno();
    simtime_picosec ts = p->ts();
    bool last_packet = ((NdpPacket*)&pkt)->last_packet();
    switch (pkt.type()) {
    case NDP:

        // debug
        //if (_flow_dst==0) {
        //   cout << "Receiver 0 received a packet" << endl;
        //}

        break;
    case NDPACK:
    case NDPNACK:
    case NDPPULL:
        // Is there anything we should do here?  Generally won't happen unless the topolgy is very asymmetric.
        assert(pkt.bounced());
        cout << "Got bounced feedback packet!\n";
        p->free(); // delete packet
        return;
    }
	
    //update_path_history(*p); // don't need this anymore...

    if (pkt.header_only()){

        // debug:
        //if (_flow_dst==0) {
        //    cout << "It's a header..." << endl;
        //    cout << " Sending NACK" << endl;
        //}

        send_nack(ts,((NdpPacket*)&pkt)->seqno(), pacer_no);	  
        pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_RCVDESTROY);
        p->free();
        return;
    }

    int size = p->size()-ACKSIZE; // TODO: the following code assumes all packets are the same size

    if (last_packet) {
        // we've seen the last packet of this flow, but may not have
        // seen all the preceding packets
        _last_packet_seqno = p->seqno() + size - 1;
    }

    pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_RCVDESTROY);
    p->free();

    _total_received+=size;

    // debug
    //if (_flow_dst==0) {
    //    cout << "It's a full packet..." << endl;
    //    cout << " Sending ACK" << endl;
    //    cout << "  seqno = " << seqno << endl;
    //}

    if (seqno == _cumulative_ack+1) { // it's the next expected seq no
        _cumulative_ack = seqno + size - 1;
        // are there any additional received packets we can now ack?
        while (!_received.empty() && (_received.front() == _cumulative_ack+1) ) {
            _received.pop_front();
            _cumulative_ack+= size;
        }
    } else if (seqno < _cumulative_ack+1) {
        //must have been a bad retransmit
    } else { // it's not the next expected sequence number
        if (_received.empty()) {
            _received.push_front(seqno);
            //it's a drop in this simulator there are no reorderings.
            _drops += (size + seqno-_cumulative_ack-1)/size;
        } else if (seqno > _received.back()) { // likely case
            _received.push_back(seqno);
        } 
        else { // uncommon case - it fills a hole
            list<uint64_t>::iterator i;
            for (i = _received.begin(); i != _received.end(); i++) {
                if (seqno == *i) break; // it's a bad retransmit
                if (seqno < (*i)) {
                    _received.insert(i, seqno);
                    break;
                }
            }
        }
    }

    send_ack(ts, seqno, pacer_no);
    // have we seen everything yet?
    if (_last_packet_seqno > 0 && _cumulative_ack == _last_packet_seqno) {
        _pacer->release_pulls(flow_id());
    }
}

/* _path_history was an experiment with allowing the receiver to tell
   the sender which path to use for the next data packet.  It's no
   longer used for that, but might still be useful for debugging */
/*
void NdpSink::update_path_history(const NdpPacket& p) {
    assert(p.path_id() >= 0 && p.path_id() < 10000);
    if (_path_hist_index == -1) {
	//first received packet.
	_no_of_paths = p.no_of_paths();
	assert(_no_of_paths <= PULL_MAXPATHS); //ensure we've space in the pull bitfield
	_path_history.reserve(_no_of_paths * HISTORY_PER_PATH);
	_path_hist_index = 0;
	_path_hist_first = 0;
	_path_history[_path_hist_index] = ReceiptEvent(p.path_id(), p.header_only());
    } else {
	assert(_no_of_paths == p.no_of_paths());
	_path_hist_index = (_path_hist_index + 1) % _no_of_paths * HISTORY_PER_PATH;
	if (_path_hist_first == _path_hist_index) {
	    _path_hist_first = (_path_hist_first + 1) % _no_of_paths * HISTORY_PER_PATH;
	}
	_path_history[_path_hist_index] = ReceiptEvent(p.path_id(), p.header_only());
    }
}
*/


void NdpSink::send_ack(simtime_picosec ts, NdpPacket::seq_t ackno, NdpPacket::seq_t pacer_no) {
    NdpAck *ack;
    _pull_no++;

    // note: the sender of the ACK is the `_flow_dst`
    ack = NdpAck::newpkt(_top, _src->_flow, _flow_dst, _flow_src, _src, 0,
        ackno, _cumulative_ack, _pull_no);

    ack->flow().logTraffic(*ack,*this,TrafficLogger::PKT_CREATE);
    ack->set_ts(ts);

    _pacer->sendPacket(ack, pacer_no, this);
}

void NdpSink::send_nack(simtime_picosec ts, NdpPacket::seq_t ackno, NdpPacket::seq_t pacer_no) {
    NdpNack *nack;
    _pull_no++;

    // note: the sender of the NACK is the `_flow_dst`
    nack = NdpNack::newpkt(_top, _src->_flow, _flow_dst, _flow_src, _src, 0,
        ackno, _cumulative_ack, _pull_no);

    nack->flow().logTraffic(*nack,*this,TrafficLogger::PKT_CREATE);
    nack->set_ts(ts);
    _pacer->sendPacket(nack, pacer_no, this);
}


double* NdpPullPacer::_pull_spacing_cdf = NULL;
int NdpPullPacer::_pull_spacing_cdf_count = 0;


/* Every NdpSink needs an NdpPullPacer to pace out its PULL packets.
   Multiple incoming flows at the same receiving node must share a
   single pacer */
NdpPullPacer::NdpPullPacer(EventList& event, double pull_rate_modifier)  : 
    EventSource(event, "ndp_pacer"), _last_pull(0)
{
    _packet_drain_time = (simtime_picosec)(Packet::data_packet_size() * (pow(10.0,12.0) * 8) / speedFromMbps((uint64_t)10000))/pull_rate_modifier;
    _log_me = false;
    _pacer_no = 0;
}

NdpPullPacer::NdpPullPacer(EventList& event, char* filename)  : 
    EventSource(event, "ndp_pacer"), _last_pull(0)
{
    int t;
    _packet_drain_time = 0;

    if (!_pull_spacing_cdf){
	FILE* f = fopen(filename,"r");
	fscanf(f,"%d\n",&_pull_spacing_cdf_count);
	cout << "Generating pull spacing from CDF; reading " << _pull_spacing_cdf_count << " entries from CDF file " << filename << endl;
	_pull_spacing_cdf = new double[_pull_spacing_cdf_count];

	for (int i=0;i<_pull_spacing_cdf_count;i++){
	    fscanf(f,"%d %lf\n",&t,&_pull_spacing_cdf[i]);
	    //assert(t==i);
	    //cout << " Pos " << i << " " << _pull_spacing_cdf[i]<<endl;
	}
    }
    
    _log_me = false;
    _pacer_no = 0;
}

void NdpPullPacer::log_me() {
    // avoid looping
    if (_log_me == true)
	return;

    _log_me = true;
    _total_excess = 0;
    _excess_count = 0;
}

void NdpPullPacer::set_pacerno(Packet *pkt, NdpPull::seq_t pacer_no) {
    if (pkt->type() == NDPACK) {
	((NdpAck*)pkt)->set_pacerno(pacer_no);
    } else if (pkt->type() == NDPNACK) {
	((NdpNack*)pkt)->set_pacerno(pacer_no);
    } else if (pkt->type() == NDPPULL) {
	((NdpPull*)pkt)->set_pacerno(pacer_no);
    } else {
	abort();
    }
}

void NdpPullPacer::sendToNIC(Packet* pkt) {
    DynExpTopology* top = pkt->get_topology();
    Queue* nic = top->get_queue_serv_tor(pkt->get_src()); // returns pointer to nic queue
    nic->receivePacket(*pkt); // send this packet to the nic queue
}

void NdpPullPacer::sendPacket(Packet* ack, NdpPacket::seq_t rcvd_pacer_no, NdpSink* receiver) {
    /*
    if (_log_me) {
	cout << "pacerno diff: " << _pacer_no - rcvd_pacer_no << endl;
    }
    */

    if (rcvd_pacer_no != 0 && _pacer_no - rcvd_pacer_no < RCV_CWND) {
        // we need to increase the number of packets in flight from this flow
        if (_log_me)
            cout << "increase_window\n";
        receiver->increase_window();
    }

    simtime_picosec drain_time;

    if (_packet_drain_time>0)
        drain_time = _packet_drain_time;
    else {
        int t = (int)(drand()*_pull_spacing_cdf_count);
        drain_time = 10*timeFromNs(_pull_spacing_cdf[t])/20;
        //cout << "Drain time is " << timeAsUs(drain_time);
    }
	    

    if (_pull_queue.empty()){
        simtime_picosec delta = eventlist().now()-_last_pull;
    
        if (delta >= drain_time){

            // debug:
            //if (ack->get_src()==0 && timeAsMs(eventlist().now())>=37 && timeAsMs(eventlist().now())<38) {
            //    cout << "      -> sending as is at " << timeAsUs(eventlist().now()) << " us." << endl;
            //}

            //send out as long as last NACK/ACK was sent more than packetDrain time ago.
            ack->flow().logTraffic(*ack,*this,TrafficLogger::PKT_SEND);
            if (_log_me) {
                double excess = (delta - drain_time)/(double)drain_time;
                _total_excess += excess;
                _excess_count++;
            }
            set_pacerno(ack, _pacer_no++);
            //ack->sendOn();
            //ack->sendToNIC();
            sendToNIC(ack);
            _last_pull = eventlist().now();
            return;
        } else {
            eventlist().sourceIsPendingRel(*this,drain_time - delta);
        }
    }

    /*
    if (_log_me) {
        _excess_count++;
        cout << "Mean excess: " << _total_excess / _excess_count << endl;
        if (ack->type() == NDPACK) {
            cout << "Ack " <<  (((NdpAck*)ack)->ackno()-1)/9000 << " (queue)\n";
        } else if (ack->type() == NDPNACK) {
            cout << "Nack " << (((NdpNack*)ack)->ackno()-1)/9000 << " (queue)\n";
        } else {
            cout << "WTF\n";
        }
    }
    */

    // debug:
    //if (ack->get_src()==0 && timeAsMs(eventlist().now())>=37 && timeAsMs(eventlist().now())<38) {
    //    cout << " *creating pull packet\n Sending NACK at " << timeAsUs(eventlist().now()) << " us." << endl;
    //}

    // Create a pull packet and stick it in the queue.
    // Send the ack/nack, but with pull cleared.
    NdpPull *pull_pkt = NULL;
    if (ack->type() == NDPACK) {
        // note: this will inherit the correct sender from the ACK
        pull_pkt = NdpPull::newpkt((NdpAck*)ack);
        ((NdpAck*)ack)->dont_pull();
    } else if (ack->type() == NDPNACK) {
        // note: this will inherit the correct sender from the NACK
        pull_pkt = NdpPull::newpkt((NdpNack*)ack);
        ((NdpNack*)ack)->dont_pull();
    }
    pull_pkt->flow().logTraffic(*pull_pkt,*this,TrafficLogger::PKT_CREATE);
    _pull_queue.enqueue(*pull_pkt);
    ack->flow().logTraffic(*ack,*this,TrafficLogger::PKT_SEND);
    //ack->sendOn();
    //ack->sendToNIC();
    sendToNIC(ack);
    
    //   if (_log_me) {
    //       list <Packet*>::iterator i = _waiting_pulls.begin();
    //       cout << "Queue: ";
    //       while (i != _waiting_pulls.end()) {
    // 	  Packet* p = *i;
    // 	  if (p->type() == NDPNACK) {
    // 	      cout << "Nack(" << ((NdpNack*)p)->ackno() << ") ";
    // 	  } else if (p->type() == NDPACK) {
    // 	      cout << "Ack(" << ((NdpAck*)p)->ackno() << ") ";
    // 	  } 
    // 	  i++;
    //       }
    //       cout << endl;
    //   }
    //cout << "Qsize = " << _waiting_pulls.size() << endl;
}

// when we're reached the last packet of a connection, we can release
// all the queued acks for that connection because we know they won't
// generate any more data packets.  This will move the nacks up the
// queue too, causing any retransmitted packets from the tail of the
// file to be received earlier
void NdpPullPacer::release_pulls(uint32_t flow_id) {
    _pull_queue.flush_flow(flow_id);
}

void NdpPullPacer::doNextEvent(){
    if (_pull_queue.empty()) {
        // this can happen if we released all the acks at the end of
        // the connection.  we didn't cancel the timer, so we end up
        // here.
        return;
    }

    Packet *pkt = _pull_queue.dequeue();

    //   cout << "Sending NACK for packet " << nack->ackno() << endl;
    pkt->flow().logTraffic(*pkt,*this,TrafficLogger::PKT_SEND);
    if (pkt->flow().log_me()) {
        if (pkt->type() == NDPACK) {
            abort(); //we now only pace pulls
        } else if (pkt->type() == NDPNACK) {
            abort(); //we now only pace pulls
        } if (pkt->type() == NDPPULL) {
            //cout << "Pull (queued) " << ((NdpNack*)pkt)->ackno() << "\n";
        } else {
            abort(); //we now only pace pulls
        }
    }
    set_pacerno(pkt, _pacer_no++);
    
    //pkt->sendOn();
    //pkt->sendToNIC();

    // debug:
    //if (pkt->get_src()==0 && timeAsMs(eventlist().now())>=37 && timeAsMs(eventlist().now())<38) {
    //    cout << " Sending PULL at " << timeAsUs(eventlist().now()) << " us." << endl;
    //}

    sendToNIC(pkt);

    _last_pull = eventlist().now();

    simtime_picosec drain_time;

    if (_packet_drain_time>0)
        drain_time = _packet_drain_time;
    else {
        int t = (int)(drand()*_pull_spacing_cdf_count);
        drain_time = 10*timeFromNs(_pull_spacing_cdf[t])/20;
        //cout << "Drain time is " << timeAsUs(drain_time);
    }

    if (!_pull_queue.empty()){
        eventlist().sourceIsPendingRel(*this,drain_time);//*(0.5+drand()));
    }
    else {
	//    cout << "Empty pacer queue at " << timeAsMs(eventlist().now()) << endl; 
    }
}


////////////////////////////////////////////////////////////////
//  NDP RETRANSMISSION TIMER
////////////////////////////////////////////////////////////////

NdpRtxTimerScanner::NdpRtxTimerScanner(simtime_picosec scanPeriod, EventList& eventlist)
  : EventSource(eventlist,"RtxScanner"), 
    _scanPeriod(scanPeriod)
{
    eventlist.sourceIsPendingRel(*this, 0);
}

void 
NdpRtxTimerScanner::registerNdp(NdpSrc &tcpsrc)
{
    _tcps.push_back(&tcpsrc);
}

void
NdpRtxTimerScanner::doNextEvent() 
{
    simtime_picosec now = eventlist().now();
    tcps_t::iterator i;
    for (i = _tcps.begin(); i!=_tcps.end(); i++) {
        (*i)->rtx_timer_hook(now,_scanPeriod);
    }
    eventlist().sourceIsPendingRel(*this, _scanPeriod);
}
