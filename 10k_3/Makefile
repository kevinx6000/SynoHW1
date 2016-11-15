# Parameters
CC = g++
CFLAGS = -c -Wall
SRCS = main.cpp
OBJS = $(SRCS:.cpp=.o)
EXEC = main

# Dependency
all: $(SRCS) $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(OBJS) -o $@ -lpthread

.cpp.o:
	$(CC) $(CFLAGS) -o $@ $<

clean:
	@rm -rf $(EXEC) $(OBJS) *~
