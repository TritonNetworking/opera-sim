#ifndef EXPANDER
#define EXPANDER
#include "main.h"
#include "randomqueue.h"
#include "pipe.h"
#include "config.h"
#include "loggers.h"
#include "network.h"
#include "firstfit.h"
#include "topology.h"
#include "logfile.h"
#include "eventlist.h"
//#include "switch.h" // don't need this unless we do lossless protocol
#include <ostream>

#ifndef QT
#define QT
typedef enum {RANDOM, ECN, COMPOSITE, CTRL_PRIO} queue_type;
#endif

class ExpanderTopology: public Topology{
  public:

  // basic topology elements: pipes and queues

  vector<vector<Pipe*>> pipes_serv_tor;
  vector<vector<Queue*>> queues_serv_tor;

  vector<vector<Pipe*>> pipes_tor_tor;
  vector<vector<Queue*>> queues_tor_tor;

  vector<vector<Pipe*>> pipes_tor_serv;
  vector<vector<Queue*>> queues_tor_serv;


  FirstFit* ff;
  Logfile* logfile;
  EventList* eventlist;
  int failed_links;
  queue_type qt;

  ExpanderTopology(int no_of_nodes, mem_b queuesize, Logfile* log, EventList* ev, FirstFit* f, queue_type q);

  void init_network();
  virtual vector<const Route*>* get_paths(int src, int dest);

  Queue* alloc_src_queue(QueueLogger* q);
  Queue* alloc_queue(QueueLogger* q, mem_b queuesize);
  Queue* alloc_queue(QueueLogger* q, uint64_t speed, mem_b queuesize);

  void count_queue(Queue*);
  vector<int>* get_neighbours(int src) {return NULL;};
  int no_of_nodes() const {return _no_of_nodes;}

 private:
  map<Queue*,int> _link_usage;
  void read_params();
  vector<vector<int>> _adjacency; // Tor-to-Tor adjacency matrix
  vector<vector<vector<vector<int>>>> _rts; // routes allowed in Expander topology
  void set_params(int no_of_nodes);
  int _ndl, _nul, _ntor, _no_of_nodes; // number down links, number uplinks, number ToRs, number servers
  mem_b _queuesize; // queue sizes
};

#endif
