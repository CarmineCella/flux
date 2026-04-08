CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic -Wno-misleading-indentation
PREFIX   ?= /usr/local
FLUXDIR  ?= $(HOME)/.flux

flux: flux.cpp flux.h
	$(CXX) $(CXXFLAGS) -o $@ flux.cpp

test: flux
	@./flux test_core.flux && ./flux test_stdlib.flux

install: flux
	install -m 755 flux $(PREFIX)/bin/flux
	mkdir -p $(FLUXDIR)
	install -m 644 stdlib.flux $(FLUXDIR)/stdlib.flux

uninstall:
	rm -f $(PREFIX)/bin/flux
	rm -rf $(FLUXDIR)

clean:
	rm -f flux

.PHONY: test install uninstall clean
