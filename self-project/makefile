CXX=g++
CFLAG=-std=c++11 -g -Wall -fPIE
LDFLAG=
DEP_INCLUDE=-I../build/include
DEP_LIB=-lpthread -L../build/lib -lavformat -lavcodec -lavutil -Wl,-rpath-link=../build/lib:../x264/lib

SOURCE = $(wildcard *.cc)
OBJS = $(patsubst %.cc,%.o,${SOURCE})
TARGETS = $(patsubst %.o,%,${OBJS})

$(info $(OBJS))
$(info $(TARGETS))

all:${TARGETS}

${TARGETS}:%:%.o
	$(CXX) $(LDFLAG) -o $@ $< $(DEP_LIB)
	rm *.o -f

%.o:%.cc
	$(CXX) $(CFLAG) $(DEP_INCLUDE) -c -o $@ $<

.PHONY:clean
clean:
	rm -rf *.o $(TARGETS)
