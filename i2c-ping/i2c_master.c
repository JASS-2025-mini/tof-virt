// i2c_master.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "soft_i2c.h"

#define SDA_PIN 17  // GPIO pin for SDA 
#define SCL_PIN 27  // GPIO pin for SCL
#define SLAVE_ADDR 0x42  // I2C address of slave device

volatile int running = 1;

void handle_signal(int sig) {
    (void)sig;  // Avoid unused parameter warning
    running = 0;
}

int main(int argc, char *argv[]) {
    (void)argc;  // Avoid unused parameter warning
    (void)argv;  // Avoid unused parameter warning
    
    I2C_Config config;
    uint8_t send_buffer[32];  // Increased buffer size
    uint8_t recv_buffer[32];  // Increased buffer size
    int i, result;
    uint64_t send_time, recv_time, rtt;
    
    // Register signal handler for clean exit
    signal(SIGINT, handle_signal);
    
    // Configure I2C
    config.sda_pin = SDA_PIN;
    config.scl_pin = SCL_PIN;
    config.slave_address = SLAVE_ADDR;
    config.bit_delay = 2000;  // 2000 microseconds delay for better stability
    
    // Initialize I2C
    if (i2c_init(&config) < 0) {
        fprintf(stderr, "Failed to initialize GPIO\n");
        return 1;
    }
    
    printf("I2C Master initialized. Press Ctrl+C to exit.\n");
    printf("Using SDA: GPIO%d, SCL: GPIO%d, Slave address: 0x%02X\n", 
           config.sda_pin, config.scl_pin, config.slave_address);
    
    // Main loop - VL53L0X-like register access test
    i = 0;
    while (running) {
        printf("\n=== Test cycle %d ===\n", i++);
        
        // Record test start time
        send_time = get_timestamp_ms();
        
        // Test 1: Write register address for device ID
        printf("1. Setting register address to 0x00 (Device ID)\n");
        send_buffer[0] = 0x00;  // Device ID register
        result = i2c_master_write(&config, send_buffer, 1);
        if (result < 0) {
            fprintf(stderr, "Failed to write register address\n");
            sleep(1);
            continue;
        }
        
        // Small delay between transactions
        usleep(10000);  // 10ms delay
        
        // Test 2: Read device ID
        printf("2. Reading device ID from register 0x00\n");
        memset(recv_buffer, 0, sizeof(recv_buffer));
        result = i2c_master_read(&config, recv_buffer, 11);  // Read "VL53L0X_SIM"
        
        if (result < 0) {
            fprintf(stderr, "Failed to read device ID\n");
        } else {
            printf("Device ID: %s\n", recv_buffer);
        }
        
        usleep(10000);  // 10ms delay
        
        // Test 3: Write register address for status
        printf("3. Setting register address to 0x01 (Status)\n");
        send_buffer[0] = 0x01;  // Status register
        result = i2c_master_write(&config, send_buffer, 1);
        if (result < 0) {
            fprintf(stderr, "Failed to write status register address\n");
        }
        
        usleep(10000);  // 10ms delay
        
        // Test 4: Read status
        printf("4. Reading status from register 0x01\n");
        memset(recv_buffer, 0, sizeof(recv_buffer));
        result = i2c_master_read(&config, recv_buffer, 1);
        
        recv_time = get_timestamp_ms();
        rtt = recv_time - send_time;
        
        if (result < 0) {
            fprintf(stderr, "Failed to read status\n");
        } else {
            printf("Status: 0x%02X\n", recv_buffer[0]);
        }
        
        printf("Test cycle RTT: %llu ms\n", (unsigned long long)rtt);
        
        // Wait before next test cycle
        sleep(3);
    }
    
    printf("Cleaning up...\n");
    i2c_cleanup(&config);
    
    return 0;
}