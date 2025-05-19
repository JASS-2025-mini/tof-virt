#ifndef I2C_TRANSPORT_H
#define I2C_TRANSPORT_H

#include <stdint.h>


typedef struct {
    int file_descriptor;
    uint8_t device_addr;
} i2c_device_t;

int i2c_init(i2c_device_t *dev, const char *bus, uint8_t addr);

void i2c_close(i2c_device_t *dev);


int i2c_write_byte(i2c_device_t *dev, uint8_t reg, uint8_t value);


int i2c_read_byte(i2c_device_t *dev, uint8_t reg, uint8_t *value);


int i2c_read_bytes(i2c_device_t *dev, uint8_t reg, uint8_t *value, uint8_t len);

#endif /* I2C_TRANSPORT_H */