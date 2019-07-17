CXXFLAGS=-Wall -Wextra -O3 -g -I. -include defl.h -std=gnu++11
CXX=g++

CFLAGS=-std=gnu11 -Wall -Wextra -Og -g
CC=gcc

#CXXFLAGS+=-fsanitize=address -fsanitize=undefined
#LIBS+=-lasan -lubsan

LIBS=-lstdc++

SOURCES=painbox.cpp dump.cpp rfollow.cpp helper.cpp run.cpp socket.cpp $(addprefix sysimp/,read.cpp write.cpp accept.cpp connect.cpp recvfrom.cpp sendto.cpp clone.cpp) scnames.cpp
DEPS=$(SOURCES:.cpp=.d)
OBJECTS=$(SOURCES:.cpp=.o)

EXAMPLES=client server quorum_server rdlog_sender rdlog_receiver simplog_sender
EXAMPLES_SRC=$(addsuffix .c,$(EXAMPLES))

all: painbox $(EXAMPLES)

painbox: $(OBJECTS)
	$(CXX) -o painbox $(OBJECTS) $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $< -MD

%: %.c
	$(CC) $(CFLAGS) -o $@ $< -MD

-include $(DEPS) $(EXAMPLES_SRC:.c=.d)

clean:
	-rm -f $(OBJECTS) $(DEPS) painbox client server $(EXAMPLES) *.m4 *.pdf *.inc *.dot $(EXAMPLES_SRC:.c=.d)
