CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -lgpiod -lm

TARGETS = i2c_master i2c_slave i2c_tof_sim i2c_vl53l0x_master simple_master simple_slave test_ping

all: $(TARGETS)

i2c_master: i2c_master.c soft_i2c.c soft_i2c.h
	$(CC) $(CFLAGS) -o i2c_master i2c_master.c soft_i2c.c $(LDFLAGS)

i2c_slave: i2c_slave.c soft_i2c.c soft_i2c.h
	$(CC) $(CFLAGS) -o i2c_slave i2c_slave.c soft_i2c.c $(LDFLAGS)

i2c_tof_sim: i2c_tof_sim.c soft_i2c.c soft_i2c.h
	$(CC) $(CFLAGS) -o i2c_tof_sim i2c_tof_sim.c soft_i2c.c $(LDFLAGS)

i2c_vl53l0x_master: i2c_vl53l0x_master.c soft_i2c.c soft_i2c.h
	$(CC) $(CFLAGS) -o i2c_vl53l0x_master i2c_vl53l0x_master.c soft_i2c.c $(LDFLAGS)

simple_master: simple_master.c soft_i2c.c soft_i2c.h
	$(CC) $(CFLAGS) -o simple_master simple_master.c soft_i2c.c $(LDFLAGS)

simple_slave: simple_slave.c soft_i2c.c soft_i2c.h
	$(CC) $(CFLAGS) -o simple_slave simple_slave.c soft_i2c.c $(LDFLAGS)

test_ping: test_ping.c soft_i2c.c soft_i2c.h
	$(CC) $(CFLAGS) -o test_ping test_ping.c soft_i2c.c $(LDFLAGS)

clean:
	rm -f $(TARGETS) *.o

.PHONY: all clean