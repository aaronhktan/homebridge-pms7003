CC = gcc
CFLAGS = -Wall -std=gnu99
LD = gcc
LDFLAGS = -g -std=gnu99
LDLIBS = 

DEBUGFLAG = 0

SRCS = pms7003-cli.c pms7003.c
OBJS = pms7003-cli.o pms7003.o
TARGETS = pms7003-cli debug

debug: CFLAGS += -DDEBUG -g

all: ${TARGETS}

pms7003-cli: $(OBJS)
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS)

debug: $(OBJS)
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS)

%.o: %.c 
	$(CC) $(CFLAGS) -c $< 

%.d: %.c
	gcc -MM -MF $@ $<

-include $(SRCS:.c=.d)

.PHONY: clean
clean:
	rm -f *~ *.d *.o $(TARGETS) 
