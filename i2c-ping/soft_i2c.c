// soft_i2c.c
#include "soft_i2c.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

// Initialize GPIO 
int i2c_init(I2C_Config *config) {
    // Initialize pigpio library
    if (gpioInitialise() < 0) {
        fprintf(stderr, "Failed to initialize GPIO\n");
        return -1;
    }

    // Configure pins
    gpioSetMode(config->sda_pin, PI_OUTPUT);
    gpioSetMode(config->scl_pin, PI_OUTPUT);
    
    // Set pins to high (idle state)
    gpioWrite(config->sda_pin, 1);
    gpioWrite(config->scl_pin, 1);
    
    // Default bit delay if not specified
    if (config->bit_delay == 0) {
        // config->bit_delay = 1000;  // 1000 microseconds default for reliability
        config->bit_delay = 10;
    }
    
    // Enable internal pull-up resistors
    gpioSetPullUpDown(config->sda_pin, PI_PUD_UP);
    gpioSetPullUpDown(config->scl_pin, PI_PUD_UP);
    
    return 0;
}

void i2c_cleanup(void) {
    gpioTerminate();
}

// Debug function to display pin status
void i2c_debug_status(I2C_Config *config) {
    int sda_state = gpioRead(config->sda_pin);
    int scl_state = gpioRead(config->scl_pin);
    printf("DEBUG: SDA=%d, SCL=%d\n", sda_state, scl_state);
}

// Set SDA pin as input or output
static void sda_set_mode(I2C_Config *config, int mode) {
    gpioSetMode(config->sda_pin, mode);
    if (mode == PI_OUTPUT) {
        gpioWrite(config->sda_pin, 1);  // Pull high when setting as output
    }
}

// Generate I2C start condition
int i2c_start(I2C_Config *config) {
    // Ensure SDA is output
    printf("DEBUG: i2c_start - Setting SDA to output\n");
    sda_set_mode(config, PI_OUTPUT);
    
    // Ensure both lines are high
    printf("DEBUG: i2c_start - Setting both lines high\n");
    gpioWrite(config->sda_pin, 1);
    gpioWrite(config->scl_pin, 1);
    gpioDelay(config->bit_delay);
    
    // Generate start condition: SDA goes low while SCL is high
    printf("DEBUG: i2c_start - Generating START condition\n");
    gpioWrite(config->sda_pin, 0);
    gpioDelay(config->bit_delay);
    gpioWrite(config->scl_pin, 0);
    
    printf("DEBUG: i2c_start - START condition complete\n");
    
    return 0;
}

// Generate I2C stop condition
void i2c_stop(I2C_Config *config) {
    // Ensure SDA is output
    sda_set_mode(config, PI_OUTPUT);
    
    // Prepare for stop: ensure SCL is low, then set SDA low
    gpioWrite(config->scl_pin, 0);
    gpioWrite(config->sda_pin, 0);
    gpioDelay(config->bit_delay);
    
    // Generate stop condition: SCL goes high, then SDA goes high
    gpioWrite(config->scl_pin, 1);
    gpioDelay(config->bit_delay);
    gpioWrite(config->sda_pin, 1);
    gpioDelay(config->bit_delay);
    
    printf("DEBUG: i2c_stop - STOP condition complete\n");
}

// Write a byte to I2C bus
int i2c_write_byte(I2C_Config *config, uint8_t byte) {
    int i;
    int ack;
    
    printf("DEBUG: i2c_write_byte - Writing byte 0x%02X\n", byte);
    
    sda_set_mode(config, PI_OUTPUT);
    
    // Send 8 bits, MSB first
    for (i = 7; i >= 0; i--) {
        // Set SDA according to bit
        int bit = (byte >> i) & 1;
        printf("DEBUG: i2c_write_byte - Bit %d = %d\n", i, bit);
        
        gpioWrite(config->sda_pin, bit);
        gpioDelay(config->bit_delay);
        
        // Clock high and delay
        gpioWrite(config->scl_pin, 1);
        gpioDelay(config->bit_delay * 2);
        
        // Clock low and delay
        gpioWrite(config->scl_pin, 0);
        gpioDelay(config->bit_delay);
    }
    
    printf("DEBUG: i2c_write_byte - All bits sent, waiting for ACK\n");
    
    // Release SDA for slave acknowledgment
    gpioWrite(config->sda_pin, 1);
    sda_set_mode(config, PI_INPUT);
    
    // Clock high to read ACK bit
    gpioWrite(config->scl_pin, 1);
    gpioDelay(config->bit_delay);
    
    // Read ACK bit (0 = acknowledged)
    ack = gpioRead(config->sda_pin);
    printf("DEBUG: i2c_write_byte - ACK bit = %d (0=ACK, 1=NACK)\n", ack);
    
    // Clock low
    gpioWrite(config->scl_pin, 0);
    gpioDelay(config->bit_delay);
    
    // Restore SDA as output
    sda_set_mode(config, PI_OUTPUT);
    
    printf("DEBUG: i2c_write_byte - Returning %d\n", (ack == 0) ? 0 : -1);
    
    return (ack == 0) ? 0 : -1; // Return 0 on success (ACK received)
}

