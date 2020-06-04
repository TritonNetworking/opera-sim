// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        
#include "compositequeue.h"
#include <math.h>

#include <iostream>
#include <sstream>

#include "dynexp_topology.h"
#include "rlbpacket.h" // added for debugging
#include "rlbmodule.h"

// !!! NOTE: this one does selective RLB packet dropping.

// !!! NOTE: this has been modified to also include a lower priority RLB queue

CompositeQueue::CompositeQueue(linkspeed_bps bitrate, mem_b maxsize, EventList& eventlist, 
			       QueueLogger* logger, int tor, int port)
  : Queue(bitrate, maxsize, eventlist, logger)
{
  _tor = tor;
  _port = port;
  // original version:
  //_ratio_high = 10; // number of headers to send per data packet (originally 24 for Jan '18 version)
  //_ratio_low = 1; // number of full packets
  // new version:
  _ratio_high = 640; // bytes (640 = 10 64B headers)
  _ratio_low = 1500; // bytes (1500 = 1 1500B packet)
  _crt = 0;
  _num_headers = 0;
  _num_packets = 0;
  _num_acks = 0;
  _num_nacks = 0;
  _num_pulls = 0;
  _num_drops = 0;
  _num_stripped = 0;
  _num_bounced = 0;

  _queuesize_high = _queuesize_low = _queuesize_rlb = 0;
  _serv = QUEUE_INVALID;

}

void CompositeQueue::beginService(){
	if ( !_enqueued_high.empty() && !_enqueued_low.empty() ){

		if (_crt >= (_ratio_high+_ratio_low))
			_crt = 0;

		if (_crt< _ratio_high) {
			_serv = QUEUE_HIGH;
			eventlist().sourceIsPendingRel(*this, drainTime(_enqueued_high.back()));
			_crt = _crt + 64; // !!! hardcoded header size for now...

			// debug:
			//if (_tor == 0 && _port == 6)
			//	cout << "composite_queue sending a header (full packets in queue)" << endl;
		} else {
			assert(_crt < _ratio_high+_ratio_low);
			_serv = QUEUE_LOW;
			eventlist().sourceIsPendingRel(*this, drainTime(_enqueued_low.back()));
			int sz = _enqueued_low.back()->size();
			_crt = _crt + sz;
			// debug:
			//if (_tor == 0 && _port == 6) {
			//	cout << "composite_queue sending a full packet (headers in queue)" << endl;
			//	cout << "   NDP packet size measured at composite_queue = " << sz << " bytes" << endl;
			//}
		}
		return;
	}

	if (!_enqueued_high.empty()) {
		_serv = QUEUE_HIGH;
		eventlist().sourceIsPendingRel(*this, drainTime(_enqueued_high.back()));

		// debug:
		//if (_tor == 0 && _port == 6)
		//	cout << "composite_queue sending a header (no packets in queue)" << endl;

	} else if (!_enqueued_low.empty()) {
		_serv = QUEUE_LOW;
		eventlist().sourceIsPendingRel(*this, drainTime(_enqueued_low.back()));

		// debug:
		//if (_tor == 0 && _port == 6)
		//	cout << "composite_queue sending a full packet: " << (_enqueued_low.back())->size() << " bytes (no headers in queue)" << endl;

	} else if (!_enqueued_rlb.empty()) {
		_serv = QUEUE_RLB;
		eventlist().sourceIsPendingRel(*this, drainTime(_enqueued_rlb.back()));
	} else {
		assert(0);
		_serv = QUEUE_INVALID;
	}
}

