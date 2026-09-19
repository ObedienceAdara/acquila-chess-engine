CXX ?= g++
CXXFLAGS ?= -std=c++17 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -march=native
LDFLAGS ?=
TARGET := aquila
PORTABLE := aquila-portable
SRC := src/main.cpp

all: $(TARGET) $(PORTABLE)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $@ $(LDFLAGS)

$(PORTABLE): $(SRC)
	$(CXX) -std=c++17 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -march=x86-64 -mtune=generic $(SRC) -o $@ $(LDFLAGS)

asan:
	$(CXX) -std=c++17 -O1 -g -fsanitize=address,undefined -Wall -Wextra $(SRC) -o $(TARGET)-asan

test: $(TARGET)
	bash ./tests/perft_suite.sh

clean:
	rm -f $(TARGET) $(PORTABLE) $(TARGET)-asan

.PHONY: all asan test clean
