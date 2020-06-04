// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-
#include "test_topology.h"
#include <vector>
#include "string.h"
#include <sstream>
#include <strstream>
#include <iostream>
#include "main.h"
#include "queue.h"
#include "switch.h"
#include "compositequeue.h"
#include "prioqueue.h"
#include "queue_lossless.h"
#include "queue_lossless_input.h"
#include "queue_lossless_output.h"
#include "ecnqueue.h"

extern uint32_t RTT;

string ntoa(double n);
string itoa(uint64_t n);

//extern int N;

TestTopology::TestTopology(int no_of_nodes, mem_b queuesize, Logfile* lg, EventList* ev,FirstFit * fit,queue_type q){
    _queuesize = queuesize;
    logfile = lg;
    eventlist = ev;
    ff = fit;
    qt = q;
    failed_links = 0;
 
    set_params(no_of_nodes);

    init_network();
}

void TestTopology::set_params(int no_of_nodes) {
    cout << "Set params " << no_of_nodes << endl;
    
    NSRV = no_of_nodes; // number of servers
    _no_of_nodes = no_of_nodes;
    K = no_of_nodes; // number of packet switch ports. single switch with NSRV ports

    cout << "K " << K << endl;
    cout << "Queue type " << qt << endl;

    switches_lp.resize(1,NULL); // # lower pod switches

    pipes_nlp_ns.resize(1, vector<Pipe*>(NSRV)); // lower pod switches to servers
    queues_nlp_ns.resize(1, vector<Queue*>(NSRV)); // lower pod switches to servers

    pipes_ns_nlp.resize(NSRV, vector<Pipe*>(1)); // servers to lower pod switches
    queues_ns_nlp.resize(NSRV, vector<Queue*>(1)); // servers to lower pod switches
}

Queue* TestTopology::alloc_src_queue(QueueLogger* queueLogger){
    return  new PriorityQueue(speedFromMbps((uint64_t)HOST_NIC), memFromPkt(FEEDER_BUFFER), *eventlist, queueLogger);
}

Queue* TestTopology::alloc_queue(QueueLogger* queueLogger, mem_b queuesize){
    return alloc_queue(queueLogger, HOST_NIC, queuesize);
}

Queue* TestTopology::alloc_queue(QueueLogger* queueLogger, uint64_t speed, mem_b queuesize){
    if (qt==RANDOM)
	return new RandomQueue(speedFromMbps(speed), memFromPkt(SWITCH_BUFFER + RANDOM_BUFFER), *eventlist, queueLogger, memFromPkt(RANDOM_BUFFER));
    else if (qt==COMPOSITE)
	return new CompositeQueue(speedFromMbps(speed), queuesize, *eventlist, queueLogger);
    else if (qt==CTRL_PRIO)
	return new CtrlPrioQueue(speedFromMbps(speed), queuesize, *eventlist, queueLogger);
    else if (qt==ECN)
	return new ECNQueue(speedFromMbps(speed), memFromPkt(2*SWITCH_BUFFER), *eventlist, queueLogger, memFromPkt(15));
    else if (qt==LOSSLESS)
	return new LosslessQueue(speedFromMbps(speed), memFromPkt(50), *eventlist, queueLogger, NULL);
    else if (qt==LOSSLESS_INPUT)
	return new LosslessOutputQueue(speedFromMbps(speed), memFromPkt(200), *eventlist, queueLogger);    
    else if (qt==LOSSLESS_INPUT_ECN)
	return new LosslessOutputQueue(speedFromMbps(speed), memFromPkt(10000), *eventlist, queueLogger,1,memFromPkt(16));
    assert(0);
}

