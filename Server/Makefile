# Parameters
CC = g++
CFLAGS = -c -Wall
SRCS = server.cpp
OBJS = $(SRCS:.cpp=.o)
EXEC = server

# Dependency
all: $(SRCS) $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(OBJS) -o $@

.cpp.o:
	$(CC) $(CFLAGS) -o $@ $<

clean:
	@rm -rf $(EXEC) $(OBJS) *~
