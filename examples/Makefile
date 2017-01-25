
EXAMPLES = basic group_hierarchy

GLO_INCLUDE = ../include

CXXFLAGS = -g -I$(GLO_INCLUDE) -std=c++14 -Wall -finput-charset=utf-8

LDFLAGS = -lpthread

.PHONY: clean all

all: $(EXAMPLES)

%: %.cpp $(GLO_INCLUDE)/glo/common.hpp $(GLO_INCLUDE)/glo/status_group.hpp $(GLO_INCLUDE)/glo/http_status_server.hpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(EXAMPLES)
