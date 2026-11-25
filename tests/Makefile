CXX = g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
CPPFLAGS += -I../engine

TESTS := $(patsubst %.cpp,build/%,$(wildcard test_*.cpp))
HEADERS := $(wildcard ../engine/*.hpp ../engine/*/*.hpp)

run: $(TESTS)
	@set -e; for test in $(TESTS); do echo "== $$test"; ./$$test; done

build/%: %.cpp test_main.hpp $(HEADERS)
	@mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ $<

clean:
	rm -rf build

.PHONY: run clean
