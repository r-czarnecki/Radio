TARGET: radio-proxy radio-client

CC	= g++
CFLAGS	= -Wall -Wextra -O2
LFLAGS	= -lpthread

nk-server-tcp.o nk-client-tcp.o: err.h

obj/%.o: src/%.cpp
	$(CC) -c $(CFLAGS) $< -o $@

radio-proxy: obj/argHolder.o  obj/err.o  obj/radio.o  obj/receiver.o obj/proxy.o
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

radio-client: obj/argHolder.o  obj/communicator.o  obj/err.o  obj/telnetMenu.o obj/client.o
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

.PHONY: clean TARGET
clean:
	rm -f radio-proxy radio-client obj/*.o *~ *.bak
