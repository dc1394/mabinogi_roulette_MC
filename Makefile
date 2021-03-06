PROG := mabinogi_roulette_mc
SRCS :=	checkpoint.cpp goexit.cpp mabinogi_roulette_mc.cpp SFMT.c

OBJS = checkpoint.o goexit.o mabinogi_roulette_mc.o SFMT.o
DEPS = checkpoint.d goexit.d mabinogi_roulette_mc.d SFMT.d

VPATH  = src/checkpoint src/mabinogi_roulette_MC src/mabinogi_roulette_MC/myrandom \
		 src/mabinogi_roulette_MC/goexit src/SFMT-src-1.5.1
CC = gcc
CFLAGS = -Wall -Wextra -O3 -mtune=native -march=native -pipe 
CXX = g++
CXXFLAGS = -Wall -Wextra -O3 -mtune=native -march=native -pipe -std=c++17
LDFLAGS = -L/home/dc1394/oss/tbb/lib/intel64/gcc4.8 -ltbb

all: $(PROG) ;
#rm -f $(OBJS) $(DEPS)

$(PROG): $(OBJS)
		$(CXX) $^ $(CXXFLAGS) $(LDFLAGS) -o $@

%.o: %.c
		$(CC) $(CFLAGS) -c -MMD -MP -msse2 -DHAVE_SSE2 -DSFMT_MEXP=19937 $<

%.o: %.cpp
		$(CXX) $(CXXFLAGS) -c -MMD -MP -msse2 -DHAVE_SSE2 -DSFMT_MEXP=19937 -D_CHECK_PARALELL_PERFORM $<

clean:
		rm -f $(PROG) $(OBJS) $(DEPS) result/*.csv
