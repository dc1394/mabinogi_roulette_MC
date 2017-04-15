PROG := mabinogi_roulette_mc
SRCS :=	checkpoint.cpp mabinogi_roulette_mc.cpp

OBJS :=	$(SRCS:%.cpp=%.o)
DEPS :=	$(SRCS:%.cpp=%.d)

VPATH  = src/checkpoint src/mabinogi_roulette_MC src/mabinogi_roulette_MC/myrandom
CXX = clang++
CXXFLAGS = -Wextra -O3 -pipe -std=c++14
LDFLAGS = -L/home/dc1394/oss/tbb2017_20170226oss/lib/intel64/gcc4.7 -ltbb

all: $(PROG) ;
#rm -f $(OBJS) $(DEPS)

-include $(DEPS)

$(PROG): $(OBJS)
		$(CXX) $(LDFLAGS) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
		$(CXX) $(CXXFLAGS) -c -MMD -MP -D_CHECK_PARALELL_PERFORM $<

clean:
		rm -f $(PROG) $(OBJS) $(DEPS)
