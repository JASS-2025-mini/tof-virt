// i2c_slave.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "soft_i2c.h"

#define SDA_PIN 20  // GPIO pin for SDA (alternative pins)
#define SCL_PIN 21  // GPIO pin for SCL (alternative pins)
#define MY_ADDR 0x42  // I2C address of this slave device

volatile int running = 1;

// Signal handler to handle Ctrl+C
void handle_signal(int sig) {
    (void)sig;  // Avoid unused parameter warning
    running = 0;
}

int main(int argc, char *argv[]) {
    (void)argc;  // Avoid unused parameter warning
    (void)argv;  // Avoid unused parameter warning
    
    I2C_Config config;
    int result;
    
    // Register signal handler for clean exit
    signal(SIGINT, handle_signal);
    
    // Configure I2C
    config.sda_pin = SDA_PIN;
    config.scl_pin = SCL_PIN;
    config.slave_address = MY_ADDR;
    config.bit_delay = 2000;  // 2000 microseconds delay for better stability
    
    // Initialize I2C
    if (i2c_init(&config) < 0) {
        fprintf(stderr, "Failed to initialize I2C\n");
        return 1;
    }
    
    printf("I2C Slave initialized. Press Ctrl+C to exit.\n");
    printf("Using SDA: GPIO%d, SCL: GPIO%d, My address: 0x%02X\n", 
           config.sda_pin, config.scl_pin, config.slave_address);
    
    // Register simulation for VL53L0X-like behavior
    uint8_t current_register = 0;
    uint8_t register_data[256];
    
    // Initialize some test registers
    memset(register_data, 0, sizeof(register_data));
    snprintf((char*)&register_data[0x00], 32, "VL53L0X_SIM");  // Device ID register
    register_data[0x01] = 0x42;  // Status register
    
    // Main loop - robust slave with proper state machine
    while (running) {
        printf("DEBUG: Waiting for START condition...\n");
        
        // Listen for any I2C transaction
        int rw_bit = i2c_slave_listen(&config);
        if (rw_bit < 0) {
            printf("DEBUG: Address not for us, retrying...\n");
            usleep(10000);  // Wait 10ms before retry
            continue;
        }
        
        if (rw_bit == 0) {
            // Master wants to WRITE (send us register address)
            printf("DEBUG: Master WRITE transaction - receiving register address\n");
            
            uint8_t reg_addr;
            result = i2c_slave_read_byte_with_stop_check(&config, &reg_addr);
            if (result > 0) {
                current_register = reg_addr;
                printf("DEBUG: Register address set to 0x%02X\n", current_register);
            } else {
                printf("DEBUG: Failed to read register address or STOP received\n");
            }
        } else {
            // Master wants to READ (get data from current register)
            printf("DEBUG: Master READ transaction - sending register 0x%02X data\n", current_register);
            
            uint8_t response_data = register_data[current_register];
            result = i2c_slave_write_byte(&config, response_data);
            if (result >= 0) {
                printf("DEBUG: Successfully sent 0x%02X from register 0x%02X\n", 
                       response_data, current_register);
            } else {
                printf("DEBUG: Failed to send data or NACK received\n");
            }
        }
        
        printf("DEBUG: Transaction complete, ready for next\n");
        // Small delay to ensure bus is stable
        usleep(5000);  // 5ms delay
    }
    
    printf("Cleaning up...\n");
    i2c_cleanup(&config);
    
    return 0;
}