// Read a byte from I2C bus
uint8_t i2c_read_byte(I2C_Config *config, int ack) {
    int i;
    uint8_t byte = 0;
    
    printf("DEBUG: i2c_read_byte - Starting (ack=%d)\n", ack);
    
    // Release SDA so slave can control it
    sda_set_mode(config, PI_INPUT);
    
    // Read 8 bits
    for (i = 7; i >= 0; i--) {
        gpioWrite(config->scl_pin, 1);
        gpioDelay(config->bit_delay);
        
        if (gpioRead(config->sda_pin)) {
            byte |= (1 << i);
            printf("DEBUG: i2c_read_byte - Bit %d = 1\n", i);
        } else {
            printf("DEBUG: i2c_read_byte - Bit %d = 0\n", i);
        }
        
        gpioWrite(config->scl_pin, 0);
        gpioDelay(config->bit_delay);
    }
    
    // Send ACK/NACK
    sda_set_mode(config, PI_OUTPUT);
    gpioWrite(config->sda_pin, ack ? 1 : 0); // 0 = ACK, 1 = NACK
    printf("DEBUG: i2c_read_byte - Sending %s\n", ack ? "NACK" : "ACK");
    
    // Clock ACK/NACK bit
    gpioWrite(config->scl_pin, 1);
    gpioDelay(config->bit_delay);
    gpioWrite(config->scl_pin, 0);
    gpioDelay(config->bit_delay);
    
    printf("DEBUG: i2c_read_byte - Read 0x%02X\n", byte);
    
    return byte;
}

// Get current timestamp in milliseconds
uint64_t get_timestamp_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)(tv.tv_sec) * 1000 + (uint64_t)(tv.tv_usec) / 1000;
}

