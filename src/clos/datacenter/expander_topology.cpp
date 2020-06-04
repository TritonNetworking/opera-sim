// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-
#include "expander_topology.h"
#include <vector>
#include "string.h"
#include <sstream>
#include <strstream>
#include <iostream>
#include <fstream> // to read from file
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

ExpanderTopology::ExpanderTopology(int no_of_nodes, mem_b queuesize, Logfile* lg, EventList* ev,FirstFit * fit,queue_type q){
    _queuesize = queuesize;
    logfile = lg;
    eventlist = ev;
    ff = fit;
    qt = q;

    read_params();
 
    set_params(no_of_nodes);

    init_network();
}

// read the topology info from file (generated in Matlab)
void ExpanderTopology::read_params() {
    /*
    _no_of_nodes = 6; // number of servers
    _ndl = 2; // number of downlinks from ToR
    _nul = 2; // number of uplinks from ToR (to other ToRs)
    _ntor = 3; // number of ToRs
    */
  
  ifstream input("expander.txt");

  if (input.is_open()){

    // read the first line of basic parameters:
    string line;
    getline(input, line);
    stringstream stream(line);
    stream >> _no_of_nodes;
    stream >> _ndl;
    stream >> _nul;
    stream >> _ntor;

    // get ToR-to-ToR pipes
    int temp;
    _adjacency.resize(_ntor);
    for (int i = 0; i < _ntor; i++) {
      getline(input, line);
      stringstream stream(line);
      for (int j = 0; j < _ntor; j++) {
        stream >> temp;
        _adjacency[i].push_back(temp);
      }
    }

    // get routes (rest of file). Format: (src ToR) (dst ToR) (intermediate ToRs in order)
    _rts.resize(_ntor);
    for (int i = 0; i < _ntor; i++) {
      _rts[i].resize(_ntor);
    }

    int sz = 0;
    while(!input.eof()){
      vector<int> vtemp;
      getline(input, line);
      stringstream stream(line);
      while (stream >> temp)
        vtemp.push_back(temp);
      if (vtemp.size() == 2) { // there are no intermediate ToRs
        sz = _rts[vtemp[0]][vtemp[1]].size();
        _rts[vtemp[0]][vtemp[1]].resize(sz + 1);
        _rts[vtemp[0]][vtemp[1]][sz].push_back(vtemp[0]);
        _rts[vtemp[0]][vtemp[1]][sz].push_back(vtemp[1]);
      }
      else { // there are intermediate ToRs
        sz = _rts[vtemp[0]][vtemp[1]].size();
        _rts[vtemp[0]][vtemp[1]].resize(sz + 1);
        _rts[vtemp[0]][vtemp[1]][sz].push_back(vtemp[0]);
        for (int i = 2; i < vtemp.size(); i++)
          _rts[vtemp[0]][vtemp[1]][sz].push_back(vtemp[i]);
        _rts[vtemp[0]][vtemp[1]][sz].push_back(vtemp[1]);
      }
    }
  }
}

// set number of possible pipes and queues
void ExpanderTopology::set_params(int no_of_nodes) {
    //cout << "Set params: number of nodes = " << no_of_nodes << endl;
    //cout << "Queue type: " << qt << endl;

    pipes_tor_serv.resize(_ntor, vector<Pipe*>(_no_of_nodes)); // tors to servers
    queues_tor_serv.resize(_ntor, vector<Queue*>(_no_of_nodes));

    pipes_serv_tor.resize(_no_of_nodes, vector<Pipe*>(_ntor)); // servers to tors
    queues_serv_tor.resize(_no_of_nodes, vector<Queue*>(_ntor));

    pipes_tor_tor.resize(_ntor, vector<Pipe*>(_ntor)); // tors to tors
    queues_tor_tor.resize(_ntor, vector<Queue*>(_ntor));
}

Queue* ExpanderTopology::alloc_src_queue(QueueLogger* queueLogger) {
    return  new PriorityQueue(speedFromMbps((uint64_t)HOST_NIC), memFromPkt(FEEDER_BUFFER), *eventlist, queueLogger);
}

Queue* ExpanderTopology::alloc_queue(QueueLogger* queueLogger, mem_b queuesize) {
    return alloc_queue(queueLogger, HOST_NIC, queuesize);
}

