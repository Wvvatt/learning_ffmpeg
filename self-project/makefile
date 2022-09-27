FLAG=-g 
INCLUDE=-I../deps/include
DEP_LIB=-L../deps/lib -lavformat -lavcodec -lavutil -Wl,-rpath-link=../deps/lib
CPPSRC=$(foreach TMP_SRC_DIRS, ., $(wildcard $(TMP_SRC_DIRS)/*.cc))
CPPOBJ:=$(CPPSRC:.cc=.o)

all:decoder

decoder:decoder.o
	g++ -std=c++11 $^ $(FLAG) $(DEP_LIB)  -o $@

%.o:%.cc
	g++ -std=c++11 $(FLAG) $(INCLUDE) -c $< -o $@

.PHONY:clean
clean:
	rm -rf decoder *.o

