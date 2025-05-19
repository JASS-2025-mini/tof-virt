#include "i2c_transport.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

// VL53L0X I2C address
#define VL53L0X_ADDR 0x29

// VL53L0X registers
#define REG_IDENTIFICATION_MODEL_ID       0xC0
#define REG_IDENTIFICATION_REVISION_ID    0xC2
#define REG_PRE_RANGE_CONFIG_VCSEL_PERIOD 0x50
#define REG_FINAL_RANGE_CONFIG_VCSEL_PERIOD 0x70

// Function to scan I2C bus for devices
void scan_i2c_bus(const char *bus) {
    i2c_device_t dev;
    uint8_t value;
    int ret;
    
    printf("Scanning I2C bus %s...\n", bus);
    
    // Try to detect all possible I2C devices
    for (int addr = 0x03; addr < 0x78; addr++) {
        // Skip unwanted addresses
        if (addr == 0x28 || addr == 0x37) {
            continue;
        }
        
        // Try to initialize I2C device
        ret = i2c_init(&dev, bus, addr);
        if (ret < 0) {
            continue;
        }
        
        // Try to read from the device
        ret = i2c_read_byte(&dev, 0x00, &value);
        if (ret >= 0) {
            printf("Found I2C device at address: 0x%02X\n", addr);
            
            // Check if it's VL53L0X
            if (addr == VL53L0X_ADDR) {
                uint8_t model_id;
                if (i2c_read_byte(&dev, REG_IDENTIFICATION_MODEL_ID, &model_id) >= 0) {
                    if (model_id == 0xEE) {
                        printf("VL53L0X sensor detected! Model ID: 0x%02X\n", model_id);
                        
                        // Read more information
                        uint8_t revision_id;
                        if (i2c_read_byte(&dev, REG_IDENTIFICATION_REVISION_ID, &revision_id) >= 0) {
                            printf("VL53L0X Revision ID: 0x%02X\n", revision_id);
                        }
                        
                        uint8_t pre_range_period;
                        if (i2c_read_byte(&dev, REG_PRE_RANGE_CONFIG_VCSEL_PERIOD, &pre_range_period) >= 0) {
                            printf("VL53L0X Pre Range Period: 0x%02X\n", pre_range_period);
                        }
                        
                        uint8_t final_range_period;
                        if (i2c_read_byte(&dev, REG_FINAL_RANGE_CONFIG_VCSEL_PERIOD, &final_range_period) >= 0) {
                            printf("VL53L0X Final Range Period: 0x%02X\n", final_range_period);
                        }
                    } else {
                        printf("Device at 0x%02X is not a VL53L0X sensor (Model ID: 0x%02X)\n", addr, model_id);
                    }
                }
            }
        }
        
        i2c_close(&dev);
    }
    
    printf("I2C bus scan completed.\n");
}

// Function to test VL53L0X sensor
int test_vl53l0x(const char *bus) {
    i2c_device_t dev;
    uint8_t model_id;
    int ret;
    
    printf("\nTesting VL53L0X sensor on bus %s...\n", bus);
    
    // Initialize I2C device
    ret = i2c_init(&dev, bus, VL53L0X_ADDR);
    if (ret < 0) {
        printf("Failed to initialize I2C device at address 0x%02X\n", VL53L0X_ADDR);
        return -1;
    }
    
    // Read Model ID
    ret = i2c_read_byte(&dev, REG_IDENTIFICATION_MODEL_ID, &model_id);
    if (ret < 0) {
        printf("Failed to read Model ID\n");
        i2c_close(&dev);
        return -1;
    }
    
    if (model_id != 0xEE) {
        printf("Error: Invalid Model ID (0x%02X, expected 0xEE)\n", model_id);
        i2c_close(&dev);
        return -1;
    }
    
    printf("VL53L0X sensor test successful!\n");
    printf("Model ID: 0x%02X (correct value)\n", model_id);
    
    // Close I2C device
    i2c_close(&dev);
    
    return 0;
}

int main(int argc, char *argv[]) {
    const char *bus = "/dev/i2c-1";  // Default I2C bus on Raspberry Pi 3B
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--bus") == 0 && i + 1 < argc) {
            bus = argv[i + 1];
            i++;
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [--bus BUS]\n", argv[0]);
            printf("  --bus BUS   Specify I2C bus (default: /dev/i2c-1)\n");
            return 0;
        }
    }
    
    // Scan I2C bus for all devices
    scan_i2c_bus(bus);
    
    // Test VL53L0X sensor specifically
    test_vl53l0x(bus);
    
    return 0;
}