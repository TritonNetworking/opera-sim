#ifndef TOPOLOGY
#define TOPOLOGY
//#include "network.h" // mod

// interface class - set up functionality for derived topology classes

class Topology {
 public:
  //virtual vector<const Route*>* get_paths(int src,int dest,int slice)=0;
  //virtual vector<int>* get_neighbours(int src) = 0;  
  virtual int no_of_nodes() const { abort();};
};

#endif
