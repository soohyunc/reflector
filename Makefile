# Generated automatically from Makefile.in by configure.
CC = gcc
CFLAGS = -ggdb
INCLUDE =   
LIB = -ldl -lnsl -lsocket  -ltk8.0 -L/usr/local/lib -ltcl8.0 -lXext -lX11 -lm

reflector: reflector.o ui.o queue.o ui-controller.o stats.o
	$(CC) -o $@ reflector.o ui.o queue.o ui-controller.o stats.o $(LIB) 

stats.o: stats.c
	$(CC) $(CFLAGS) -c $(INCLUDE) stats.c

reflector.o: reflector.c
	$(CC) $(CFLAGS) -c $(INCLUDE) reflector.c

queue.o: queue.c
	$(CC) $(CFLAGS) -c $(INCLUDE) queue.c

ui-controller.o: ui-controller.c
	$(CC) $(CFLAGS) -c $(INCLUDE) ui-controller.c

ui.o: ui.c
	$(CC) $(CFLAGS) -c $(INCLUDE) ui.c

ui.c: tcl2c ui.tcl
	tcl2c tclscript < ui.tcl > ui.c

tcl2c:  tcl2c.c
	$(CC) -o $@ tcl2c.c

CLEAN = *.o reflector tcl2c ui.c config.log config.cache core reflector.core
clean:
	rm -rf $(CLEAN)