// Slave listens for its address
int i2c_slave_listen(I2C_Config *config) {
    int i;
    uint8_t address = 0;
    int read_write_bit;
    
    printf("DEBUG: Entering i2c_slave_listen\n");
    
    // Wait for START condition (SDA falling edge while SCL is high)
    sda_set_mode(config, PI_INPUT);
    gpioSetMode(config->scl_pin, PI_INPUT);
    
    printf("DEBUG: Set pins to input mode\n");
    
    // Wait for both lines to be high (idle)
    while (gpioRead(config->sda_pin) == 0 || gpioRead(config->scl_pin) == 0) {
        printf("DEBUG: Waiting for idle, SDA=%d, SCL=%d\n", 
               gpioRead(config->sda_pin), gpioRead(config->scl_pin));
        gpioDelay(config->bit_delay);
    }
    
    printf("DEBUG: Detected idle state, waiting for START\n");
    
    // Wait for SDA to go low while SCL is high (START condition)
    while (gpioRead(config->sda_pin) == 1) {
        if (gpioRead(config->scl_pin) == 0) {
            // SCL went low before SDA, not a START condition
            printf("DEBUG: SCL went low before SDA, not a START\n");
            gpioDelay(config->bit_delay);
            continue;
        }
        gpioDelay(config->bit_delay);
    }
    
    printf("DEBUG: Detected START condition\n");
    
    // Now SCL should go low
    while (gpioRead(config->scl_pin) == 1) {
        gpioDelay(config->bit_delay);
    }
    
    printf("DEBUG: SCL went low after START\n");
    
    // Read 7-bit address + R/W bit
    for (i = 7; i >= 0; i--) {
        while (gpioRead(config->scl_pin) == 0) {
            gpioDelay(config->bit_delay / 2);
        }
        
        int bit = gpioRead(config->sda_pin);
        printf("DEBUG: Read bit %d = %d\n", i, bit);
        
        if (bit) {
            address |= (1 << i);
        }
        
        while (gpioRead(config->scl_pin) == 1) {
            gpioDelay(config->bit_delay / 2);
        }
    }
    
    // Extract R/W bit
    read_write_bit = address & 0x01;
    address >>= 1;
    
    printf("DEBUG: Received address 0x%02X, R/W=%d\n", address, read_write_bit);
    
    // Check if this address matches our slave address
    if (address != config->slave_address) {
        printf("DEBUG: Address mismatch, expected 0x%02X, got 0x%02X\n", 
               config->slave_address, address);
        return -1; // Not our address
    }
    
    printf("DEBUG: Address match, sending ACK\n");
    
    // Send ACK
    sda_set_mode(config, PI_OUTPUT);
    gpioWrite(config->sda_pin, 0); // ACK
    
    // Wait for SCL to go high
    gpioSetMode(config->scl_pin, PI_INPUT);
    while (gpioRead(config->scl_pin) == 0) {
        gpioDelay(config->bit_delay / 2);
    }
    
    // Wait for SCL to go low
    while (gpioRead(config->scl_pin) == 1) {
        gpioDelay(config->bit_delay / 2);
    }
    
    printf("DEBUG: ACK bit clocked\n");
    
    // Release SDA
    gpioWrite(config->sda_pin, 1);
    
    printf("DEBUG: i2c_slave_listen returning %d\n", read_write_bit);
    
    return read_write_bit; // Return 1 if master wants to read, 0 if master wants to write
}

// Slave reads a byte
int i2c_slave_read_byte(I2C_Config *config) {
    int i;
    uint8_t byte = 0;
    
    printf("DEBUG: i2c_slave_read_byte - Starting\n");
    
    sda_set_mode(config, PI_INPUT);
    gpioSetMode(config->scl_pin, PI_INPUT);
    
    // Read 8 bits
    for (i = 7; i >= 0; i--) {
        // Wait for SCL to go high
        while (gpioRead(config->scl_pin) == 0) {
            gpioDelay(config->bit_delay / 2);
        }
        
        // Read bit
        int bit = gpioRead(config->sda_pin);
        printf("DEBUG: i2c_slave_read_byte - Bit %d = %d\n", i, bit);
        
        if (bit) {
            byte |= (1 << i);
        }
        
        // Wait for SCL to go low
        while (gpioRead(config->scl_pin) == 1) {
            gpioDelay(config->bit_delay / 2);
        }
    }
    
    // Send ACK
    printf("DEBUG: i2c_slave_read_byte - Sending ACK\n");
    sda_set_mode(config, PI_OUTPUT);
    gpioWrite(config->sda_pin, 0); // ACK
    
    // Wait for SCL to go high then low (clock ACK bit)
    gpioSetMode(config->scl_pin, PI_INPUT);
    while (gpioRead(config->scl_pin) == 0) {
        gpioDelay(config->bit_delay / 2);
    }
    while (gpioRead(config->scl_pin) == 1) {
        gpioDelay(config->bit_delay / 2);
    }
    
    // Release SDA
    gpioWrite(config->sda_pin, 1);
    sda_set_mode(config, PI_INPUT);
    
    printf("DEBUG: i2c_slave_read_byte - Completed, read 0x%02X\n", byte);
    
    return byte;
}

