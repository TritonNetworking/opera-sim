// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        
#include "config.h"
#include <sstream>
#include <strstream>
#include <fstream> // need to read flows
#include <iostream>
#include <string.h>
#include <math.h>
#include "network.h"
#include "randomqueue.h"
//#include "shortflows.h"
#include "pipe.h"
#include "eventlist.h"
#include "logfile.h"
#include "loggers.h"
#include "clock.h"
#include "tcp.h"
#include "mtcp.h"
#include "compositequeue.h"
#include "firstfit.h"
#include "topology.h"
//#include "connection_matrix.h"

// Choose the topology here:
#include "fat_tree_topology.h"

#include <list>

// Simulation params

#define PRINT_PATHS 0

#define PERIODIC 0
#include "main.h"

uint32_t RTT_rack = 0; // ns
uint32_t RTT_net = 500; // ns
int DEFAULT_NODES = 16;

FirstFit* ff = NULL;
//unsigned int subflow_count = 8; // probably not necessary ???

#define DEFAULT_PACKET_SIZE 1500 // full packet (including header), Bytes
#define DEFAULT_HEADER_SIZE 64 // header size, Bytes
#define DEFAULT_QUEUE_SIZE 100


string ntoa(double n);
string itoa(uint64_t n);

EventList eventlist;
Logfile* lg;

void exit_error(char* progr, char *param) {
    cerr << "Bad parameter: " << param << endl;
    cerr << "Usage " << progr << " [UNCOUPLED(DEFAULT)|COUPLED_INC|FULLY_COUPLED|COUPLED_EPSILON] [epsilon][COUPLED_SCALABLE_TCP]" << endl;
    exit(1);
}

void print_path(std::ofstream &paths, const Route* rt){
    for (unsigned int i=1;i<rt->size()-1;i+=2){
	RandomQueue* q = (RandomQueue*)rt->at(i);
	if (q!=NULL)
	    paths << q->str() << " ";
	else 
	    paths << "NULL ";
    }

    paths<<endl;
}

