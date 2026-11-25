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

WRAPPERS := $(wildcard ../src/[A-Z]*.cpp)
build/wrappers.hpp: extract_modules.py $(WRAPPERS)
	@mkdir -p build
	python3 extract_modules.py $(WRAPPERS) > $@.tmp
	mv $@.tmp $@

build/test_robustness: build/wrappers.hpp rack_shim.hpp $(wildcard ../src/*.hpp)
