CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic 
PREFIX   ?= /usr/local
FLUXDIR  ?= $(HOME)/.flux

flux: ../src/flux.cpp ../src/flux.h ../src/dsp.h
	$(CXX) $(CXXFLAGS) -o $@ ../src/flux.cpp

test: flux
	@./flux ../tests/test_core.flux && ./flux ../tests/test_stdlib.flux && ./flux ../tests/test_dsp.flux

install: flux
	install -m 755 flux $(PREFIX)/bin/flux
	mkdir -p $(FLUXDIR)
	install -m 644 ../src/stdlib.flux $(FLUXDIR)/stdlib.flux
	install -m 644 ../src/dsp.flux $(FLUXDIR)/dsp.flux
	install -m 644 ../src/sampsynth.flux $(FLUXDIR)/sampsynth.flux

uninstall:
	rm -f $(PREFIX)/bin/flux
	rm -rf $(FLUXDIR)

clean:
	rm -f flux

.PHONY: test install uninstall clean
