#ifndef MAIN_H
#define MAIN_H

#include <string>


#define HOST_NIC 10000 // host nic speed in Mbps
#define CORE_TO_HOST 1 // core links are this much faster than NIC

//basic setup!


#define NI 3        //Number of intermediate switches
#define NA 6        //Number of aggregation switches
#define NT 9        //Number of ToR switches (180 hosts)

#define NS 20        //Number of servers per ToR switch
#define TOR_AGG2(tor) (10*NA - tor - 1)%NA

#define NT2A 2      //Number of connections from a ToR to aggregation switches

#define TOR_ID(id) N+id
#define AGG_ID(id) N+NT+id
#define INT_ID(id) N+NT+NA+id
#define HOST_ID(hid,tid) tid*NS+hid

#define HOST_TOR(host) host/NS
#define HOST_TOR_ID(host) host%NS
#define TOR_AGG1(tor) tor%NA


// switch_buffer and random_buffer are for 'random_queue'

#define SWITCH_BUFFER 97 // number of packets
#define RANDOM_BUFFER 3 // number of packets
#define FEEDER_BUFFER 1000 // number of packets

#endif
