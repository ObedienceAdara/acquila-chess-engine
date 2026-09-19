CXX ?= g++
CXXFLAGS ?= -std=c++17 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -march=native
LDFLAGS ?=
TARGET := aquila
PORTABLE := aquila-portable
CORE_TEST := aquila-core-tests
SRC := src/main.cpp
TEST_CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic

all: $(TARGET) $(PORTABLE)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $@ $(LDFLAGS)

$(PORTABLE): $(SRC)
	$(CXX) -std=c++17 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -march=x86-64 -mtune=generic $(SRC) -o $@ $(LDFLAGS)

asan:
	$(CXX) -std=c++17 -O1 -g -fsanitize=address,undefined -Wall -Wextra $(SRC) -o $(TARGET)-asan

$(CORE_TEST): tests/core_regression.cpp $(SRC)
	$(CXX) $(TEST_CXXFLAGS) tests/core_regression.cpp -o $@

test: $(TARGET) $(CORE_TEST)
	bash ./tests/perft_suite.sh
	./$(CORE_TEST)
	python3 tests/uci_regression.py ./$(TARGET)

clean:
	rm -f $(TARGET) $(PORTABLE) $(TARGET)-asan $(CORE_TEST)

.PHONY: all asan test clean