void CompositeQueue::completeService() {

	Packet* pkt;

	uint64_t new_NDP_bytes_sent;

	bool sendingpkt = true;

	if (_serv == QUEUE_RLB) {
		assert(!_enqueued_rlb.empty());
		pkt = _enqueued_rlb.back();

		DynExpTopology* top = pkt->get_topology();
		int ul = top->no_of_hpr(); // !!! # uplinks = # downlinks = # hosts per rack

		if (_port >= ul) { // it's a ToR uplink

			// check if we're still connected to the right rack:

			// get the current slice:
			int64_t superslice = (eventlist().now() / top->get_slicetime(3)) %
                top->get_nsuperslice();
            // next, get the relative time from the beginning of that superslice
            int64_t reltime = eventlist().now() - superslice*top->get_slicetime(3) -
                (eventlist().now() / (top->get_nsuperslice()*top->get_slicetime(3))) * 
                (top->get_nsuperslice()*top->get_slicetime(3));
            int slice; // the current slice
            if (reltime < top->get_slicetime(0))
                slice = 0 + superslice*3;
            else if (reltime < top->get_slicetime(0) + top->get_slicetime(1))
                slice = 1 + superslice*3;
            else
                slice = 2 + superslice*3;

            bool pktfound = false;

            while (!_enqueued_rlb.empty()) {

            	pkt = _enqueued_rlb.back(); // get the next packet

            	// get the destination ToR:
            	int dstToR = top->get_firstToR(pkt->get_dst());
            	// get the currently-connected ToR:
            	int nextToR = top->get_nextToR(slice, pkt->get_crtToR(), pkt->get_crtport());

            	if (dstToR == nextToR) {
            		// this is a "fresh" RLB packet
            		_enqueued_rlb.pop_back();
					_queuesize_rlb -= pkt->size();
					pktfound = true;
            		break;
            	} else {
            		// this is an old packet, "drop" it and move on to the next one

            		RlbPacket *p = (RlbPacket*)(pkt);

            		// debug:
            		//cout << "X dropped an RLB packet at port " << pkt->get_crtport() << ", seqno:" << p->seqno() << endl;
            		//cout << "   ~ checked @ " << timeAsUs(eventlist().now()) << " us: specified ToR:" << dstToR << ", current ToR:" << nextToR << endl;
            		
            		// old version: just drop the packet
            		//pkt->free();

            		// new version: NACK the packet
            		// NOTE: have not actually implemented NACK mechanism... Future work
            		RlbModule* module = top->get_rlb_module(p->get_src()); // returns pointer to Rlb module that sent the packet
    				module->receivePacket(*p, 1); // 1 means to put it at the front of the queue

            		_enqueued_rlb.pop_back(); // pop the packet
					_queuesize_rlb -= pkt->size(); // decrement the queue size
            	}
            }

            // we went through the whole queue and they were all "old" packets
            if (!pktfound)
            	sendingpkt = false;

		} else { // its a ToR downlink
			_enqueued_rlb.pop_back();
			_queuesize_rlb -= pkt->size();
		}

		//_num_packets++;
	} else if (_serv == QUEUE_LOW) {
		assert(!_enqueued_low.empty());
		pkt = _enqueued_low.back();
		_enqueued_low.pop_back();
		_queuesize_low -= pkt->size();
		_num_packets++;
	} else if (_serv == QUEUE_HIGH) {
		assert(!_enqueued_high.empty());
		pkt = _enqueued_high.back();
		_enqueued_high.pop_back();
		_queuesize_high -= pkt->size();
    	if (pkt->type() == NDPACK)
			_num_acks++;
    	else if (pkt->type() == NDPNACK)
			_num_nacks++;
    	else if (pkt->type() == NDPPULL)
			_num_pulls++;
    	else {
			_num_headers++;
    	}
  	} else {
    	assert(0);
  	}
    
    if (sendingpkt)
  		sendFromQueue(pkt);

  	_serv = QUEUE_INVALID;

  	if (!_enqueued_high.empty() || !_enqueued_low.empty() || !_enqueued_rlb.empty())
  		beginService();
}

void CompositeQueue::doNextEvent() {
	completeService();
}

