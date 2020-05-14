# bin and src directories
BIN=bin
SRC=src

CXXFLAGS=-Wall -Wextra -O3 -g -I$(SRC) -include defl.h -std=gnu++11
CXX=g++

#note: add back in prefixing AND flat bin structure somehow?
SOURCES=painbox.cpp dump.cpp rfollow.cpp helper.cpp run.cpp socket.cpp scnames.cpp \
 		$(addprefix sysimp/, read.cpp write.cpp accept.cpp connect.cpp recvfrom.cpp sendto.cpp clone.cpp)

DEPS=$(SOURCES:.cpp=.d) 
OBJECTS=$(SOURCES:.cpp=.o) 

LIBS=-lstdc++

all: bin painbox examples

bin:
	-@mkdir -p $(BIN)

painbox: $(addprefix $(BIN)/,$(OBJECTS))
	$(CXX) -o painbox $(addprefix $(BIN)/,$(OBJECTS)) $(LIBS)

$(BIN)/%.o: $(SRC)/%.cpp 
	# create directory in bin (if it doesn't exist)
	-@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $< -MD

.PHONY: examples
examples:
	@$(MAKE) -sC examples

clean:
	-rm -f painbox *.m4 *.pdf *.inc *.dot
	-rm -rf $(BIN)/*
	-@make -sC examples clean