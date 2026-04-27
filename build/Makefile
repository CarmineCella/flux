CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic
PREFIX   ?= /usr/local
FLUXDIR  ?= $(HOME)/.flux

# Auto-detect GNU readline. If <readline/readline.h> is available we compile
# with -DFLUX_USE_READLINE and link -lreadline; the REPL then gets line
# editing, persistent history (~/.flux_history), Ctrl-R reverse-search, and
# completion hooks. Without readline the REPL falls back to plain getline()
# and still works — just less pleasantly.
#
# To force off:   make READLINE=0
# To force on:    make READLINE=1
HAS_READLINE := $(shell printf '\#include <readline/readline.h>\nint main(){return 0;}\n' \
                  | $(CXX) -x c++ -E - >/dev/null 2>&1 && echo 1 || echo 0)

READLINE ?= $(HAS_READLINE)
ifeq ($(READLINE),1)
  RL_FLAGS := -DFLUX_USE_READLINE
  RL_LIBS  := -lreadline
endif

flux: ../src/flux.cpp ../src/flux.h ../src/dsp.h ../src/sampsynth.h
	$(CXX) $(CXXFLAGS) $(RL_FLAGS) -o $@ ../src/flux.cpp $(RL_LIBS)

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
