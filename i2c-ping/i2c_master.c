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
    config.bit_delay = 1000;  // 1000 microseconds delay for reliability
    
    // Initialize I2C
    if (i2c_init(&config) < 0) {
        fprintf(stderr, "Failed to initialize GPIO\n");
        return 1;
    }
    
    printf("I2C Master initialized. Press Ctrl+C to exit.\n");
    printf("Using SDA: GPIO%d, SCL: GPIO%d, Slave address: 0x%02X\n", 
           config.sda_pin, config.scl_pin, config.slave_address);
    
    // Main loop
    i = 0;
    while (running) {
        // Prepare ping message with safer snprintf
        memset(send_buffer, 0, sizeof(send_buffer));
        snprintf((char*)send_buffer, sizeof(send_buffer), "PING:%d", i++);
        
        printf("Sending: %s\n", send_buffer);
        
        // Record send time
        send_time = get_timestamp_ms();
        
        // Send ping
        result = i2c_master_write(&config, send_buffer, strlen((char*)send_buffer) + 1);
        if (result < 0) {
            fprintf(stderr, "Failed to send ping\n");
            sleep(1);
            continue;
        }
        
        // Longer delay to allow slave to prepare response
        printf("DEBUG: main - Allowing slave time to process and prepare response\n");
        usleep(50000);  // 50ms delay
        
        // Read pong response
        memset(recv_buffer, 0, sizeof(recv_buffer));
        result = i2c_master_read(&config, recv_buffer, sizeof(recv_buffer));
        
        // Record receive time and calculate round-trip time
        recv_time = get_timestamp_ms();
        rtt = recv_time - send_time;
        
        if (result < 0) {
            fprintf(stderr, "Failed to read pong response\n");
        } else {
            printf("Received: %s, RTT: %llu ms\n", recv_buffer, (unsigned long long)rtt);
        }
        
        // Wait a second before next ping
        sleep(2);
    }
    
    printf("Cleaning up...\n");
    i2c_cleanup();
    
    return 0;
}