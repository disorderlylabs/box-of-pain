SRC=src

# makefile variables
CXXFLAGS=-Wall -Wextra -O3 -g -I$(SRC) -include defl.h -std=gnu++11
CXX=g++

SOURCES_RELATIVE=painbox.cpp dump.cpp rfollow.cpp helper.cpp run.cpp socket.cpp scnames.cpp \
                 $(addprefix sysimp/, read.cpp write.cpp accept.cpp connect.cpp recvfrom.cpp sendto.cpp clone.cpp)

SOURCES=$(addprefix $(SRC)/, $(SOURCES_RELATIVE))
DEPS=$(SOURCES:.cpp=.d)
OBJECTS=$(SOURCES:.cpp=.o)

LIBS=-lstdc++

all: painbox examples

painbox:$(OBJECTS)
	$(CXX) -o painbox $(OBJECTS) $(LIBS)

$(SRC)/%.o: $(SRC)/%.cpp 
	$(CXX) $(CXXFLAGS) -c -o $@ $< -MD

.PHONY: examples clean
examples:
	@$(MAKE) -sC examples

-include $(DEPS)

clean:
	-rm -f painbox *.m4 *.pdf *.inc *.dot $(DEPS) $(OBJECTS)
	-$(MAKE) -sC examples ARGS=clean