CC=clang
OPTIONS=-c
SOURCES=informer.c list.c tracker.c configuration.c syncdaemon.c 
OBJS=$(SOURCES:.c=.o)
EXE=SyncDaemon

all:
	$(CC) $(OPTIONS) $(SOURCES)
	$(CC) -pthread $(OBJS) -o $(EXE)

debug:
	$(CC) $(OPTIONS) -g -DDEBUG $(SOURCES)
	$(CC) -pthread $(OBJS) -o $(EXE)

clean:
	rm $(OBJS) $(EXE)

trace:	debug
	valgrind --leak-check=yes --track-origins=yes ./$(EXE)
