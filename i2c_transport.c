#include "i2c_transport.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>


int i2c_init(i2c_device_t *dev, const char *bus, uint8_t addr) {
    dev->file_descriptor = open(bus, O_RDWR);
    if (dev->file_descriptor < 0) {
        perror("Ошибка открытия устройства I2C");
        return -1;
    }
    
    if (ioctl(dev->file_descriptor, I2C_SLAVE, addr) < 0) {
        perror("Ошибка установки адреса I2C");
        close(dev->file_descriptor);
        return -1;
    }
    
    dev->device_addr = addr;
    return 0;
}

void i2c_close(i2c_device_t *dev) {
    if (dev->file_descriptor >= 0) {
        close(dev->file_descriptor);
        dev->file_descriptor = -1;
    }
}


int i2c_write_byte(i2c_device_t *dev, uint8_t reg, uint8_t value) {
    uint8_t buf[2];
    buf[0] = reg;
    buf[1] = value;
    if (write(dev->file_descriptor, buf, 2) != 2) {
        return -1;
    }
    return 0;
}

int i2c_read_byte(i2c_device_t *dev, uint8_t reg, uint8_t *value) {
    if (write(dev->file_descriptor, &reg, 1) != 1) {
        return -1;
    }
    if (read(dev->file_descriptor, value, 1) != 1) {
        return -1;
    }
    return 0;
}

int i2c_read_bytes(i2c_device_t *dev, uint8_t reg, uint8_t *value, uint8_t len) {
    if (write(dev->file_descriptor, &reg, 1) != 1) {
        return -1;
    }
    if (read(dev->file_descriptor, value, len) != len) {
        return -1;
    }
    return 0;
}