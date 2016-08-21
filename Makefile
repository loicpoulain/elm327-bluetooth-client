LDFLAGS = -lbluetooth

all: elm-bt.o

elm-bt.o: elm-bt.c
	$(CC) $(CFLAGS) elm-bt.c -o elm-bt $(LDFLAGS)

clean:
	rm -f *.o elm-bt

install: all
	cp elm-bt $(DESTDIR)/bin/elm-bt

uninstall:
	rm -f $(DESTDIR)/bin/elm-bt
