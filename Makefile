CC = gcc
CFLAGS = -Wall -O2

# Default target
all: i2c_tof_test emulator

# Object files
i2c_transport.o: i2c_transport.c i2c_transport.h
	$(CC) $(CFLAGS) -c i2c_transport.c

# Test program
i2c_tof_test: i2c_tof_test.c i2c_transport.o
	$(CC) $(CFLAGS) -o i2c_tof_test i2c_tof_test.c i2c_transport.o

emulator: vl53l0x_emu.c
	gcc -Wall -O2 -o vl53l0x_emu vl53l0x_emu.c -lgpiod


clean:
	rm -f *.o i2c_tof_test

.PHONY: all clean

