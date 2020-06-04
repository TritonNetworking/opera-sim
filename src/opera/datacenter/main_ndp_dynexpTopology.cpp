// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        
#include "config.h"
#include <sstream>
#include <strstream>
#include <fstream> // needed to read flows
#include <iostream>
#include <string.h>
#include <math.h>
#include "network.h"
#include "randomqueue.h"
#include "pipe.h"
#include "eventlist.h"
#include "logfile.h"
#include "loggers.h"
#include "clock.h"
#include "ndp.h"
#include "compositequeue.h"
#include "topology.h"
#include <list>
#include "main.h"

// Choose the topology here:
#include "dynexp_topology.h"
#include "rlb.h"
#include "rlbmodule.h"

// Simulation params

#define PRINT_PATHS 0

uint32_t delay_host2ToR = 0; // host-to-tor link delay in nanoseconds
uint32_t delay_ToR2ToR = 500; // tor-to-tor link delay in nanoseconds

#define DEFAULT_PACKET_SIZE 1500 // MAXIMUM FULL packet size (includes header + payload), Bytes
#define DEFAULT_HEADER_SIZE 64 // header size, Bytes
    // note: there is another parameter defined in `ndppacket.h`: "ACKSIZE". This should be set to the same size.
// set the NDP queue size in units of packets (of length DEFAULT_PACKET_SIZE Bytes)
#define DEFAULT_QUEUE_SIZE 8

string ntoa(double n); // convert a double to a string
string itoa(uint64_t n); // convert an int to a string

EventList eventlist;

Logfile* lg;

void exit_error(char* progr){
    cout << "Usage " << progr << " [UNCOUPLED(DEFAULT)|COUPLED_INC|FULLY_COUPLED|COUPLED_EPSILON] [epsilon][COUPLED_SCALABLE_TCP" << endl;
    exit(1);
}

