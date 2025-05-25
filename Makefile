CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -lgpiod -lm

TARGETS = i2c_vl53l0x_master vl53l0x_slave

all: $(TARGETS)

i2c_vl53l0x_master: i2c_vl53l0x_master.c soft_i2c.c soft_i2c.h
	$(CC) $(CFLAGS) -o i2c_vl53l0x_master i2c_vl53l0x_master.c soft_i2c.c $(LDFLAGS)

vl53l0x_slave: vl53l0x_slave.c soft_i2c.c soft_i2c.h
	$(CC) $(CFLAGS) -o vl53l0x_slave vl53l0x_slave.c soft_i2c.c $(LDFLAGS)

clean:
	rm -f $(TARGETS) *.o

.PHONY: all clean