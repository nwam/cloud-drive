CC = gcc
CFLAGS = -ggdb -O0 -std=gnu11 -lz -lhfs

all: client clean

client: client.o socketutils.o udp_sockets.o udp_client.o
	$(CC) -o client client.o socketutils.o udp_sockets.o udp_client.o $(CFLAGS)

client.o: client.c client.h
	$(CC) -c client.c $(CFLAGS)

socketutils.o: ../common/socketutils.c ../common/socketutils.h
	$(CC) -c ../common/socketutils.c $(CFLAGS)

udp_sockets.o: ../common/udp_sockets.c ../common/udp_sockets.h
	$(CC) -c ../common/udp_sockets.c $(CFLAGS)

udp_client.o: ../common/udp_client.c ../common/udp_client.h ../common/udp_sockets.h
	$(CC) -c ../common/udp_client.c $(CFLAGS)

clean:
	rm -f *.o *.a
