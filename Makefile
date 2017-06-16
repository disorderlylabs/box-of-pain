CXXFLAGS=-std=c++1z -Wall -Wextra -Og -g
CXX=g++

SOURCES=painbox.cpp helper.cpp
DEPS=$(SOURCES:.cpp=.d)
OBJECTS=$(SOURCES:.cpp=.o)

all: painbox

painbox: $(OBJECTS)
	$(CXX) -o painbox $(OBJECTS) -lstdc++

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $< -MD



-include $(DEPS)

clean:
	-rm -f $(OBJECTS) $(DEPS) painbox
