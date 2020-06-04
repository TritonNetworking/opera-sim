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

extern uint32_t RTT_rack;
extern uint32_t RTT_net;

string ntoa(double n);
string itoa(uint64_t n);

ExpanderTopology::ExpanderTopology(mem_b queuesize, Logfile* lg, EventList* ev,queue_type q, string topfile){
    _queuesize = queuesize;
    logfile = lg;
    eventlist = ev;
    qt = q;

    read_params(topfile);
 
    set_params();

    init_network();
}

// read the topology info from file (generated in Matlab)
void ExpanderTopology::read_params(string topfile) {
    /*
    _no_of_nodes = 6; // number of servers
    _ndl = 2; // number of downlinks from ToR
    _nul = 2; // number of uplinks from ToR (to other ToRs)
    _ntor = 3; // number of ToRs
    */
  
  ifstream input(topfile);

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
void ExpanderTopology::set_params() {

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
    assert(0);
}

// initializes all the pipes and queues in the Topology
void ExpanderTopology::init_network() {
  QueueLoggerSampling* queueLogger;

  // initialize pipes/queues between ToRs and servers
  for (int j = 0; j < _ntor; j++) { // sweep ToR switches
    for (int k = 0; k < _no_of_nodes; k++) { // sweep servers
      queues_tor_serv[j][k] = NULL;
      pipes_tor_serv[j][k] = NULL;
      queues_serv_tor[k][j] = NULL;
      pipes_serv_tor[k][j] = NULL;
    }
  }

  // create pipes/queues between ToRs and servers
  for (int j = 0; j < _ntor; j++) { // sweep ToRs
    for (int l = 0; l < _ndl; l++) { // sweep ToR downlinks
      int k = j * _ndl + l;

      // Downlink: ToR to server
      queueLogger = new QueueLoggerSampling(timeFromMs(1000), *eventlist);
      logfile->addLogger(*queueLogger);
      queues_tor_serv[j][k] = alloc_queue(queueLogger, _queuesize);
      //queues_tor_serv[j][k]->setName("TOR" + ntoa(j) + "->DST" +ntoa(k));
      //logfile->writeName(*(queues_tor_serv[j][k]));
      pipes_tor_serv[j][k] = new Pipe(timeFromNs(RTT_rack), *eventlist);
      pipes_tor_serv[j][k]->setlongname("Pipe-TOR" + ntoa(j)  + "->DST" + ntoa(k));
      //pipes_tor_serv[j][k]->setName("Pipe-TOR" + ntoa(j)  + "->DST" + ntoa(k));
      //logfile->writeName(*(pipes_tor_serv[j][k]));


      pipes_tor_serv[j][k]->set_pipe_downlink(); // modification - set this for the UtilMonitor

	  
      // Uplink: server to ToR
      queueLogger = new QueueLoggerSampling(timeFromMs(1000), *eventlist);
      logfile->addLogger(*queueLogger);
      queues_serv_tor[k][j] = alloc_src_queue(queueLogger);
      //queues_serv_tor[k][j]->setName("SRC" + ntoa(k) + "->TOR" +ntoa(j));
      //logfile->writeName(*(queues_serv_tor[k][j]));
      pipes_serv_tor[k][j] = new Pipe(timeFromNs(RTT_rack), *eventlist);
      pipes_serv_tor[k][j]->setlongname("Pipe-SRC" + ntoa(k) + "->TOR" + ntoa(j));
      //pipes_serv_tor[k][j]->setName("Pipe-SRC" + ntoa(k) + "->TOR" + ntoa(j));
      //logfile->writeName(*(pipes_serv_tor[k][j]));

    }
  }

  // initialize pipes/queues between ToRs
  for (int j = 0; j < _ntor; j++) // sweep "source" ToR switches
    for (int k = 0; k < _ntor; k++) { // sweep "destination" ToR switches
      queues_tor_tor[j][k] = NULL;
      pipes_tor_tor[j][k] = NULL;
    }

    int pipe_cnt = 0;
  // create pipes/queues between ToRs
  for (int j = 0; j < _ntor; j++) { // sweep "source" ToR switches
    for (int k = 0; k < _ntor; k++) { // sweep "destination" ToR switches
      
      if (_adjacency[j][k] == 1){

        // add pipe and queue
        queueLogger = new QueueLoggerSampling(timeFromMs(1000), *eventlist);
        logfile->addLogger(*queueLogger);
        queues_tor_tor[j][k] = alloc_queue(queueLogger, _queuesize);
        //queues_tor_tor[j][k]->setName("TOR" + ntoa(j) + "->TOR" +ntoa(k));
        //logfile->writeName(*(queues_tor_tor[j][k]));
        pipes_tor_tor[j][k] = new Pipe(timeFromNs(RTT_net), *eventlist);
        pipes_tor_tor[j][k]->setlongname("Pipe-TOR" + ntoa(j)  + "->ToR" + ntoa(k));
        //pipes_tor_tor[j][k]->setName("Pipe-TOR" + ntoa(j)  + "->ToR" + ntoa(k));
        //logfile->writeName(*(pipes_tor_tor[j][k]));

        //if (j == 0) {
        //if (k == 9) {
        //  pipes_tor_tor[j][k]->set_uplink_pipe_id(pipe_cnt);
        //  pipe_cnt++;
        //}

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

int ExpanderTopology::get_num_shortest_paths(int src, int dest) {
  int srcrack = src/_ndl; // index of source rack
  int destrack = dest/_ndl; // index of destination rack
  if (srcrack == destrack)
    return 1;
  else
    return _rts[srcrack][destrack].size();
}

// defines the routes between `src` and `dest` servers
vector<const Route*>* ExpanderTopology::get_paths(int src, int dest, bool vlb) {
  
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
    
    // all paths (previously read from file)

    // first, get the k shortest paths:

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
        routeback->push_back(queues_tor_tor[_rts[srcrack][destrack][i][j]][_rts[srcrack][destrack][i][j-1]]);
        routeback->push_back(pipes_tor_tor[_rts[srcrack][destrack][i][j]][_rts[srcrack][destrack][i][j-1]]);
      }
      
      routeback->push_back(queues_tor_serv[srcrack][src]);
      routeback->push_back(pipes_tor_serv[srcrack][src]);

      routeout->set_reverse(routeback);
      routeback->set_reverse(routeout);

      paths->push_back(routeout);
      check_non_null(routeout);
    }

    if (vlb) {

    // next, get the VLB routes:

    int NVLB = 20;

    for (int imrack_ind = 0; imrack_ind < NVLB; imrack_ind++) { // sweep NVLB possible intermediate ToRs 

    	int imrack = rand() % _ntor; // pick NVLB intermediate ToRs randomly

      if (imrack != srcrack && imrack != destrack) {

        int npaths1 = _rts[srcrack][imrack].size();

        for (int i = 0; i < npaths1; i++) {
          
          // forward path
          routeout = new Route();
          routeout->push_back(queues_serv_tor[src][srcrack]);
          routeout->push_back(pipes_serv_tor[src][srcrack]);

          int nhops = _rts[srcrack][imrack][i].size();
          for (int j = 0; j < nhops-1; j++){
            routeout->push_back(queues_tor_tor[_rts[srcrack][imrack][i][j]][_rts[srcrack][imrack][i][j+1]]);
            routeout->push_back(pipes_tor_tor[_rts[srcrack][imrack][i][j]][_rts[srcrack][imrack][i][j+1]]);
          }

          // we're at the intermediate ToR, now fill in the path from here to the destination
          // !!! note - hardcoding only one path for now !!!

          //int pathchoice = 0;

          // 3/15/19 - modification to increase path diversity:
          int npaths2 = _rts[imrack][destrack].size();
          int pathchoice = random() % npaths2; // randomize the path selection from intermediate ToR to dest ToR.

          
          nhops = _rts[imrack][destrack][pathchoice].size();
          for (int j = 0; j < nhops-1; j++){
            routeout->push_back(queues_tor_tor[_rts[imrack][destrack][pathchoice][j]][_rts[imrack][destrack][pathchoice][j+1]]);
            routeout->push_back(pipes_tor_tor[_rts[imrack][destrack][pathchoice][j]][_rts[imrack][destrack][pathchoice][j+1]]);
          }

          routeout->push_back(queues_tor_serv[destrack][dest]);
          routeout->push_back(pipes_tor_serv[destrack][dest]);

          // -------------------------

          // reverse path for RTS packets
          routeback = new Route();
          routeback->push_back(queues_serv_tor[dest][destrack]);
          routeback->push_back(pipes_serv_tor[dest][destrack]);

          nhops = _rts[imrack][destrack][pathchoice].size();
          for (int j = nhops-1; j > 0; j--){
            routeback->push_back(queues_tor_tor[_rts[imrack][destrack][pathchoice][j]][_rts[imrack][destrack][pathchoice][j-1]]);
            routeback->push_back(pipes_tor_tor[_rts[imrack][destrack][pathchoice][j]][_rts[imrack][destrack][pathchoice][j-1]]);
          }

          // we're at the intermediate ToR

          nhops = _rts[srcrack][imrack][i].size();
          for (int j = nhops-1; j > 0; j--){
            routeback->push_back(queues_tor_tor[_rts[srcrack][imrack][i][j]][_rts[srcrack][imrack][i][j-1]]);
            routeback->push_back(pipes_tor_tor[_rts[srcrack][imrack][i][j]][_rts[srcrack][imrack][i][j-1]]);
          }
      
          routeback->push_back(queues_tor_serv[srcrack][src]);
          routeback->push_back(pipes_tor_serv[srcrack][src]);

          routeout->set_reverse(routeback);
          routeback->set_reverse(routeout);

          paths->push_back(routeout);
          check_non_null(routeout);
        }
      }
    }
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




//////////////////////////////////////////////
//      Aggregate utilization monitor       //
//////////////////////////////////////////////


UtilMonitor::UtilMonitor(ExpanderTopology* top, EventList &eventlist)
  : EventSource(eventlist,"utilmonitor"), _top(top)
{

    _H = _top->get_no_of_nodes(); // number of hosts
    _N = _top->get_ntor(); // racks
    _hpr = _top->get_ndl(); // hosts per rack
    uint64_t rate = 10000000000 / 8; // bytes / second
    rate = rate * _H;

    _max_agg_Bps = rate;

    // debug:
    //cout << "max bytes per second = " << rate << endl;

}

void UtilMonitor::start(simtime_picosec period) {
    _period = period;
    _max_B_in_period = _max_agg_Bps * timeAsSec(_period);

    // debug:
    //cout << "_max_pkts_in_period = " << _max_pkts_in_period << endl;

    eventlist().sourceIsPending(*this, _period);
}

void UtilMonitor::doNextEvent() {
    printAggUtil();
}

void UtilMonitor::printAggUtil() {

    uint64_t B_sum = 0;

    int host = 0;
    for (int tor = 0; tor < _N; tor++) {
        for (int downlink = 0; downlink < _hpr; downlink++) {
            Pipe* pipe = _top->get_downlink(tor, host);
            B_sum = B_sum + pipe->reportBytes();
            host++;
        }
    }

    // debug:
    //cout << "B_sum = " << B_sum << endl;
    //cout << "_max_B_in_period = " << _max_B_in_period << endl;

    double util = (double)B_sum / (double)_max_B_in_period;

    cout << "Util " << fixed << util << " " << timeAsMs(eventlist().now()) << endl;

    //if (eventlist().now() + _period < eventlist().getEndtime())
    eventlist().sourceIsPendingRel(*this, _period);

}