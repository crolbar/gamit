DEPS = raylib libcurl
CFLAGGS_DEPS = $(shell pkg-config --cflags $(DEPS)) -DCURL_STATICLIB
LDLIBS_WIN += $(shell pkg-config --libs --static $(DEPS)) -lopengl32 -lgdi32 -lwinmm
LDLIBS += $(shell pkg-config --libs $(DEPS))

SRC = main.c
SRC_SERVER = server.c

BINS = gamit gamit-server

all: $(BINS)
win: gamit.exe

gamit: $(SRC)
	$(CC) $(CFLAGS) $(CFLAGGS_DEPS) -o $@ $(SRC) $(LDLIBS)

gamit.exe: $(SRC)
	$(CC) $(CFLAGS) $(CFLAGGS_DEPS) -o $@ -static $(SRC) $(LDLIBS_WIN)

gamit-server: $(SRC)
	$(CC) $(CFLAGS) $(CFLAGGS_DEPS) -o $@ $(SRC_SERVER) $(LDLIBS)


run: $(BINS)
	./run.sh

.PHONY: all $(BINS) win gamit.exe
