CFLAGS += -O2 -Wall -g
CPPFLAGS += -g -D_GNU_SOURCE -Iinclude
LDFLAGS += -g

all:

doc:
	doxygen doxygen.conf

clean:
	rm -rf doc $(EXECUTABLES)
	find -name "*.la" -o -name "*.lo" -o -name "*.o" | xargs rm -rf

include src/Makefile.sub
include examples/Makefile.sub
include tests/Makefile.sub

all: $(LIBRARIES) $(EXECUTABLES)

# build rules

%.la: %.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) -o $@ $^

%.lo: %.c
	$(CC) -c -fPIC $(CFLAGS) -DPIC $(CPPFLAGS) -o $@ $^

%.a:
	rm -f $@ ; ar clqv $@ $^ ; ranlib $@

%.so:
	$(CC) -shared -o $@ $^

$(EXECUTABLES):
	$(CC) -o $@ $^ $(LDFLAGS)

.PHONY: doc