Queue* ExpanderTopology::alloc_queue(QueueLogger* queueLogger, uint64_t speed, mem_b queuesize) {
    if (qt==RANDOM)
      return new RandomQueue(speedFromMbps(speed), memFromPkt(SWITCH_BUFFER + RANDOM_BUFFER), *eventlist, queueLogger, memFromPkt(RANDOM_BUFFER));
    else if (qt==COMPOSITE)
      return new CompositeQueue(speedFromMbps(speed), queuesize, *eventlist, queueLogger);
    else if (qt==CTRL_PRIO)
      return new CtrlPrioQueue(speedFromMbps(speed), queuesize, *eventlist, queueLogger);
    else if (qt==ECN)
      return new ECNQueue(speedFromMbps(speed), memFromPkt(2*SWITCH_BUFFER), *eventlist, queueLogger, memFromPkt(15));
    assert(0);
}

// initializes all the pipes and queues in the Topology
void ExpanderTopology::init_network() {
  QueueLoggerSampling* queueLogger;

  // initialize pipes/queues between ToRs and servers
  for (int j = 0; j < _ntor; j++) // sweep ToR switches
    for (int k = 0; k < _no_of_nodes; k++) { // sweep servers
      queues_tor_serv[j][k] = NULL;
      pipes_tor_serv[j][k] = NULL;
      queues_serv_tor[k][j] = NULL;
      pipes_serv_tor[k][j] = NULL;
    }
      
  // create pipes/queues between ToRs and servers
  for (int j = 0; j < _ntor; j++) { // sweep ToRs
    for (int l = 0; l < _ndl; l++) { // sweep ToR downlinks
      int k = j * _ndl + l;

      // Downlink: ToR to server
      queueLogger = new QueueLoggerSampling(timeFromMs(1000), *eventlist);
      logfile->addLogger(*queueLogger);
      queues_tor_serv[j][k] = alloc_queue(queueLogger, _queuesize);
      queues_tor_serv[j][k]->setName("TOR" + ntoa(j) + "->DST" +ntoa(k));
      logfile->writeName(*(queues_tor_serv[j][k]));
      pipes_tor_serv[j][k] = new Pipe(timeFromUs(RTT), *eventlist);
      pipes_tor_serv[j][k]->setName("Pipe-TOR" + ntoa(j)  + "->DST" + ntoa(k));
      logfile->writeName(*(pipes_tor_serv[j][k]));
	  
      // Uplink: server to ToR
      queueLogger = new QueueLoggerSampling(timeFromMs(1000), *eventlist);
      logfile->addLogger(*queueLogger);
      queues_serv_tor[k][j] = alloc_src_queue(queueLogger);
      queues_serv_tor[k][j]->setName("SRC" + ntoa(k) + "->TOR" +ntoa(j));
      logfile->writeName(*(queues_serv_tor[k][j]));
      pipes_serv_tor[k][j] = new Pipe(timeFromUs(RTT), *eventlist);
      pipes_serv_tor[k][j]->setName("Pipe-SRC" + ntoa(k) + "->TOR" + ntoa(j));
      logfile->writeName(*(pipes_serv_tor[k][j]));
	  
      if (ff) {
        ff->add_queue(queues_tor_serv[j][k]);
        ff->add_queue(queues_serv_tor[k][j]);
      }
    }
  }

  // initialize pipes/queues between ToRs
  for (int j = 0; j < _ntor; j++) // sweep "source" ToR switches
    for (int k = 0; k < _ntor; k++) { // sweep "destination" ToR switches
      queues_tor_tor[j][k] = NULL;
      pipes_tor_tor[j][k] = NULL;
    }

  // create pipes/queues between ToRs
  for (int j = 0; j < _ntor; j++) { // sweep "source" ToR switches
    for (int k = 0; k < _ntor; k++) { // sweep "destination" ToR switches
      
      if (_adjacency[j][k] == 1){

        // add pipe and queue
        queueLogger = new QueueLoggerSampling(timeFromMs(1000), *eventlist);
        logfile->addLogger(*queueLogger);
        queues_tor_tor[j][k] = alloc_queue(queueLogger, _queuesize);
        queues_tor_tor[j][k]->setName("TOR" + ntoa(j) + "->TOR" +ntoa(k));
        logfile->writeName(*(queues_tor_tor[j][k]));
        pipes_tor_tor[j][k] = new Pipe(timeFromUs(RTT), *eventlist);
        pipes_tor_tor[j][k]->setName("Pipe-TOR" + ntoa(j)  + "->ToR" + ntoa(k));
        logfile->writeName(*(pipes_tor_tor[j][k]));
    
        if (ff){
          ff->add_queue(queues_tor_tor[j][k]);
        }

      }
    }
  }

}

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


