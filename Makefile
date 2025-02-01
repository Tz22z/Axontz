# MemPlumber Memory Allocator
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -g
BINDIR = bin

.PHONY: all clean

all:
	@echo "MemPlumber project initialized"
	@echo "Build system ready for implementation"

clean:
	rm -rf $(BINDIR)/*

$(BINDIR):
	mkdir -p $(BINDIR)
