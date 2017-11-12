CC=clang
CXX=clang++

ARNOLD_PATH=/Users/zeno/Arnold-5.0.2.0-darwin

CXXFLAGS=-std=c++11 -Wall -O3 -shared -fPIC -I${ARNOLD_PATH}/include
LDFLAGS=-L${ARNOLD_PATH}/bin -lai

HEADERS=\

.PHONY=all clean

all: zoic

zoic: Makefile src/zoic.cpp ${HEADERS}
	${CXX} ${CXXFLAGS} src/zoic.cpp -o bin/zoic.dylib ${LDFLAGS}

clean:
	rm -f zoic
