CXX=g++
FLAG=-std=c++11 -g -Wall
DEP_INCLUDE=-I../deps/include
DEP_LIB=-L../deps/lib -lavformat -lavcodec -lavutil -Wl,-rpath-link=../deps/lib

SOURCE = $(wildcard *.cc)
OBJS = $(patsubst %.cc,%.o,${SOURCE})
TARGETS = $(patsubst %.o,%,${OBJS})

$(info $(OBJS))
$(info $(TARGETS))

all:${TARGETS}

${TARGETS}:%:%.o
	$(CXX) $(FLAG) -o $@ $< $(DEP_LIB)

%.o:%.cc
	$(CXX) $(FLAG) $(DEP_INCLUDE) -c -o $@ $<

.PHONY:clean
clean:
	rm -rf *.o $(TARGETS)

