// vl53l0x_fixed_slave.c - Fixed VL53L0X I2C Slave with proper START detection
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include "soft_i2c.h"

#define SDA_PIN 22  // GPIO pin for SDA
#define SCL_PIN 23  // GPIO pin for SCL
#define VL53L0X_ADDR 0x29  // VL53L0X I2C address

// VL53L0X Register addresses
#define VL53L0X_REG_IDENTIFICATION_MODEL_ID     0xC0
#define VL53L0X_REG_IDENTIFICATION_REVISION_ID  0xC2
#define VL53L0X_REG_SYSRANGE_START              0x00
#define VL53L0X_REG_RESULT_INTERRUPT_STATUS     0x13
#define VL53L0X_REG_RESULT_RANGE_STATUS         0x14
#define VL53L0X_REG_RESULT_RANGE_VAL            0x1E

// VL53L0X expected values
#define VL53L0X_MODEL_ID    0xEE
#define VL53L0X_REVISION_ID 0x10

volatile int running = 1;

// Virtual device registers
uint8_t registers[256];
uint8_t current_reg = 0;
uint16_t distance_mm = 500;

void handle_signal(int sig) {
    (void)sig;
    running = 0;
}

// Initialize virtual device registers
void init_registers(void) {
    memset(registers, 0, sizeof(registers));
    
    // Set identification registers
    registers[VL53L0X_REG_IDENTIFICATION_MODEL_ID] = VL53L0X_MODEL_ID;
    registers[VL53L0X_REG_IDENTIFICATION_REVISION_ID] = VL53L0X_REVISION_ID;
    
    // Set initial distance
    registers[VL53L0X_REG_RESULT_RANGE_VAL] = (distance_mm >> 8) & 0xFF;
    registers[VL53L0X_REG_RESULT_RANGE_VAL + 1] = distance_mm & 0xFF;
    
    // Set status registers
    registers[VL53L0X_REG_RESULT_INTERRUPT_STATUS] = 0x07;  // Data ready
    registers[VL53L0X_REG_RESULT_RANGE_STATUS] = 0x00;      // Valid range
}

// Wait for proper START condition
int wait_for_start(I2C_Config *config) {
    int last_sda = 1;
    int last_scl = 1;
    int timeout = 0;
    
    // First wait for idle state (both high)
    while (timeout < 100000) {
        int sda = gpiod_line_get_value(config->sda_line);
        int scl = gpiod_line_get_value(config->scl_line);
        
        // Detect START: SDA falls while SCL is high
        if (last_scl == 1 && scl == 1 && last_sda == 1 && sda == 0) {
            // Found START condition
            return 0;
        }
        
        last_sda = sda;
        last_scl = scl;
        timeout++;
        usleep(1);
    }
    
    return -1;  // Timeout
}

