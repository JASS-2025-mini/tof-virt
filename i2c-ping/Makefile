CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -lgpiod

all: i2c_master i2c_slave

i2c_master: i2c_master.c soft_i2c.c soft_i2c.h
	$(CC) $(CFLAGS) -o i2c_master i2c_master.c soft_i2c.c $(LDFLAGS)

i2c_slave: i2c_slave.c soft_i2c.c soft_i2c.h
	$(CC) $(CFLAGS) -o i2c_slave i2c_slave.c soft_i2c.c $(LDFLAGS)

clean:
	rm -f i2c_master i2c_slave *.o

.PHONY: all clean