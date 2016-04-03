CC = gcc
CFLAGS = -g -O0 -std=gnu11 

all: hftpd clean

hftpd: hftpd.o socketutils.o udp_sockets.o udp_server.o hdb.o
	$(CC) -o hftpd hftpd.o socketutils.o udp_sockets.o udp_server.o hdb.o $(CFLAGS) -lhiredis

hftpd.o: hftpd.c hftpd.h
	$(CC) -c hftpd.c $(CFLAGS)

socketutils.o: ../common/socketutils.c ../common/socketutils.h
	$(CC) -c ../common/socketutils.c $(CFLAGS)

udp_sockets.o: ../common/udp_sockets.c ../common/udp_sockets.h
	$(CC) -c ../common/udp_sockets.c $(CFLAGS)

udp_server.o: ../common/udp_server.c ../common/udp_server.h ../common/udp_sockets.h
	$(CC) -c ../common/udp_server.c $(CFLAGS)

hdb.o: ../hdb/hdb.c ../hdb/hdb.h
	$(CC) -c ../hdb/hdb.c $(CFLAGS)


clean:
	rm -f *.o *.a
