CXX=g++
FLAG=-std=c++11 -g -Wall
DEP_LIB=

SOURCE = $(wildcard *.cc)
OBJS = $(patsubst %.cc,%.o,${SOURCE})
TARGETS = $(patsubst %.o,%,${OBJS})

$(info $(OBJS))
$(info $(TARGETS))

all:${TARGETS}

${TARGETS}:%:%.o
	$(CXX) $(FLAG) -o $@ $< $(DEP_LIB)
	rm *.o -f

%.o:%.cc
	$(CXX) $(FLAG) $(DEP_INCLUDE) -c -o $@ $<

.PHONY:clean
clean:
	rm -rf *.o $(TARGETS)
