DEPS = raylib
CFLAGGS_DEPS = $(shell pkg-config --cflags $(DEPS))
LDLIBS += $(shell pkg-config --libs $(DEPS))

SRC = main.c


gamit: $(SRC)
	$(CC) $(CFLAGS) $(CFLAGGS_DEPS) -o $@ $(SRC) $(LDLIBS)

