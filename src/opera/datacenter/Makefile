CC = g++-7
CFLAGS = -Wall -ggdb -O
CRT=`pwd`
INCLUDE= -I/$(CRT)/.. -I$(CRT) 
LIB=-L..


all:	htsim_ndp_dynexpTopology


htsim_ndp_dynexpTopology: main_ndp_dynexpTopology.o ../libhtsim.a dynexp_topology.o
	$(CC) $(CFLAGS) main_ndp_dynexpTopology.o dynexp_topology.o $(LIB) -lhtsim -o htsim_ndp_dynexpTopology

main_ndp_dynexpTopology.o: main_ndp_dynexpTopology.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndp_dynexpTopology.cpp 

dynexp_topology.o: dynexp_topology.cpp dynexp_topology.h topology.h
	$(CC) $(INCLUDE) $(CFLAGS) -c dynexp_topology.cpp

clean:	
	rm -f *.o htsim_ndp* htsim_tcp* htsim_dctcp*