void CompositeQueue::receivePacket(Packet& pkt) {

	// debug:
	//if (pkt.get_time_sent() == 342944606400 && pkt.get_real_src() == 177 && pkt.get_real_dst() == 423)
	//	cout << "debug @compositequeue: ToR " << _tor << ", port " << _port << " received the packet" << endl;

	// debug:
	//cout << "_maxsize = " << _maxsize << endl;

    //pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_ARRIVE);

    // debug:
	//if (pkt.been_bounced() == true && pkt.bounced() == false) {
	//	cout << "ToR " << _tor << " received a previously bounced packet" << endl;
	//	cout << "    src = " << pkt.get_src() << endl;
	//} else if (pkt.bounced() == true) {
	//	cout << "ToR " << _tor << " received a currently bounced packet" << endl;
	//	cout << "    src = " << pkt.get_src() << endl;
	//}

	switch (pkt.type()) {
    case RLB:
    {
        
    	// debug:
    	//RlbPacket *p = (RlbPacket*)(&pkt);
        //    if (p->seqno() == 1)
        //        cout << "# marked packet queued at ToR: " << _tor << ", port: " << _port << endl;

    	_enqueued_rlb.push_front(&pkt);
		_queuesize_rlb += pkt.size();

        break;
    }
    case NDP:
    case NDPACK:
    case NDPNACK:
    case NDPPULL:
    {
        if (!pkt.header_only()){

        	// debug:
			//if (_tor == 0 && _port == 6)
			//	cout << "> received a full packet: " << pkt.size() << " bytes" << endl;

			if (_queuesize_low + pkt.size() <= _maxsize || drand()<0.5) {
				//regular packet; don't drop the arriving packet

				// we are here because either the queue isn't full or,
				// it might be full and we randomly chose an
				// enqueued packet to trim

				bool chk = true;
		    
				if (_queuesize_low + pkt.size() > _maxsize) {
					// we're going to drop an existing packet from the queue

					// debug:
					//if (_tor == 0 && _port == 6)
					//	cout << "  x clipping a queued packet. (_queuesize_low = " << _queuesize_low << ", pkt.size() = " << pkt.size() << ", _maxsize = " << _maxsize << ")" << endl;

					if (_enqueued_low.empty()){
						//cout << "QUeuesize " << _queuesize_low << " packetsize " << pkt.size() << " maxsize " << _maxsize << endl;
						assert(0);
					}
					//take last packet from low prio queue, make it a header and place it in the high prio queue

					Packet* booted_pkt = _enqueued_low.front();

					// added a check to make sure that the booted packet makes enough space in the queue
        			// for the incoming packet
					if (booted_pkt->size() >= pkt.size()) {

						chk = true;

						_enqueued_low.pop_front();
						_queuesize_low -= booted_pkt->size();

						booted_pkt->strip_payload();
						_num_stripped++;
						booted_pkt->flow().logTraffic(*booted_pkt,*this,TrafficLogger::PKT_TRIM);
						if (_logger)
							_logger->logQueue(*this, QueueLogger::PKT_TRIM, pkt);
				
						if (_queuesize_high+booted_pkt->size() > _maxsize) {

							// debug:
							//cout << "!!! NDP - header queue overflow <booted> ..." << endl;

							// old stuff -------------
							//cout << "Error - need to implement RTS handling!" << endl;
							//abort();
							//booted_pkt->free();
							// -----------------------

							// new stuff:

							if (booted_pkt->bounced() == false) {
								//return the packet to the sender
								//if (_logger) _logger->logQueue(*this, QueueLogger::PKT_BOUNCE, *booted_pkt);
								//booted_pkt->flow().logTraffic(pkt,*this,TrafficLogger::PKT_BOUNCE);

								// debug:
								//cout << "   ... returning to sender." << endl;

								DynExpTopology* top = booted_pkt->get_topology();
								booted_pkt->bounce(); // indicate that the packet has been bounced
								_num_bounced++;

					            // flip the source and dst of the packet:
					            int s = booted_pkt->get_src();
					            int d = booted_pkt->get_dst();
					            booted_pkt->set_src(d);
					            booted_pkt->set_dst(s);

					            // get the current ToR, this will be the new src_ToR of the packet
					            int new_src_ToR = booted_pkt->get_crtToR();

					            if (new_src_ToR == booted_pkt->get_src_ToR()) {
					            	// the packet got returned at the source ToR
					            	// we need to send on a downlink right away
					            	booted_pkt->set_src_ToR(new_src_ToR);
					            	booted_pkt->set_crtport(top->get_lastport(booted_pkt->get_dst()));
					            	booted_pkt->set_maxhops(0);

					            	// debug:
						            //cout << "   packet RTSed at the first ToR (ToR = " << new_src_ToR << ")" << endl;

					            } else {
					            	booted_pkt->set_src_ToR(new_src_ToR);

					            	// get paths
					            	// get the current slice:
									int64_t superslice = (eventlist().now() / top->get_slicetime(3)) %
						                top->get_nsuperslice();
						            // next, get the relative time from the beginning of that superslice
						            int64_t reltime = eventlist().now() - superslice*top->get_slicetime(3) -
						                (eventlist().now() / (top->get_nsuperslice()*top->get_slicetime(3))) * 
						                (top->get_nsuperslice()*top->get_slicetime(3));
						            int slice; // the current slice
						            if (reltime < top->get_slicetime(0))
						                slice = 0 + superslice*3;
						            else if (reltime < top->get_slicetime(0) + top->get_slicetime(1))
						                slice = 1 + superslice*3;
						            else
						                slice = 2 + superslice*3;

						            booted_pkt->set_slice_sent(slice); // "timestamp" the packet
						            // get the number of available paths for this packet during this slice
					            	int npaths = top->get_no_paths(booted_pkt->get_src_ToR(),
					                	top->get_firstToR(booted_pkt->get_dst()), slice);
						            if (npaths == 0)
						                cout << "Error: there were no paths!" << endl;
						            assert(npaths > 0);

						            // randomly choose a path for the packet
						            int path_index = random() % npaths;
					            	booted_pkt->set_path_index(path_index); // set which path the packet will take
					            	booted_pkt->set_maxhops(top->get_no_hops(booted_pkt->get_src_ToR(),
					                	top->get_firstToR(booted_pkt->get_dst()), slice, path_index));

					            	booted_pkt->set_crtport(top->get_port(new_src_ToR, top->get_firstToR(booted_pkt->get_dst()),
					            		slice, path_index, 0)); // current hop = 0 (hardcoded)

					            }

					            booted_pkt->set_crthop(0);
					            booted_pkt->set_crtToR(new_src_ToR);

					            // debug:
						        //cout << "   packet RTSed to node " << booted_pkt->get_dst() << " at ToR = " << new_src_ToR << endl;

					            Queue* nextqueue = top->get_queue_tor(booted_pkt->get_crtToR(), booted_pkt->get_crtport());
	            				nextqueue->receivePacket(*booted_pkt);


						    } else {
						    	// debug:
								cout << "   ... this is an RTS packet. Dropped.\n";
								//booted_pkt->flow().logTraffic(*booted_pkt,*this,TrafficLogger::PKT_DROP);
								booted_pkt->free();
								//if (_logger) _logger->logQueue(*this, QueueLogger::PKT_DROP, pkt);
						    }
						}
						else {
							_enqueued_high.push_front(booted_pkt);
							_queuesize_high += booted_pkt->size();
						}
					} else {
						chk = false;
					}
				}

				if (chk) {
					// the new packet fit
					assert(_queuesize_low + pkt.size() <= _maxsize);
					_enqueued_low.push_front(&pkt);
					_queuesize_low += pkt.size();
					if (_serv==QUEUE_INVALID) {
						beginService();
					}
					return;
				} else {
					// the packet wouldn't fit if we booted the existing packet
					pkt.strip_payload();
					_num_stripped++;
				}

			} else {
				//strip payload on the arriving packet - low priority queue is full
				pkt.strip_payload();
				_num_stripped++;

				// debug:
				//if (_tor == 0 && _port == 6)
				//	cout << "  > stripping payload of arriving packet" << endl;

			}
	    }

	    assert(pkt.header_only());
	    
	    if (_queuesize_high + pkt.size() > _maxsize){
			
			// debug:
			//cout << "!!! NDP - header queue overflow ..." << endl;

			// old stuff -------------
			//cout << "_queuesize_high = " << _queuesize_high << endl;
			//cout << "Error - need to implement RTS handling!" << endl;
			//abort();
			//pkt.free();
			// -----------------------

			// new stuff:

			if (pkt.bounced() == false) {
	    		//return the packet to the sender
	    		//if (_logger) _logger->logQueue(*this, QueueLogger::PKT_BOUNCE, pkt);
	    		//pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_BOUNCE);

	    		// debug:
				//cout << "   ... returning to sender." << endl;

				DynExpTopology* top = pkt.get_topology();
	    		pkt.bounce();
	    		_num_bounced++;

	            // flip the source and dst of the packet:
	            int s = pkt.get_src();
	            int d = pkt.get_dst();
	            pkt.set_src(d);
	            pkt.set_dst(s);

	            // get the current ToR, this will be the new src_ToR of the packet
	            int new_src_ToR = pkt.get_crtToR();

	            if (new_src_ToR == pkt.get_src_ToR()) {
	            	// the packet got returned at the source ToR
	            	// we need to send on a downlink right away
	            	pkt.set_src_ToR(new_src_ToR);
	            	pkt.set_crtport(top->get_lastport(pkt.get_dst()));
	            	pkt.set_maxhops(0);

	            	// debug:
		            //cout << "   packet RTSed at the first ToR (ToR = " << new_src_ToR << ")" << endl;

	            } else {
	            	pkt.set_src_ToR(new_src_ToR);

	            	// get paths
	            	// get the current slice:
					int64_t superslice = (eventlist().now() / top->get_slicetime(3)) %
		                top->get_nsuperslice();
		            // next, get the relative time from the beginning of that superslice
		            int64_t reltime = eventlist().now() - superslice*top->get_slicetime(3) -
		                (eventlist().now() / (top->get_nsuperslice()*top->get_slicetime(3))) * 
		                (top->get_nsuperslice()*top->get_slicetime(3));
		            int slice; // the current slice
		            if (reltime < top->get_slicetime(0))
		                slice = 0 + superslice*3;
		            else if (reltime < top->get_slicetime(0) + top->get_slicetime(1))
		                slice = 1 + superslice*3;
		            else
		                slice = 2 + superslice*3;

		            pkt.set_slice_sent(slice); // "timestamp" the packet
		            // get the number of available paths for this packet during this slice
	            	int npaths = top->get_no_paths(pkt.get_src_ToR(),
	                	top->get_firstToR(pkt.get_dst()), slice);
		            if (npaths == 0)
		                cout << "Error: there were no paths!" << endl;
		            assert(npaths > 0);

		            // randomly choose a path for the packet
		            int path_index = random() % npaths;
	            	pkt.set_path_index(path_index); // set which path the packet will take
	            	pkt.set_maxhops(top->get_no_hops(pkt.get_src_ToR(),
	                	top->get_firstToR(pkt.get_dst()), slice, path_index));

	            	pkt.set_crtport(top->get_port(new_src_ToR, top->get_firstToR(pkt.get_dst()),
	            		slice, path_index, 0)); // current hop = 0 (hardcoded)

	            }

	            // debug:
		        //cout << "   packet RTSed to node " << pkt.get_dst() << " at ToR = " << new_src_ToR << endl;

	            pkt.set_crthop(0);
	            pkt.set_crtToR(new_src_ToR);

	            Queue* nextqueue = top->get_queue_tor(pkt.get_crtToR(), pkt.get_crtport());
				nextqueue->receivePacket(pkt);

	    		return;
			} else {

				// debug:
				cout << "   ... this is an RTS packet. Dropped.\n";
				//if (_logger) _logger->logQueue(*this, QueueLogger::PKT_DROP, pkt);
	    		//pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_DROP);

				pkt.free();
				_num_drops++;
	    		return;
			}
	    }

	    // debug:
	    //if (_tor == 0 && _port == 6)
	    //	cout << "  > enqueueing header" << endl;
	    
	    _enqueued_high.push_front(&pkt);
	    _queuesize_high += pkt.size();

        break;
    }
    }
    
    if (_serv == QUEUE_INVALID) {
		beginService();
    }
}

mem_b CompositeQueue::queuesize() {
    return _queuesize_low + _queuesize_high;
}
