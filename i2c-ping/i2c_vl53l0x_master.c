// i2c_vl53l0x_master.c - VL53L0X Master Test Program
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "soft_i2c.h"

#define SDA_PIN 17  // GPIO pin for SDA
#define SCL_PIN 27  // GPIO pin for SCL
#define VL53L0X_ADDR 0x29  // VL53L0X I2C address

// VL53L0X Register addresses
#define VL53L0X_REG_IDENTIFICATION_MODEL_ID     0xC0
#define VL53L0X_REG_IDENTIFICATION_REVISION_ID  0xC2
#define VL53L0X_REG_SYSRANGE_START              0x00
#define VL53L0X_REG_RESULT_INTERRUPT_STATUS     0x13
#define VL53L0X_REG_RESULT_RANGE_STATUS         0x14
#define VL53L0X_REG_RESULT_RANGE_VAL            0x1E

volatile int running = 1;

void handle_signal(int sig) {
    (void)sig;
    running = 0;
}

// Read a single register from VL53L0X
int vl53l0x_read_register(I2C_Config *config, uint8_t reg_addr, uint8_t *value) {
    // Write register address
    if (i2c_master_write(config, &reg_addr, 1) < 0) {
        return -1;
    }
    
    usleep(1000);  // Small delay between write and read
    
    // Read register value
    if (i2c_master_read(config, value, 1) < 0) {
        return -1;
    }
    
    return 0;
}

// Write a single register to VL53L0X
int vl53l0x_write_register(I2C_Config *config, uint8_t reg_addr, uint8_t value) {
    uint8_t data[2] = {reg_addr, value};
    return i2c_master_write(config, data, 2);
}

// Read 16-bit distance value (big-endian)
int vl53l0x_read_distance(I2C_Config *config, uint16_t *distance_mm) {
    uint8_t high_byte, low_byte;
    
    // Read high byte
    if (vl53l0x_read_register(config, VL53L0X_REG_RESULT_RANGE_VAL, &high_byte) < 0) {
        return -1;
    }
    
    // Read low byte
    if (vl53l0x_read_register(config, VL53L0X_REG_RESULT_RANGE_VAL + 1, &low_byte) < 0) {
        return -1;
    }
    
    *distance_mm = (high_byte << 8) | low_byte;
    return 0;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    I2C_Config config;
    uint8_t model_id, revision_id;
    uint8_t status;
    uint16_t distance_mm;
    int cycle = 0;
    
    signal(SIGINT, handle_signal);
    
    // Configure I2C
    config.sda_pin = SDA_PIN;
    config.scl_pin = SCL_PIN;
    config.slave_address = VL53L0X_ADDR;
    config.bit_delay = 2000;
    
    // Initialize I2C
    if (i2c_init(&config) < 0) {
        fprintf(stderr, "Failed to initialize I2C\n");
        return 1;
    }
    
    printf("VL53L0X Master Test Program\n");
    printf("Using SDA: GPIO%d, SCL: GPIO%d, VL53L0X address: 0x%02X\n", 
           config.sda_pin, config.scl_pin, config.slave_address);
    
    // Read device identification
    printf("\n=== Device Identification ===\n");
    if (vl53l0x_read_register(&config, VL53L0X_REG_IDENTIFICATION_MODEL_ID, &model_id) == 0) {
        printf("Model ID: 0x%02X\n", model_id);
    } else {
        printf("Failed to read Model ID\n");
    }
    
    if (vl53l0x_read_register(&config, VL53L0X_REG_IDENTIFICATION_REVISION_ID, &revision_id) == 0) {
        printf("Revision ID: 0x%02X\n", revision_id);
    } else {
        printf("Failed to read Revision ID\n");
    }
    
    printf("\n=== Starting Distance Measurements ===\n");
    
    // Main measurement loop
    while (running) {
        printf("\n--- Measurement Cycle %d ---\n", cycle++);
        
        // Start single measurement
        printf("1. Starting measurement...\n");
        if (vl53l0x_write_register(&config, VL53L0X_REG_SYSRANGE_START, 0x01) < 0) {
            printf("   Failed to start measurement\n");
            sleep(1);
            continue;
        }
        
        // Wait for measurement to complete - simplified approach
        printf("2. Waiting for measurement completion...\n");
        usleep(50000);  // Fixed 50ms delay for measurement
        
        uint8_t interrupt_status = 0;
        if (vl53l0x_read_register(&config, VL53L0X_REG_RESULT_INTERRUPT_STATUS, &interrupt_status) < 0) {
            printf("   Failed to read interrupt status\n");
            sleep(1);
            continue;
        }
        
        printf("   Measurement complete (interrupt status: 0x%02X)\n", interrupt_status);
        
        // Read range status
        if (vl53l0x_read_register(&config, VL53L0X_REG_RESULT_RANGE_STATUS, &status) == 0) {
            printf("3. Range status: 0x%02X\n", status);
        } else {
            printf("3. Failed to read range status\n");
        }
        
        // Read distance measurement
        if (vl53l0x_read_distance(&config, &distance_mm) == 0) {
            printf("4. Distance: %d mm\n", distance_mm);
        } else {
            printf("4. Failed to read distance\n");
        }
        
        // Wait before next measurement
        sleep(2);
    }
    
    printf("\nCleaning up...\n");
    i2c_cleanup(&config);
    
    return 0;
}