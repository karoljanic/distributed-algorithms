CC=g++
CFLAGS=-Wall -Wextra -O3


all: generate_graph flooding bfs flooding_with_termination_detection

generate_graph: generate_graph.cpp
	$(CC) $(CFLAGS) -o generate_graph generate_graph.cpp

flooding: flooding.cpp graph.hpp simulator.hpp
	$(CC) $(CFLAGS) -o flooding flooding.cpp

bfs: bfs.cpp graph.hpp simulator.hpp
	$(CC) $(CFLAGS) -o bfs bfs.cpp

flooding_with_termination_detection: flooding_with_termination_detection.cpp graph.hpp simulator.hpp
	$(CC) $(CFLAGS) -o flooding_with_termination_detection flooding_with_termination_detection.cpp

clean:
	rm -f generate_graph flooding bfs flooding_with_termination_detection