// Function to read byte with STOP condition check
int i2c_slave_read_byte_with_stop_check(I2C_Config *config, uint8_t *byte) {
    int i;
    *byte = 0;
    
    /// Todo
    config->bit_delay=10;
    printf("!DEBUG: i2c_slave_read_byte_with_stop_check, bit_delay: %d - Starting\n", config->bit_delay);
    
    sda_set_mode(config, PI_INPUT);
    gpioSetMode(config->scl_pin, PI_INPUT);
    
    // Check for STOP condition (SDA rising while SCL high)
    while (gpioRead(config->scl_pin) == 1) {
        if (gpioRead(config->sda_pin) == 0) {
            int prev_sda = 0;
            while (gpioRead(config->scl_pin) == 1) {
                int current_sda = gpioRead(config->sda_pin);
                if (prev_sda == 0 && current_sda == 1) {
                    printf("DEBUG: i2c_slave_read_byte_with_stop_check - STOP detected\n");
                    return 0; // STOP condition
                }
                prev_sda = current_sda;
                gpioDelay(config->bit_delay / 10);
            }
        }
        gpioDelay(config->bit_delay / 10);
    }
    
    // Read 8 bits
    for (i = 7; i >= 0; i--) {
        // Wait for SCL to go high
        while (gpioRead(config->scl_pin) == 0) {
            gpioDelay(config->bit_delay / 2);
        }
        
        // Read bit
        int bit = gpioRead(config->sda_pin);
        printf("DEBUG: i2c_slave_read_byte_with_stop_check - Bit %d = %d, bit_delay: %d\n", i, bit, config->bit_delay);
        
        if (bit) {
            *byte |= (1 << i);
        }
        
        // Wait for SCL to go low
        while (gpioRead(config->scl_pin) == 1) {
            gpioDelay(config->bit_delay / 2);
        }
    }
    
    // Send ACK
    printf("DEBUG: i2c_slave_read_byte_with_stop_check - Sending ACK\n");
    sda_set_mode(config, PI_OUTPUT);
    gpioWrite(config->sda_pin, 0); // ACK
    
    // Wait for SCL to go high then low (clock ACK bit)
    gpioSetMode(config->scl_pin, PI_INPUT);
    while (gpioRead(config->scl_pin) == 0) {
        gpioDelay(config->bit_delay / 2);
    }
    while (gpioRead(config->scl_pin) == 1) {
        gpioDelay(config->bit_delay / 2);
    }
    
    // Release SDA
    gpioWrite(config->sda_pin, 1);
    sda_set_mode(config, PI_INPUT);
    
    printf("DEBUG: i2c_slave_read_byte_with_stop_check - Read 0x%02X\n", *byte);
    
    return 1; // Successfully read byte
}

// Slave writes a byte
int i2c_slave_write_byte(I2C_Config *config, uint8_t byte) {
    int i;
    int ack;
    
    printf("DEBUG: i2c_slave_write_byte - Starting, writing 0x%02X\n", byte);
    
    sda_set_mode(config, PI_OUTPUT);
    gpioSetMode(config->scl_pin, PI_INPUT);
    
    // Write 8 bits
    for (i = 7; i >= 0; i--) {
        // Set SDA according to bit
        int bit = (byte >> i) & 1;
        gpioWrite(config->sda_pin, bit);
        printf("DEBUG: i2c_slave_write_byte - Bit %d = %d\n", i, bit);
        
        // Wait for SCL to go high
        while (gpioRead(config->scl_pin) == 0) {
            gpioDelay(config->bit_delay / 2);
        }
        
        // Wait for SCL to go low
        while (gpioRead(config->scl_pin) == 1) {
            gpioDelay(config->bit_delay / 2);
        }
    }
    
    // Release SDA to read ACK
    printf("DEBUG: i2c_slave_write_byte - Waiting for ACK\n");
    gpioWrite(config->sda_pin, 1);
    sda_set_mode(config, PI_INPUT);
    
    // Wait for SCL to go high
    while (gpioRead(config->scl_pin) == 0) {
        gpioDelay(config->bit_delay / 2);
    }
    
    // Read ACK bit
    ack = gpioRead(config->sda_pin);
    printf("DEBUG: i2c_slave_write_byte - ACK bit = %d (0=ACK, 1=NACK)\n", ack);
    
    // Wait for SCL to go low
    while (gpioRead(config->scl_pin) == 1) {
        gpioDelay(config->bit_delay / 2);
    }
    
    printf("DEBUG: i2c_slave_write_byte - Completed, returning %d\n", (ack == 0) ? 0 : -1);
    
    return (ack == 0) ? 0 : -1; // Return 0 on success (ACK received)
}