// defines the routes between `src` and `dest` servers
vector<const Route*>* ExpanderTopology::get_paths(int src, int dest){
  
  vector<const Route*>* paths = new vector<const Route*>();
  route_t *routeout, *routeback;

  int srcrack = src/_ndl; // index of source rack
  int destrack = dest/_ndl; // index of destination rack
  
  // if `src` and `dest` are in the same rack:
  if (srcrack == destrack) {

    // forward path
    routeout = new Route();
    routeout->push_back(queues_serv_tor[src][srcrack]);
    routeout->push_back(pipes_serv_tor[src][srcrack]);

    routeout->push_back(queues_tor_serv[srcrack][dest]);
    routeout->push_back(pipes_tor_serv[srcrack][dest]);

    // reverse path for RTS packets
    routeback = new Route();
    routeback->push_back(queues_serv_tor[dest][srcrack]);
    routeback->push_back(pipes_serv_tor[dest][srcrack]);

    routeback->push_back(queues_tor_serv[srcrack][src]);
    routeback->push_back(pipes_tor_serv[srcrack][src]);

    routeout->set_reverse(routeback);
    routeback->set_reverse(routeout);

    //print_route(*routeout);
    paths->push_back(routeout);
    check_non_null(routeout);

    return paths;
  }
  else { // `src` and `dest` are in different racks
    
    /*
    // only direct paths:
    
    // forward path
    routeout = new Route();
    routeout->push_back(queues_serv_tor[src][srcrack]);
    routeout->push_back(pipes_serv_tor[src][srcrack]);
    routeout->push_back(queues_tor_tor[srcrack][destrack]);
    routeout->push_back(pipes_tor_tor[srcrack][destrack]);
    routeout->push_back(queues_tor_serv[destrack][dest]);
    routeout->push_back(pipes_tor_serv[destrack][dest]);

    // reverse path for RTS packets
    routeback = new Route();
    routeback->push_back(queues_serv_tor[dest][destrack]);
    routeback->push_back(pipes_serv_tor[dest][destrack]);
    routeout->push_back(queues_tor_tor[destrack][srcrack]);
    routeout->push_back(pipes_tor_tor[destrack][srcrack]);
    routeback->push_back(queues_tor_serv[srcrack][src]);
    routeback->push_back(pipes_tor_serv[srcrack][src]);

    routeout->set_reverse(routeback);
    routeback->set_reverse(routeout);

    //print_route(*routeout);
    paths->push_back(routeout);
    check_non_null(routeout);
    */
    
    // all paths (read from file):

    int npaths = _rts[srcrack][destrack].size();

    for (int i = 0; i < npaths; i++) {
      
      // forward path
      routeout = new Route();
      routeout->push_back(queues_serv_tor[src][srcrack]);
      routeout->push_back(pipes_serv_tor[src][srcrack]);

      int nhops = _rts[srcrack][destrack][i].size();
      for (int j = 0; j < nhops-1; j++){
        routeout->push_back(queues_tor_tor[_rts[srcrack][destrack][i][j]][_rts[srcrack][destrack][i][j+1]]);
        routeout->push_back(pipes_tor_tor[_rts[srcrack][destrack][i][j]][_rts[srcrack][destrack][i][j+1]]);
      }

      routeout->push_back(queues_tor_serv[destrack][dest]);
      routeout->push_back(pipes_tor_serv[destrack][dest]);

      // reverse path for RTS packets
      routeback = new Route();
      routeback->push_back(queues_serv_tor[dest][destrack]);
      routeback->push_back(pipes_serv_tor[dest][destrack]);

      for (int j = nhops-1; j > 0; j--){
        routeout->push_back(queues_tor_tor[_rts[srcrack][destrack][i][j]][_rts[srcrack][destrack][i][j-1]]);
        routeout->push_back(pipes_tor_tor[_rts[srcrack][destrack][i][j]][_rts[srcrack][destrack][i][j-1]]);
      }
      
      routeback->push_back(queues_tor_serv[srcrack][src]);
      routeback->push_back(pipes_tor_serv[srcrack][src]);

      routeout->set_reverse(routeback);
      routeback->set_reverse(routeout);

      paths->push_back(routeout);
      check_non_null(routeout);
    }
    
    return paths;

  }
}

void ExpanderTopology::count_queue(Queue* queue){
  if (_link_usage.find(queue)==_link_usage.end()){
    _link_usage[queue] = 0;
  }

  _link_usage[queue] = _link_usage[queue] + 1;
}
