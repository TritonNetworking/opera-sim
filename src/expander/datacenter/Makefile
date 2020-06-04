CC = g++-7
CFLAGS = -Wall -g -O
CRT=`pwd`
INCLUDE= -I/$(CRT)/.. -I$(CRT) 
#-I$(CRT)/ksp -I$(CRT)/ksp/boost
LIB=-L.. 
#-Lksp

all:	htsim_ndp_expander

htsim_ndp_expander: main_ndp_expander.o ../libhtsim.a expander_topology.o
	$(CC) $(CFLAGS) main_ndp_expander.o expander_topology.o $(LIB) -lhtsim -o htsim_ndp_expander

main_ndp_expander.o: main_ndp_expander.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndp_expander.cpp 

expander_topology.o: expander_topology.cpp expander_topology.h topology.h
	$(CC) $(INCLUDE) $(CFLAGS) -c expander_topology.cpp

clean:	
	rm -f *.o htsim_ndp* htsim_tcp* htsim_dctcp*
