// soft_i2c.h
#ifndef SOFT_I2C_H
#define SOFT_I2C_H

#include <stdint.h>
#include <gpiod.h>
#include <time.h>

// Configuration for pins
typedef struct {
    int sda_pin;  // Data pin
    int scl_pin;  // Clock pin
    uint8_t slave_address;  // I2C slave address
    int bit_delay;  // Delay in microseconds between bit operations
    
    // GPIO handles (internal)
    struct gpiod_chip *chip;
    struct gpiod_line *sda_line;
    struct gpiod_line *scl_line;
} I2C_Config;

// Initialize software I2C with given configuration
int i2c_init(I2C_Config *config);

// Initialize software I2C for slave (input mode)
int i2c_init_slave(I2C_Config *config);

// Helper function for slave to send ACK
int i2c_slave_send_ack(I2C_Config *config, int ack);

// Clean up resources
void i2c_cleanup(I2C_Config *config);

// Master functions
int i2c_start(I2C_Config *config);
void i2c_stop(I2C_Config *config);
int i2c_write_byte(I2C_Config *config, uint8_t byte);
uint8_t i2c_read_byte(I2C_Config *config, int ack);

// Slave functions
int i2c_slave_listen(I2C_Config *config);
int i2c_slave_read_byte(I2C_Config *config);
int i2c_slave_write_byte(I2C_Config *config, uint8_t byte);
int i2c_slave_read_byte_with_stop_check(I2C_Config *config, uint8_t *byte);

// High-level functions
int i2c_master_write(I2C_Config *config, uint8_t *data, int length);
int i2c_master_read(I2C_Config *config, uint8_t *buffer, int length);
int i2c_slave_write(I2C_Config *config, uint8_t *data, int length);
int i2c_slave_read(I2C_Config *config, uint8_t *buffer, int length);

// Get current timestamp in milliseconds
uint64_t get_timestamp_ms(void);

// Debug function
void i2c_debug_status(I2C_Config *config);

// Bus recovery
void i2c_bus_recovery(I2C_Config *config);

#endif // SOFT_I2C_H