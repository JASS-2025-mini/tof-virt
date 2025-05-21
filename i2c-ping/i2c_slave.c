// i2c_slave.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "soft_i2c.h"

#define SDA_PIN 17  // GPIO pin for SDA
#define SCL_PIN 27  // GPIO pin for SCL
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
    uint8_t recv_buffer[32];  // Increased buffer size
    uint8_t send_buffer[64];  // Increased buffer size for timestamp
    int result;
    uint64_t timestamp;
    
    // Register signal handler for clean exit
    signal(SIGINT, handle_signal);
    
    // Configure I2C
    config.sda_pin = SDA_PIN;
    config.scl_pin = SCL_PIN;
    config.slave_address = MY_ADDR;
    config.bit_delay = 1000;  // 1000 microseconds delay for reliability
    
    // Initialize I2C
    if (i2c_init(&config) < 0) {
        fprintf(stderr, "Failed to initialize I2C\n");
        return 1;
    }
    
    printf("I2C Slave initialized. Press Ctrl+C to exit.\n");
    printf("Using SDA: GPIO%d, SCL: GPIO%d, My address: 0x%02X\n", 
           config.sda_pin, config.scl_pin, config.slave_address);
    
    // Main loop
    while (running) {
        // Clear buffers
        memset(recv_buffer, 0, sizeof(recv_buffer));
        memset(send_buffer, 0, sizeof(send_buffer));
        
        // Listen for incoming data
        printf("Waiting for ping...\n");
        printf("DEBUG: main - About to call i2c_slave_read\n");
        
        // Improved message reception that can handle STOP condition
        int read_bytes = 0;
        while (read_bytes < sizeof(recv_buffer) - 1) {
            uint8_t byte;
            printf("DEBUG: main - Waiting for next byte\n");
            result = i2c_slave_read_byte_with_stop_check(&config, &byte);
            if (result <= 0) {
                // STOP condition received or error
                printf("DEBUG: main - STOP condition detected or error\n");
                break;
            }
            recv_buffer[read_bytes++] = byte;
            printf("DEBUG: main - Received byte 0x%02X\n", byte);
            if (byte == 0) { // Null terminator
                printf("DEBUG: main - Null terminator received\n");
                break;
            }
        }
        result = (read_bytes > 0) ? read_bytes : -1;
        
        if (result > 0) {
            // Get current timestamp
            timestamp = get_timestamp_ms();
            
            printf("DEBUG: main - Received message: %s\n", recv_buffer);
            printf("Received: %s\n", recv_buffer);
            
            // Prepare pong response with timestamp
            printf("DEBUG: main - Preparing PONG response\n");
            snprintf((char*)send_buffer, sizeof(send_buffer), "PONG:%llu", 
                   (unsigned long long)timestamp);
            printf("Sending: %s\n", send_buffer);
            
            // Small delay to allow master to prepare for reading
            // usleep(10000);  // 10ms delay
            
            printf("DEBUG: main - Waiting for master to request data\n");
            
            // Wait for master to request data and then send response
            int max_wait_ms = 1000;  // 1 second maximum
            int waited = 0;
            
            while (waited < max_wait_ms) {
                // Try to send data when master requests it
                printf("DEBUG: main - Trying to send response\n");
                result = i2c_slave_write(&config, send_buffer, strlen((char*)send_buffer) + 1);
                if (result >= 0) {
                    printf("DEBUG: main - Response sent successfully: %d bytes\n", result);
                    break;
                }
                
                // Failed to send, wait a bit and try again
                // usleep(10000);  // 10ms
                waited += 10;
            }
            
            if (waited >= max_wait_ms) {
                printf("DEBUG: main - Timeout waiting for master to request data\n");
            }
        } else {
            // No data received, wait a bit
            usleep(100000);  // 100ms delay
        }
    }
    
    printf("Cleaning up...\n");
    i2c_cleanup();
    
    return 0;
}