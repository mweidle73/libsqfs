CFLAGS += -O2 -Wall -g
CPPFLAGS += -g -D_GNU_SOURCE -Iinclude

all:

doc:
	doxygen doxygen.conf

clean:
	rm -rf doc $(EXECUTABLES)

include examples/Makefile.sub

all: $(EXECUTABLES)

.PHONY: doc
