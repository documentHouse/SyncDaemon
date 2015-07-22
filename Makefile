CC=clang
OPTIONS=-c
SOURCES=informer.c list.c daemonize.c
OBJS=$(SOURCES:.c=.o)
EXE=Daemonize

all:
	$(CC) $(OPTIONS) $(SOURCES)
	$(CC) -pthread $(OBJS) -o $(EXE)

debug:
	$(CC) $(OPTIONS) -g -DDEBUG $(SOURCES)
	$(CC) -pthread $(OBJS) -o $(EXE)

clean:
	rm $(OBJS) $(EXE)
