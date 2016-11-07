# Parameters
CC = g++
CFLAGS = -c -Wall
SRCS = client.cpp
OBJS = $(SRCS:.cpp=.o)
EXEC = client

# Dependency
all: $(SRCS) $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(OBJS) -o $@

.cpp.o:
	$(CC) $(CFLAGS) -o $@ $<

clean:
	@rm -rf $(EXEC) $(OBJS) *~
