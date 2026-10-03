DEPS = raylib
CFLAGGS_DEPS = $(shell pkg-config --cflags $(DEPS))
LDLIBS += $(shell pkg-config --libs $(DEPS))

SRC = main.c
SRC_SERVER = server.c

BINS = gamit gamit-server

all: $(BINS)

gamit: $(SRC)
	$(CC) $(CFLAGS) $(CFLAGGS_DEPS) -o $@ $(SRC) $(LDLIBS)

gamit-server: $(SRC)
	$(CC) $(CFLAGS) $(CFLAGGS_DEPS) -o $@ $(SRC_SERVER) $(LDLIBS)


run: $(BINS)
	./run.sh

.PHONY: all $(BINS)
