CXXFLAGS=-std=c++1z -Wall -Wextra -Og -g
CXX=g++

CFLAGS=-std=gnu11 -Wall -Wextra -Og -g
CC=gcc

SOURCES=painbox.cpp helper.cpp socket.cpp
DEPS=$(SOURCES:.cpp=.d)
OBJECTS=$(SOURCES:.cpp=.o)

all: painbox client server

painbox: $(OBJECTS)
	$(CXX) -o painbox $(OBJECTS) -lstdc++

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $< -MD

client: client.c

server: server.c

-include $(DEPS)

clean:
	-rm -f $(OBJECTS) $(DEPS) painbox client server