int main(int argc, char **argv) {

    TcpPacket::set_packet_size(DEFAULT_PACKET_SIZE - DEFAULT_HEADER_SIZE); // MTU
    mem_b queuesize = DEFAULT_QUEUE_SIZE * DEFAULT_PACKET_SIZE;

    int algo = UNCOUPLED;
    double epsilon = 1;
    int ssthresh = 15;

    int no_of_nodes = DEFAULT_NODES;

    string flowfile; // so we can read the flows from a specified file
    double simtime; // seconds
    double utiltime=.01; // seconds

    stringstream filename(ios_base::out);
    int i = 1;
    filename << "logout.dat";

    while (i<argc) {
	if (!strcmp(argv[i],"-o")){
	    filename.str(std::string());
	    filename << argv[i+1];
	    i++;
	} else if (!strcmp(argv[i],"-nodes")){
	    no_of_nodes = atoi(argv[i+1]);
	    cout << "no_of_nodes "<<no_of_nodes << endl;
	    i++;
	} else if (!strcmp(argv[i],"-ssthresh")){
	    ssthresh = atoi(argv[i+1]);
	    cout << "ssthresh "<< ssthresh << endl;
	    i++;
	} else if (!strcmp(argv[i],"-q")){
	    queuesize = memFromPkt(atoi(argv[i+1]));
	    cout << "queuesize "<<queuesize << endl;
	    i++;
	} else if (!strcmp(argv[i], "UNCOUPLED"))
	    algo = UNCOUPLED;
	else if (!strcmp(argv[i], "COUPLED_INC"))
	    algo = COUPLED_INC;
	else if (!strcmp(argv[i], "FULLY_COUPLED"))
	    algo = FULLY_COUPLED;
	else if (!strcmp(argv[i], "COUPLED_TCP"))
	    algo = COUPLED_TCP;
	else if (!strcmp(argv[i], "COUPLED_SCALABLE_TCP"))
	    algo = COUPLED_SCALABLE_TCP;
	else if (!strcmp(argv[i], "COUPLED_EPSILON")) {
	    algo = COUPLED_EPSILON;
	    if (argc > i+1){
		epsilon = atof(argv[i+1]);
		i++;
	    }
	    printf("Using epsilon %f\n", epsilon);
	} else if (!strcmp(argv[i],"-flowfile")) {
		flowfile = argv[i+1];
		i++;
    } else if (!strcmp(argv[i],"-simtime")) {
        simtime = atof(argv[i+1]);
        i++;
	} else if (!strcmp(argv[i],"-utiltime")) {
        utiltime = atof(argv[i+1]);
        i++;
	} else
	    exit_error(argv[0], argv[i]);
		i++;
    }
    srand(13);

    eventlist.setEndtime(timeFromSec(simtime));
    Clock c(timeFromSec(5 / 100.), eventlist);

      
    //cout <<  "Using algo="<<algo<< " epsilon=" << epsilon << endl;

    Logfile logfile(filename.str(), eventlist);

#if PRINT_PATHS
    filename << ".paths";
    cout << "Logging path choices to " << filename.str() << endl;
    std::ofstream paths(filename.str().c_str());
    if (!paths){
	cout << "Can't open for writing paths file!"<<endl;
	exit(1);
    }
#endif

    lg = &logfile;

    


    // !!!!!!!!!!!!!!!!!!!!!!!
    logfile.setStartTime(timeFromSec(10));






    TcpSinkLoggerSampling sinkLogger = TcpSinkLoggerSampling(timeFromUs(50.), eventlist);
    logfile.addLogger(sinkLogger);
    TcpTrafficLogger traffic_logger = TcpTrafficLogger();
    logfile.addLogger(traffic_logger);

    TcpRtxTimerScanner tcpRtxScanner(timeFromMs(1), eventlist);

#ifdef FAT_TREE
    FatTreeTopology* top = new FatTreeTopology(no_of_nodes, queuesize, &logfile, &eventlist, ff, RANDOM);
    // note that 'queuesize' does not pass through currently for RANDOM...
#endif


	ifstream input(flowfile);
    if (input.is_open()){
        string line;
        int64_t temp;
        // get flows. Format: (src) (dst) (bytes) (starttime microseconds)
        while(!input.eof()){
            vector<int64_t> vtemp;
            getline(input, line);
            stringstream stream(line);
            while (stream >> temp)
                vtemp.push_back(temp);
            //cout << "src = " << vtemp[0] << " dest = " << vtemp[1] << " bytes " << vtemp[2] << " time " << vtemp[3] << endl;


            // source and destination hosts for this flow
            int flow_src = vtemp[0];
            int flow_dst = vtemp[1];

            TcpSrc* flowSrc = new TcpSrc(NULL, &traffic_logger, eventlist, flow_src, flow_dst);
            TcpSink* flowSnk = new TcpSink();
            flowSrc->set_flowsize(vtemp[2]); // bytes
            flowSrc->set_ssthresh(ssthresh*Packet::data_packet_size());
            flowSrc->_rto = timeFromMs(1);
            
            tcpRtxScanner.registerTcp(*flowSrc);

            Route* routeout, *routein;

            int choice = 0;
            vector<const Route*>* srcpaths = top->get_paths(flow_src, flow_dst);
            choice = rand()%srcpaths->size(); // comment this out if we want to use the first path
            routeout = new Route(*(srcpaths->at(choice)));
            routeout->push_back(flowSnk);

            choice = 0;
            vector<const Route*>* dstpaths = top->get_paths(flow_dst, flow_src);
            choice = rand()%dstpaths->size(); // comment this out if we want to use the first path
            routein = new Route(*(dstpaths->at(choice)));
            routein->push_back(flowSrc);

            flowSrc->connect(*routeout, *routein, *flowSnk, timeFromNs(vtemp[3]/1.));

#ifdef PACKET_SCATTER
            flowSrc->set_paths(srcpaths);
            flowSnk->set_paths(dstpaths);
#endif
            
            sinkLogger.monitorSink(flowSnk);

        }
    }


    UtilMonitor* UM = new UtilMonitor(top, eventlist);
    UM->start(timeFromSec(utiltime));


    // Record the setup
    int pktsize = Packet::data_packet_size();
    logfile.write("# pktsize=" + ntoa(pktsize) + " bytes");
    //logfile.write("# subflows=" + ntoa(subflow_count));
    logfile.write("# hostnicrate = " + ntoa(HOST_NIC) + " pkt/sec");
    logfile.write("# corelinkrate = " + ntoa(HOST_NIC*CORE_TO_HOST) + " pkt/sec");
    //logfile.write("# buffer = " + ntoa((double) (queues_na_ni[0][1]->_maxsize) / ((double) pktsize)) + " pkt");
    //double rtt = timeAsSec(timeFromUs(RTT));
    //logfile.write("# rtt =" + ntoa(rtt));

    // GO!
    while (eventlist.doNextEvent()) {
    }
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
