CC = gcc
CFLAGS = -g -O0 -std=gnu11 -Wall -Werror

all: hmds clean

hmds: hmds.o hdb.o socketutils.o
	$(CC) -o hmds hmds.o hdb.o socketutils.o $(CFLAGS) -lhiredis

hmds.o: hmds.c hmds.h ../hdb/hdb.h
	$(CC) -c hmds.c $(CFLAGS)

socketutils.o: ../common/socketutils.c ../common/socketutils.h
	$(CC) -c ../common/socketutils.c $(CFLAGS)

hdb.o: ../hdb/hdb.c ../hdb/hdb.h
	$(CC) -c ../hdb/hdb.c $(CFLAGS)

clean:
	rm -f *.o *.a