// Fixed slave write byte with proper ACK handling
int slave_write_byte_fixed(I2C_Config *config, uint8_t byte) {
    int i;
    
    // Make sure SDA is in output mode
    gpiod_line_release(config->sda_line);
    if (gpiod_line_request_output(config->sda_line, "i2c_sda_write", 0) < 0) {
        return -1;
    }
    
    // Write 8 bits
    for (i = 7; i >= 0; i--) {
        // Set SDA according to bit
        int bit = (byte >> i) & 1;
        gpiod_line_set_value(config->sda_line, bit);
        
        // Wait for SCL to go high
        while (gpiod_line_get_value(config->scl_line) == 0) {
            usleep(1);
        }
        
        // Wait for SCL to go low
        while (gpiod_line_get_value(config->scl_line) == 1) {
            usleep(1);
        }
    }
    
    // Release SDA for master to send ACK
    gpiod_line_set_value(config->sda_line, 1);
    gpiod_line_release(config->sda_line);
    if (gpiod_line_request_input(config->sda_line, "i2c_sda_read_ack") < 0) {
        return -1;
    }
    
    // Wait for SCL to go high
    while (gpiod_line_get_value(config->scl_line) == 0) {
        usleep(1);
    }
    
    // Read ACK bit
    int ack = gpiod_line_get_value(config->sda_line);
    
    // Wait for SCL to go low
    while (gpiod_line_get_value(config->scl_line) == 1) {
        usleep(1);
    }
    
    // Reconfigure SDA back to input for next transaction
    gpiod_line_release(config->sda_line);
    if (gpiod_line_request_input(config->sda_line, "i2c_sda_slave") < 0) {
        return -1;
    }
    
    return (ack == 0) ? 0 : -1;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    I2C_Config config;
    int transaction_count = 0;
    
    signal(SIGINT, handle_signal);
    
    // Configure I2C
    config.sda_pin = SDA_PIN;
    config.scl_pin = SCL_PIN;
    config.slave_address = VL53L0X_ADDR;
    config.bit_delay = 2000;
    
    // Initialize I2C as slave
    if (i2c_init_slave(&config) < 0) {
        fprintf(stderr, "Failed to initialize I2C slave\n");
        return 1;
    }
    
    // Initialize virtual device
    init_registers();
    
    printf("VL53L0X Fixed Slave Started\n");
    printf("Using SDA: GPIO%d, SCL: GPIO%d, Address: 0x%02X\n", 
           config.sda_pin, config.scl_pin, config.slave_address);
    printf("Model ID: 0x%02X, Revision ID: 0x%02X\n", VL53L0X_MODEL_ID, VL53L0X_REVISION_ID);
    printf("Initial distance: %d mm\n\n", distance_mm);
    
    while (running) {
        // Wait for proper START condition
        if (wait_for_start(&config) < 0) {
            continue;
        }
        
        // Now use regular slave listen
        int result = i2c_slave_listen(&config);
        if (result < 0) {
            continue;
        }
        
        transaction_count++;
        printf("Transaction %d: ", transaction_count);
        
        if (result == 0) {  // Write mode
            printf("WRITE - ");
            
            // Read register address
            int byte_result = i2c_slave_read_byte(&config);
            if (byte_result < 0) {
                printf("Failed to read byte\n");
                continue;
            }
            
            current_reg = (uint8_t)byte_result;
            printf("Reg 0x%02X", current_reg);
            
            // For SYSRANGE_START, try to read value
            if (current_reg == VL53L0X_REG_SYSRANGE_START) {
                // Check for more data with very short timeout
                int scl_stable = 0;
                for (int i = 0; i < 100; i++) {
                    if (gpiod_line_get_value(config.scl_line) == 0) {
                        scl_stable++;
                        if (scl_stable > 10) {
                            // Clock is low, might be data coming
                            byte_result = i2c_slave_read_byte(&config);
                            if (byte_result >= 0) {
                                uint8_t value = (uint8_t)byte_result;
                                registers[current_reg] = value;
                                printf(" = 0x%02X", value);
                                
                                if (value & 0x01) {
                                    printf(" (start measurement)");
                                    // Update distance
                                    distance_mm += 10;
                                    if (distance_mm > 1000) distance_mm = 100;
                                    registers[VL53L0X_REG_RESULT_RANGE_VAL] = (distance_mm >> 8) & 0xFF;
                                    registers[VL53L0X_REG_RESULT_RANGE_VAL + 1] = distance_mm & 0xFF;
                                }
                            }
                            break;
                        }
                    } else {
                        scl_stable = 0;
                    }
                    usleep(10);
                }
            }
            printf("\n");
            
        } else if (result == 1) {  // Read mode
            printf("READ - ");
            
            // Send register value using fixed function
            uint8_t value = registers[current_reg];
            printf("Reg 0x%02X = 0x%02X", current_reg, value);
            
            if (slave_write_byte_fixed(&config, value) < 0) {
                printf(" - FAILED");
            } else {
                printf(" - OK");
            }
            
            // Always increment for VL53L0X multi-byte reads
            current_reg++;
            printf(" (next: 0x%02X)\n", current_reg);
        }
    }
    
    printf("\nCleaning up...\n");
    i2c_cleanup(&config);
    
    return 0;
}