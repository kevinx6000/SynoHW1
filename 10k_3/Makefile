# Parameters
CC = g++
CFLAGS = -c -Wall
SRCS = main.cc
OBJS = $(SRCS:.cc=.o)
EXEC = main

# Dependency
all: $(SRCS) $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(OBJS) -o $@ -lpthread

.cc.o:
	$(CC) $(CFLAGS) -o $@ $<

clean:
	@rm -rf $(EXEC) $(OBJS) *~