int main(int argc, char **argv) {

	// set maximum packet size in bytes:
    Packet::set_packet_size(DEFAULT_PACKET_SIZE - DEFAULT_HEADER_SIZE);
    mem_b queuesize = DEFAULT_QUEUE_SIZE * DEFAULT_PACKET_SIZE;

    // defined in flags:
    int cwnd;
    stringstream filename(ios_base::out);
    string flowfile; // read the flows from a specified file
    string topfile; // read the topology from a specified file
    double pull_rate; // set the pull rate from the command line
    double simtime; // seconds
    double utiltime = .01; // seconds
    int64_t rlbflow = 0; // flow size of "flagged" RLB flows
    int64_t cutoff = 0; // cutoff between NDP and RLB flow sizes. flows < cutoff == NDP.

    int i = 1;
    filename << "logout.dat";
    RouteStrategy route_strategy = NOT_SET;

    // parse the command line flags:
    while (i<argc) {
        if (!strcmp(argv[i],"-o")){
	       filename.str(std::string());
	       filename << argv[i+1];
	       i++;
        } else if (!strcmp(argv[i],"-cwnd")){
	       cwnd = atoi(argv[i+1]);
	       i++;
        } else if (!strcmp(argv[i],"-q")){
	       queuesize = atoi(argv[i+1]) * DEFAULT_PACKET_SIZE;
	       i++;
        } else if (!strcmp(argv[i],"-strat")){
            if (!strcmp(argv[i+1], "perm")) {
                route_strategy = SCATTER_PERMUTE;
            } else if (!strcmp(argv[i+1], "rand")) {
                route_strategy = SCATTER_RANDOM;
            } else if (!strcmp(argv[i+1], "pull")) {
                route_strategy = PULL_BASED;
            } else if (!strcmp(argv[i+1], "single")) {
                route_strategy = SINGLE_PATH;
            }
            i++;
        } else if (!strcmp(argv[i],"-cutoff")) {
            cutoff = atof(argv[i+1]);
            i++;
        } else if (!strcmp(argv[i],"-rlbflow")) {
            rlbflow = atof(argv[i+1]);
            i++;
        } else if (!strcmp(argv[i],"-flowfile")) {
			flowfile = argv[i+1];
			i++;
        } else if (!strcmp(argv[i],"-topfile")) {
            topfile = argv[i+1];
            i++;
        } else if (!strcmp(argv[i],"-pullrate")) {
            pull_rate = atof(argv[i+1]);
            i++;
        } else if (!strcmp(argv[i],"-simtime")) {
            simtime = atof(argv[i+1]);
            i++;
        } else if (!strcmp(argv[i],"-utiltime")) {
            utiltime = atof(argv[i+1]);
            i++;
        } else {
            exit_error(argv[0]);
        }
        
        i++;
    }
    srand(13); // random seed

    eventlist.setEndtime(timeFromSec(simtime)); // in seconds
    Clock c(timeFromSec(5 / 100.), eventlist);

    route_strategy = SCATTER_RANDOM; // only one routing strategy for now...
      
    if (route_strategy == NOT_SET) {
	   fprintf(stderr, "Route Strategy not set.  Use the -strat param.  \nValid values are perm, rand, pull, rg and single\n");
	   exit(1);
    }

    //cout << "cwnd " << cwnd << endl;
    //cout << "Logging to " << filename.str() << endl;
    Logfile logfile(filename.str(), eventlist);

    lg = &logfile;

    // !!!!!!!!!!! make sure to set StartTime to the correct value !!!!!!!!!!!
    // *set this to be longer than the sim time if you don't want to record anything to the logger
    logfile.setStartTime(timeFromSec(10)); // record events starting at this simulator time

    // NdpSinkLoggerSampling object will iterate through all NdpSinks and log their rate every
    // X microseconds. This is used to get throughput measurements after the experiment ends.
    NdpSinkLoggerSampling sinkLogger = NdpSinkLoggerSampling(timeFromUs(50.), eventlist);

    logfile.addLogger(sinkLogger);
    NdpTrafficLogger traffic_logger = NdpTrafficLogger();
    logfile.addLogger(traffic_logger);
    NdpRtxTimerScanner ndpRtxScanner(timeFromMs(1), eventlist);


// this creates the Expander topology
#ifdef DYNEXP
    DynExpTopology* top = new DynExpTopology(queuesize, &logfile, &eventlist, COMPOSITE, topfile);
#endif

	// initialize all sources/sinks
    NdpSrc::setMinRTO(1000); // microseconds
    NdpSrc::setRouteStrategy(route_strategy);
    NdpSink::setRouteStrategy(route_strategy);

    // debug:
    cout << "Loading traffic..." << endl;

    //ifstream input("flows.txt");
    ifstream input(flowfile);
    if (input.is_open()){
        string line;
        int64_t temp;
        // get flows. Format: (src) (dst) (bytes) (starttime nanoseconds)
        while(!input.eof()){
            vector<int64_t> vtemp;
            getline(input, line);
            stringstream stream(line);
            while (stream >> temp)
                vtemp.push_back(temp);
            //cout << "src = " << vtemp[0] << ", dest = " << vtemp[1] << ", bytes =  " << vtemp[2] << ", start_time[us] " << vtemp[3] << endl;

            // source and destination hosts for this flow
            int flow_src = vtemp[0];
            int flow_dst = vtemp[1];

            if (vtemp[2] < cutoff && vtemp[2] != rlbflow) { // priority flow, sent it over NDP

                // generate an NDP source/sink:
                NdpSrc* flowSrc = new NdpSrc(top, NULL, NULL, eventlist, flow_src, flow_dst);
                flowSrc->setCwnd(cwnd*Packet::data_packet_size()); // congestion window
                flowSrc->set_flowsize(vtemp[2]); // bytes

                // Set the pull rate to something reasonable.
                // we can be more aggressive if the load is lower
                NdpPullPacer* flowpacer = new NdpPullPacer(eventlist, pull_rate); // 1 = pull at line rate
                //NdpPullPacer* flowpacer = new NdpPullPacer(eventlist, .17);

                NdpSink* flowSnk = new NdpSink(top, flowpacer, flow_src, flow_dst);
                ndpRtxScanner.registerNdp(*flowSrc);

                // set up the connection event
                flowSrc->connect(*flowSnk, timeFromNs(vtemp[3]/1.));

                sinkLogger.monitorSink(flowSnk);

            }  else { // background flow, send it over RLB

                // generate an RLB source/sink:

                RlbSrc* flowSrc = new RlbSrc(top, NULL, NULL, eventlist, flow_src, flow_dst);
                // debug:
                //cout << "setting flow size to " << vtemp[2] << " bytes..." << endl;
                flowSrc->set_flowsize(vtemp[2]); // bytes

                RlbSink* flowSnk = new RlbSink(top, eventlist, flow_src, flow_dst);

                // set up the connection event
                flowSrc->connect(*flowSnk, timeFromNs(vtemp[3]/1.));

            }
        }
    }

    cout << "Traffic loaded." << endl;

    RlbMaster* master = new RlbMaster(top, eventlist); // synchronizes the RLBmodules
    master->start();

    // NOTE: UtilMonitor defined in "pipe"
    UtilMonitor* UM = new UtilMonitor(top, eventlist);
    UM->start(timeFromSec(utiltime)); // print utilization every X milliseconds.

    // debug:
    //cout << "Starting... " << endl;


    // Record the setup
    //int pktsize = Packet::data_packet_size();
    //logfile.write("# pktsize=" + ntoa(pktsize) + " bytes");
    //logfile.write("# hostnicrate = " + ntoa(HOST_NIC) + " pkt/sec");
    //logfile.write("# corelinkrate = " + ntoa(HOST_NIC*CORE_TO_HOST) + " pkt/sec");
    //double rtt_rack = timeAsUs(timeFromNs(RTT_rack));
    //logfile.write("# rtt_rack = " + ntoa(rtt_rack) + " microseconds");
    //double rtt_net = timeAsUs(timeFromNs(RTT_net));
    //logfile.write("# rtt_net = " + ntoa(rtt_net) + " microseconds");

    // GO!
    while (eventlist.doNextEvent()) {
    }
    //cout << "Done" << endl;

}

string ntoa(double n) {
    stringstream s;
    s << n;
    return s.str();
}

string itoa(uint64_t n) {
    stringstream s;
    s << n;
    return s.str();
}
