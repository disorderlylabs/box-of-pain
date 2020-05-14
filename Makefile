

#CXXFLAGS+=-fsanitize=address -fsanitize=undefined
#LIBS+=-lasan -lubsan

LIBS=-lstdc++

# bin and src directories
BIN=bin
SRC=src

CXXFLAGS=-Wall -Wextra -O3 -g -I$(SRC) -include defl.h -std=gnu++11
CXX=g++

SOURCES=painbox.cpp dump.cpp rfollow.cpp helper.cpp run.cpp socket.cpp scnames.cpp \
		$(addprefix sysimp/, read.cpp write.cpp accept.cpp connect.cpp recvfrom.cpp sendto.cpp clone.cpp)

DEPS=$(SOURCES:.cpp=.d)
OBJECTS=$(SOURCES:.cpp=.o)

#EXAMPLES=client server quorum_server rdlog_sender rdlog_receiver simplog_sender quorum_server_thrd primary p_client
#EXAMPLES_SRC=$(addsuffix .c,$(EXAMPLES))

all: $(BIN)/painbox examples

$(BIN)/painbox: $(addprefix $(BIN)/,$(OBJECTS))
	$(CXX) -o $(BIN)/painbox $(addprefix $(BIN)/,$(notdir $(OBJECTS))) $(LIBS)

$(BIN)/%.o: $(SRC)/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $(BIN)/$(notdir $@)  $< -MD

examples:
	echo "examples TODO"

#quorum_server_thrd: qs_thrd.c
#	$(CC) $(CFLAGS) -o $@ $< -lpthread

#-include $(DEPS) $(EXAMPLES_SRC:.c=.d)

# removed EXAMPLES and $(EXAMPLES_SRC:.c=.d) from clean
clean:
	-rm -f $(OBJECTS) $(DEPS) painbox client server *.m4 *.pdf *.inc *.dot 
