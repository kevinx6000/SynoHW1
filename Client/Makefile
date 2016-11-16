# Parameters
CC = g++
CFLAGS = -c -Wall -std=c++11
SRCS = main.cpp client.cpp
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