void TestTopology::init_network(){
  QueueLoggerSampling* queueLogger;
  
  for (int j=0; j<1; j++)
    for (int k=0; k<NSRV; k++){
      queues_nlp_ns[j][k] = NULL;
      pipes_nlp_ns[j][k] = NULL;
      queues_ns_nlp[k][j] = NULL;
      pipes_ns_nlp[k][j] = NULL;
    }

  //create switches if we have lossless operation
  if (qt==LOSSLESS)
      for (int j=0; j<1; j++){
	       switches_lp[j] = new Switch("Switch_LowerPod_"+ntoa(j));
      }
      
  // links from lower layer pod switch to server
  for (int j = 0; j < 1; j++) {
    for (int l = 0; l < K; l++) {
	  int k = j * K/2 + l; // k = l (when there is only one switch and j = 0)
	  // Downlink
	  queueLogger = new QueueLoggerSampling(timeFromMs(1000), *eventlist);
	  //queueLogger = NULL;
	  logfile->addLogger(*queueLogger);
	  
	  queues_nlp_ns[j][k] = alloc_queue(queueLogger, _queuesize);
	  queues_nlp_ns[j][k]->setName("LS" + ntoa(j) + "->DST" +ntoa(k));
	  logfile->writeName(*(queues_nlp_ns[j][k]));

	  pipes_nlp_ns[j][k] = new Pipe(timeFromUs(RTT), *eventlist);
	  pipes_nlp_ns[j][k]->setName("Pipe-LS" + ntoa(j)  + "->DST" + ntoa(k));
	  logfile->writeName(*(pipes_nlp_ns[j][k]));
	  
	  // Uplink
	  queueLogger = new QueueLoggerSampling(timeFromMs(1000), *eventlist);
	  logfile->addLogger(*queueLogger);
	  queues_ns_nlp[k][j] = alloc_src_queue(queueLogger);
	  queues_ns_nlp[k][j]->setName("SRC" + ntoa(k) + "->LS" +ntoa(j));
	  logfile->writeName(*(queues_ns_nlp[k][j]));

	  if (qt==LOSSLESS){
	      switches_lp[j]->addPort(queues_nlp_ns[j][k]);
	      ((LosslessQueue*)queues_nlp_ns[j][k])->setRemoteEndpoint(queues_ns_nlp[k][j]);
	  }else if (qt==LOSSLESS_INPUT || qt == LOSSLESS_INPUT_ECN){
	      //no virtual queue needed at server
	      new LosslessInputQueue(*eventlist,queues_ns_nlp[k][j]);
	  }
	  
	  pipes_ns_nlp[k][j] = new Pipe(timeFromUs(RTT), *eventlist);
	  pipes_ns_nlp[k][j]->setName("Pipe-SRC" + ntoa(k) + "->LS" + ntoa(j));
	  logfile->writeName(*(pipes_ns_nlp[k][j]));
	  
	  if (ff){
	     ff->add_queue(queues_nlp_ns[j][k]);
	     ff->add_queue(queues_ns_nlp[k][j]);
	  }
    }
    }
    
    //init thresholds for lossless operation
    if (qt==LOSSLESS)
	   for (int j=0; j<1; j++){
	    switches_lp[j]->configureLossless();
	   }
}

// ???
void check_non_null(Route* rt){
  int fail = 0;
  for (unsigned int i=1; i<rt->size()-1; i+=2)
    if (rt->at(i)==NULL){
      fail = 1;
      break;
    }
  
  if (fail){
    //    cout <<"Null queue in route"<<endl;
    for (unsigned int i=1; i<rt->size()-1; i+=2)
      printf("%p ",rt->at(i));

    cout<<endl;
    assert(0);
  }
}


// The topology is defined here in `get_paths`:
// since we only have 1 switch, the paths are pretty simple:

vector<const Route*>* TestTopology::get_paths(int src, int dest){
  vector<const Route*>* paths = new vector<const Route*>();

  route_t *routeout, *routeback;
  
    // NOTE: HARD CODED `0` BECAUSE THERE'S ONLY ONE SWITCH

    // forward path
    routeout = new Route();
    //routeout->push_back(pqueue);
    routeout->push_back(queues_ns_nlp[src][0]);
    routeout->push_back(pipes_ns_nlp[src][0]);

    if (qt==LOSSLESS_INPUT || qt==LOSSLESS_INPUT_ECN)
	     routeout->push_back(queues_ns_nlp[src][0]->getRemoteEndpoint());

    routeout->push_back(queues_nlp_ns[0][dest]);
    routeout->push_back(pipes_nlp_ns[0][dest]);

    // reverse path for RTS packets
    routeback = new Route();
    routeback->push_back(queues_ns_nlp[dest][0]);
    routeback->push_back(pipes_ns_nlp[dest][0]);

    if (qt==LOSSLESS_INPUT || qt==LOSSLESS_INPUT_ECN)
	     routeback->push_back(queues_ns_nlp[dest][0]->getRemoteEndpoint());

    routeback->push_back(queues_nlp_ns[0][src]);
    routeback->push_back(pipes_nlp_ns[0][src]);

    routeout->set_reverse(routeback);
    routeback->set_reverse(routeout);

    //print_route(*routeout);
    paths->push_back(routeout);

    check_non_null(routeout);
    return paths;
}

void TestTopology::count_queue(Queue* queue){
  if (_link_usage.find(queue)==_link_usage.end()){
    _link_usage[queue] = 0;
  }

  _link_usage[queue] = _link_usage[queue] + 1;
}

// Find lower pod switch:
int TestTopology::find_lp_switch(Queue* queue){
  //first check ns_nlp
  for (int i=0; i<NSRV; i++)
    for (int j = 0; j<1; j++)
      if (queues_ns_nlp[i][j]==queue)
	      return j;

  //only count nup to nlp
  count_queue(queue);

  return -1;
}

int TestTopology::find_destination(Queue* queue){
  //first check nlp_ns
  for (int i=0; i<1; i++)
    for (int j=0; j<NSRV; j++)
      if (queues_nlp_ns[i][j] == queue)
	      return j;

  return -1;
}

void TestTopology::print_path(std::ofstream &paths,int src,const Route* route){
  paths << "SRC_" << src << " ";
  
  if (route->size()/2==2){
    paths << "LS_" << find_lp_switch((Queue*)route->at(1)) << " ";
    paths << "DST_" << find_destination((Queue*)route->at(3)) << " ";
  } else {
    paths << "Wrong hop count " << ntoa(route->size()/2);
  }
  
  paths << endl;
}