// Master writes multiple bytes
int i2c_master_write(I2C_Config *config, uint8_t *data, int length) {
    int i;
    
    printf("DEBUG: i2c_master_write - Starting, %d bytes\n", length);
    
    // Send start condition
    i2c_start(config);
    
    // Send slave address with write bit (0)
    if (i2c_write_byte(config, (config->slave_address << 1) | 0) != 0) {
        printf("DEBUG: i2c_master_write - Address NACK, stopping\n");
        i2c_stop(config);
        return -1; // NACK received
    }
    
    // Send data
    for (i = 0; i < length; i++) {
        if (i2c_write_byte(config, data[i]) != 0) {
            printf("DEBUG: i2c_master_write - Data NACK at byte %d, stopping\n", i);
            i2c_stop(config);
            return -1; // NACK received
        }
    }
    
    // Send stop condition
    i2c_stop(config);
    printf("DEBUG: i2c_master_write - Completed successfully\n");
    
    return 0; // Success
}

// Master reads multiple bytes
int i2c_master_read(I2C_Config *config, uint8_t *buffer, int length) {
    int i;
    
    printf("DEBUG: i2c_master_read - Starting, requesting %d bytes\n", length);
    
    // Send start condition
    i2c_start(config);
    
    // Send slave address with read bit (1)
    if (i2c_write_byte(config, (config->slave_address << 1) | 1) != 0) {
        printf("DEBUG: i2c_master_read - Address NACK, stopping\n");
        i2c_stop(config);
        return -1; // NACK received
    }
    
    printf("DEBUG: i2c_master_read - Address ACK received, reading data\n");
    
    // Read data
    for (i = 0; i < length - 1; i++) {
        buffer[i] = i2c_read_byte(config, 0); // Send ACK after each byte
        printf("DEBUG: i2c_master_read - Read byte %d: 0x%02X\n", i, buffer[i]);
    }
    buffer[length - 1] = i2c_read_byte(config, 1); // Send NACK after last byte
    printf("DEBUG: i2c_master_read - Read final byte: 0x%02X\n", buffer[length - 1]);
    
    // Send stop condition
    i2c_stop(config);
    printf("DEBUG: i2c_master_read - Completed successfully\n");
    
    return 0; // Success
}

// Slave reads multiple bytes
int i2c_slave_read(I2C_Config *config, uint8_t *buffer, int length) {
    int i;
    
    printf("DEBUG: i2c_slave_read - Starting\n");
    
    // Listen for address
    int rw = i2c_slave_listen(config);
    if (rw < 0) {
        printf("DEBUG: i2c_slave_read - Not our address\n");
        return -1; // Not our address
    }
    
    // Master wants to write to us, so we read
    if (rw == 0) {
        printf("DEBUG: i2c_slave_read - Master wants to write, reading data\n");
        // Read data
        for (i = 0; i < length; i++) {
            buffer[i] = i2c_slave_read_byte(config);
            printf("DEBUG: i2c_slave_read - Read byte 0x%02X\n", buffer[i]);
        }
        printf("DEBUG: i2c_slave_read - Completed, read %d bytes\n", i);
        return i;
    }
    
    printf("DEBUG: i2c_slave_read - Master wants to read, not write!\n");
    return -1; // Master wants to read, not write
}

// Slave writes multiple bytes
int i2c_slave_write(I2C_Config *config, uint8_t *data, int length) {
    int i;
    
    printf("DEBUG: i2c_slave_write - Starting\n");
    
    // Listen for address
    int rw = i2c_slave_listen(config);
    if (rw < 0) {
        printf("DEBUG: i2c_slave_write - Not our address\n");
        return -1; // Not our address
    }
    
    // Master wants to read from us, so we write
    if (rw == 1) {
        printf("DEBUG: i2c_slave_write - Master wants to read, writing data\n");
        // Write data
        for (i = 0; i < length; i++) {
            printf("DEBUG: i2c_slave_write - Writing byte 0x%02X\n", data[i]);
            if (i2c_slave_write_byte(config, data[i]) != 0) {
                printf("DEBUG: i2c_slave_write - NACK received after %d bytes\n", i);
                return i; // NACK received, stop sending
            }
        }
        printf("DEBUG: i2c_slave_write - Completed, wrote %d bytes\n", i);
        return i;
    }
    
    printf("DEBUG: i2c_slave_write - Master wants to write, not read!\n");
    return -1; // Master wants to write, not read
}