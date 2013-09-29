CXXFLAGS := -g -Wall -lm
CXX=c++

all: cachesim

cachesim: cache.o cachesim.o cachesim_driver.o
	$(CXX) -o cachesim cachesim.o cachesim_driver.o

clean:
	rm -f cachesim *.o
