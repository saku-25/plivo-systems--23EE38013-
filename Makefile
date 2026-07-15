CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra
TARGETS = sender receiver

all: $(TARGETS)

sender: sender.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

receiver: receiver.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -f $(TARGETS)