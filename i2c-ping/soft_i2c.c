// soft_i2c.c
#include "soft_i2c.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>
#include <errno.h>

// Initialize GPIO using libgpiod
int i2c_init(I2C_Config *config) {
    // Open GPIO chip (usually gpiochip0 on most systems)
    config->chip = gpiod_chip_open_by_name("gpiochip0");
    if (!config->chip) {
        // Try alternative chip names
        config->chip = gpiod_chip_open_by_name("gpiochip1");
        if (!config->chip) {
            fprintf(stderr, "Failed to open GPIO chip\n");
            return -1;
        }
    }
    
    // Get GPIO lines
    config->sda_line = gpiod_chip_get_line(config->chip, config->sda_pin);
    config->scl_line = gpiod_chip_get_line(config->chip, config->scl_pin);
    
    if (!config->sda_line || !config->scl_line) {
        fprintf(stderr, "Failed to get GPIO lines\n");
        gpiod_chip_close(config->chip);
        return -1;
    }
    
    // Configure pins as open-drain outputs with pull-up (I2C standard)
    printf("DEBUG: Requesting SDA line (GPIO%d) as open-drain with pull-up\n", config->sda_pin);
    if (gpiod_line_request_output_flags(config->sda_line, "i2c_sda", 
                                        GPIOD_LINE_REQUEST_FLAG_OPEN_DRAIN | 
                                        GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP, 1) < 0) {
        fprintf(stderr, "Failed to configure SDA line (GPIO%d) as open-drain: %s\n", 
                config->sda_pin, strerror(errno));
        gpiod_chip_close(config->chip);
        return -1;
    }
    
    printf("DEBUG: Requesting SCL line (GPIO%d) as open-drain with pull-up\n", config->scl_pin);
    if (gpiod_line_request_output_flags(config->scl_line, "i2c_scl", 
                                        GPIOD_LINE_REQUEST_FLAG_OPEN_DRAIN |
                                        GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP, 1) < 0) {
        fprintf(stderr, "Failed to configure SCL line (GPIO%d) as open-drain: %s\n", 
                config->scl_pin, strerror(errno));
        gpiod_chip_close(config->chip);
        return -1;
    }
    
    // Default bit delay if not specified  
    if (config->bit_delay == 0) {
        config->bit_delay = 2000;  // 2000 microseconds for better stability
    }
    
    printf("GPIO initialized: SDA=GPIO%d, SCL=GPIO%d, bit_delay=%dus\n", 
           config->sda_pin, config->scl_pin, config->bit_delay);
    
    return 0;
}

void i2c_cleanup(I2C_Config *config) {
    if (config->sda_line) {
        gpiod_line_release(config->sda_line);
    }
    if (config->scl_line) {
        gpiod_line_release(config->scl_line);
    }
    if (config->chip) {
        gpiod_chip_close(config->chip);
    }
}

// Debug function to display pin status
void i2c_debug_status(I2C_Config *config) {
    int sda_state = gpiod_line_get_value(config->sda_line);
    int scl_state = gpiod_line_get_value(config->scl_line);
    printf("DEBUG: SDA=%d, SCL=%d\n", sda_state, scl_state);
}

// Set SDA pin value (open-drain: 1=release line, 0=pull low)
static void sda_set_mode(I2C_Config *config, int mode) {
    (void)mode; // With open-drain, we always just set value
    // No mode switching needed with open-drain
}

// Set SCL pin value (open-drain: 1=release line, 0=pull low)  
static void scl_set_mode(I2C_Config *config, int mode) {
    (void)mode; // With open-drain, we always just set value
    // No mode switching needed with open-drain
}

// Check if bus is idle (both SDA and SCL high)
static int i2c_bus_is_idle(I2C_Config *config) {
    // With open-drain, just read current values
    usleep(config->bit_delay);
    
    int sda_state = gpiod_line_get_value(config->sda_line);
    int scl_state = gpiod_line_get_value(config->scl_line);
    
    return (sda_state == 1 && scl_state == 1);
}

// Generate I2C start condition
int i2c_start(I2C_Config *config) {
    // Wait for bus to be idle
    printf("DEBUG: i2c_start - Checking bus idle state\n");
    int retries = 10;
    while (!i2c_bus_is_idle(config) && retries-- > 0) {
        printf("DEBUG: i2c_start - Bus not idle, waiting...\n");
        usleep(config->bit_delay * 10);
    }
    
    if (retries <= 0) {
        printf("DEBUG: i2c_start - Bus busy, proceeding anyway\n");
    }
    
    // With open-drain, no mode switching needed
    printf("DEBUG: i2c_start - Preparing START condition\n");
    
    // Ensure both lines are high
    printf("DEBUG: i2c_start - Setting both lines high\n");
    gpiod_line_set_value(config->sda_line, 1);
    gpiod_line_set_value(config->scl_line, 1);
    usleep(config->bit_delay);
    
    // Generate start condition: SDA goes low while SCL is high
    printf("DEBUG: i2c_start - Generating START condition\n");
    gpiod_line_set_value(config->sda_line, 0);
    usleep(config->bit_delay);
    gpiod_line_set_value(config->scl_line, 0);
    
    printf("DEBUG: i2c_start - START condition complete\n");
    
    return 0;
}

// Generate I2C stop condition
void i2c_stop(I2C_Config *config) {
    // Ensure SDA is output
    sda_set_mode(config, 0);
    
    // Prepare for stop: ensure SCL is low, then set SDA low
    gpiod_line_set_value(config->scl_line, 0);
    gpiod_line_set_value(config->sda_line, 0);
    usleep(config->bit_delay);
    
    // Generate stop condition: SCL goes high, then SDA goes high
    gpiod_line_set_value(config->scl_line, 1);
    usleep(config->bit_delay);
    gpiod_line_set_value(config->sda_line, 1);
    usleep(config->bit_delay);
    
    printf("DEBUG: i2c_stop - STOP condition complete\n");
}

// Write a byte to I2C bus
int i2c_write_byte(I2C_Config *config, uint8_t byte) {
    int i;
    int ack;
    
    printf("DEBUG: i2c_write_byte - Writing byte 0x%02X\n", byte);
    
    sda_set_mode(config, 0);
    
    // Send 8 bits, MSB first
    for (i = 7; i >= 0; i--) {
        // Set SDA according to bit
        int bit = (byte >> i) & 1;
        printf("DEBUG: i2c_write_byte - Bit %d = %d\n", i, bit);
        
        gpiod_line_set_value(config->sda_line, bit);
        usleep(config->bit_delay);
        
        // Clock high and delay
        gpiod_line_set_value(config->scl_line, 1);
        usleep(config->bit_delay * 2);
        
        // Clock low and delay
        gpiod_line_set_value(config->scl_line, 0);
        usleep(config->bit_delay);
    }
    
    printf("DEBUG: i2c_write_byte - All bits sent, waiting for ACK\n");
    
    // Release SDA for slave acknowledgment
    gpiod_line_set_value(config->sda_line, 1);
    sda_set_mode(config, 1);
    
    // Clock high to read ACK bit
    gpiod_line_set_value(config->scl_line, 1);
    usleep(config->bit_delay);
    
    // Read ACK bit (0 = acknowledged)
    ack = gpiod_line_get_value(config->sda_line);
    printf("DEBUG: i2c_write_byte - ACK bit = %d (0=ACK, 1=NACK)\n", ack);
    
    // Clock low
    gpiod_line_set_value(config->scl_line, 0);
    usleep(config->bit_delay);
    
    // Restore SDA as output
    sda_set_mode(config, 0);
    
    printf("DEBUG: i2c_write_byte - Returning %d\n", (ack == 0) ? 0 : -1);
    
    return (ack == 0) ? 0 : -1; // Return 0 on success (ACK received)
}

// Read a byte from I2C bus
uint8_t i2c_read_byte(I2C_Config *config, int ack) {
    int i;
    uint8_t byte = 0;
    
    printf("DEBUG: i2c_read_byte - Starting (ack=%d)\n", ack);
    
    // Release SDA so slave can control it
    sda_set_mode(config, 1);
    
    // Read 8 bits
    for (i = 7; i >= 0; i--) {
        gpiod_line_set_value(config->scl_line, 1);
        usleep(config->bit_delay);
        
        if (gpiod_line_get_value(config->sda_line)) {
            byte |= (1 << i);
            printf("DEBUG: i2c_read_byte - Bit %d = 1\n", i);
        } else {
            printf("DEBUG: i2c_read_byte - Bit %d = 0\n", i);
        }
        
        gpiod_line_set_value(config->scl_line, 0);
        usleep(config->bit_delay);
    }
    
    // Send ACK/NACK
    sda_set_mode(config, 0);
    gpiod_line_set_value(config->sda_line, ack ? 1 : 0); // 0 = ACK, 1 = NACK
    printf("DEBUG: i2c_read_byte - Sending %s\n", ack ? "NACK" : "ACK");
    
    // Clock ACK/NACK bit
    gpiod_line_set_value(config->scl_line, 1);
    usleep(config->bit_delay);
    gpiod_line_set_value(config->scl_line, 0);
    usleep(config->bit_delay);
    
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
    sda_set_mode(config, 1);
    scl_set_mode(config, 1);
    
    printf("DEBUG: Set pins to input mode\n");
    
    // Wait for both lines to be high (idle) with timeout
    int timeout_count = 0;
    while ((gpiod_line_get_value(config->sda_line) == 0 || gpiod_line_get_value(config->scl_line) == 0) && timeout_count < 1000) {
        printf("DEBUG: Waiting for idle, SDA=%d, SCL=%d\n", 
               gpiod_line_get_value(config->sda_line), gpiod_line_get_value(config->scl_line));
        usleep(config->bit_delay);
        timeout_count++;
    }
    
    if (timeout_count >= 1000) {
        printf("DEBUG: Timeout waiting for idle state\n");
        return -1;
    }
    
    printf("DEBUG: Detected idle state, waiting for START\n");
    
    // Wait for SDA to go low while SCL is high (START condition) with timeout
    timeout_count = 0;
    while (gpiod_line_get_value(config->sda_line) == 1 && timeout_count < 5000) {
        if (gpiod_line_get_value(config->scl_line) == 0) {
            // SCL went low before SDA, not a START condition
            printf("DEBUG: SCL went low before SDA, not a START\n");
            usleep(config->bit_delay);
            timeout_count++;
            continue;
        }
        usleep(config->bit_delay);
        timeout_count++;
    }
    
    if (timeout_count >= 5000) {
        printf("DEBUG: Timeout waiting for START condition\n");
        return -1;
    }
    
    printf("DEBUG: Detected START condition\n");
    
    // Now SCL should go low with timeout
    timeout_count = 0;
    while (gpiod_line_get_value(config->scl_line) == 1 && timeout_count < 1000) {
        usleep(config->bit_delay);
        timeout_count++;
    }
    
    if (timeout_count >= 1000) {
        printf("DEBUG: Timeout waiting for SCL low after START\n");
        return -1;
    }
    
    printf("DEBUG: SCL went low after START\n");
    
    // Read 7-bit address + R/W bit with timeout
    for (i = 7; i >= 0; i--) {
        // Wait for SCL high
        timeout_count = 0;
        while (gpiod_line_get_value(config->scl_line) == 0 && timeout_count < 1000) {
            usleep(config->bit_delay / 2);
            timeout_count++;
        }
        
        if (timeout_count >= 1000) {
            printf("DEBUG: Timeout waiting for SCL high during address read\n");
            return -1;
        }
        
        int bit = gpiod_line_get_value(config->sda_line);
        printf("DEBUG: Read bit %d = %d\n", i, bit);
        
        if (bit) {
            address |= (1 << i);
        }
        
        // Wait for SCL low
        timeout_count = 0;
        while (gpiod_line_get_value(config->scl_line) == 1 && timeout_count < 1000) {
            usleep(config->bit_delay / 2);
            timeout_count++;
        }
        
        if (timeout_count >= 1000) {
            printf("DEBUG: Timeout waiting for SCL low during address read\n");
            return -1;
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
    sda_set_mode(config, 0);
    gpiod_line_set_value(config->sda_line, 0); // ACK
    
    // Wait for SCL to go high with timeout
    scl_set_mode(config, 1);
    timeout_count = 0;
    while (gpiod_line_get_value(config->scl_line) == 0 && timeout_count < 1000) {
        usleep(config->bit_delay / 2);
        timeout_count++;
    }
    
    if (timeout_count >= 1000) {
        printf("DEBUG: Timeout waiting for SCL high during ACK\n");
        return -1;
    }
    
    // Wait for SCL to go low with timeout
    timeout_count = 0;
    while (gpiod_line_get_value(config->scl_line) == 1 && timeout_count < 1000) {
        usleep(config->bit_delay / 2);
        timeout_count++;
    }
    
    if (timeout_count >= 1000) {
        printf("DEBUG: Timeout waiting for SCL low during ACK\n");
        return -1;
    }
    
    printf("DEBUG: ACK bit clocked\n");
    
    // Release SDA
    gpiod_line_set_value(config->sda_line, 1);
    
    printf("DEBUG: i2c_slave_listen returning %d\n", read_write_bit);
    
    return read_write_bit; // Return 1 if master wants to read, 0 if master wants to write
}

// Slave reads a byte
int i2c_slave_read_byte(I2C_Config *config) {
    int i;
    uint8_t byte = 0;
    
    printf("DEBUG: i2c_slave_read_byte - Starting\n");
    
    sda_set_mode(config, 1);
    scl_set_mode(config, 1);
    
    // Read 8 bits
    for (i = 7; i >= 0; i--) {
        // Wait for SCL to go high
        while (gpiod_line_get_value(config->scl_line) == 0) {
            usleep(config->bit_delay / 2);
        }
        
        // Read bit
        int bit = gpiod_line_get_value(config->sda_line);
        printf("DEBUG: i2c_slave_read_byte - Bit %d = %d\n", i, bit);
        
        if (bit) {
            byte |= (1 << i);
        }
        
        // Wait for SCL to go low
        while (gpiod_line_get_value(config->scl_line) == 1) {
            usleep(config->bit_delay / 2);
        }
    }
    
    // Send ACK
    printf("DEBUG: i2c_slave_read_byte - Sending ACK\n");
    sda_set_mode(config, 0);
    gpiod_line_set_value(config->sda_line, 0); // ACK
    
    // Wait for SCL to go high then low (clock ACK bit)
    scl_set_mode(config, 1);
    while (gpiod_line_get_value(config->scl_line) == 0) {
        usleep(config->bit_delay / 2);
    }
    while (gpiod_line_get_value(config->scl_line) == 1) {
        usleep(config->bit_delay / 2);
    }
    
    // Release SDA
    gpiod_line_set_value(config->sda_line, 1);
    sda_set_mode(config, 1);
    
    printf("DEBUG: i2c_slave_read_byte - Completed, read 0x%02X\n", byte);
    
    return byte;
}

// Function to read byte with STOP condition check
int i2c_slave_read_byte_with_stop_check(I2C_Config *config, uint8_t *byte) {
    int i;
    *byte = 0;
    
    printf("DEBUG: i2c_slave_read_byte_with_stop_check, bit_delay: %d - Starting\n", config->bit_delay);
    
    sda_set_mode(config, 1);
    scl_set_mode(config, 1);
    
    // Check for STOP condition (SDA rising while SCL high)
    while (gpiod_line_get_value(config->scl_line) == 1) {
        if (gpiod_line_get_value(config->sda_line) == 0) {
            int prev_sda = 0;
            while (gpiod_line_get_value(config->scl_line) == 1) {
                int current_sda = gpiod_line_get_value(config->sda_line);
                if (prev_sda == 0 && current_sda == 1) {
                    printf("DEBUG: i2c_slave_read_byte_with_stop_check - STOP detected\n");
                    return 0; // STOP condition
                }
                prev_sda = current_sda;
                usleep(config->bit_delay / 10);
            }
        }
        usleep(config->bit_delay / 10);
    }
    
    // Read 8 bits
    for (i = 7; i >= 0; i--) {
        // Wait for SCL to go high
        while (gpiod_line_get_value(config->scl_line) == 0) {
            usleep(config->bit_delay / 2);
        }
        
        // Read bit
        int bit = gpiod_line_get_value(config->sda_line);
        printf("DEBUG: i2c_slave_read_byte_with_stop_check - Bit %d = %d, bit_delay: %d\n", i, bit, config->bit_delay);
        
        if (bit) {
            *byte |= (1 << i);
        }
        
        // Wait for SCL to go low
        while (gpiod_line_get_value(config->scl_line) == 1) {
            usleep(config->bit_delay / 2);
        }
    }
    
    // Send ACK
    printf("DEBUG: i2c_slave_read_byte_with_stop_check - Sending ACK\n");
    sda_set_mode(config, 0);
    gpiod_line_set_value(config->sda_line, 0); // ACK
    
    // Wait for SCL to go high then low (clock ACK bit)
    scl_set_mode(config, 1);
    while (gpiod_line_get_value(config->scl_line) == 0) {
        usleep(config->bit_delay / 2);
    }
    while (gpiod_line_get_value(config->scl_line) == 1) {
        usleep(config->bit_delay / 2);
    }
    
    // Release SDA
    gpiod_line_set_value(config->sda_line, 1);
    sda_set_mode(config, 1);
    
    printf("DEBUG: i2c_slave_read_byte_with_stop_check - Read 0x%02X\n", *byte);
    
    return 1; // Successfully read byte
}

// Slave writes a byte
int i2c_slave_write_byte(I2C_Config *config, uint8_t byte) {
    int i;
    int ack;
    
    printf("DEBUG: i2c_slave_write_byte - Starting, writing 0x%02X\n", byte);
    
    sda_set_mode(config, 0);
    scl_set_mode(config, 1);
    
    // Write 8 bits
    for (i = 7; i >= 0; i--) {
        // Set SDA according to bit
        int bit = (byte >> i) & 1;
        gpiod_line_set_value(config->sda_line, bit);
        printf("DEBUG: i2c_slave_write_byte - Bit %d = %d\n", i, bit);
        
        // Wait for SCL to go high
        while (gpiod_line_get_value(config->scl_line) == 0) {
            usleep(config->bit_delay / 2);
        }
        
        // Wait for SCL to go low
        while (gpiod_line_get_value(config->scl_line) == 1) {
            usleep(config->bit_delay / 2);
        }
    }
    
    // Release SDA to read ACK
    printf("DEBUG: i2c_slave_write_byte - Waiting for ACK\n");
    gpiod_line_set_value(config->sda_line, 1);
    sda_set_mode(config, 1);
    
    // Wait for SCL to go high
    while (gpiod_line_get_value(config->scl_line) == 0) {
        usleep(config->bit_delay / 2);
    }
    
    // Read ACK bit
    ack = gpiod_line_get_value(config->sda_line);
    printf("DEBUG: i2c_slave_write_byte - ACK bit = %d (0=ACK, 1=NACK)\n", ack);
    
    // Wait for SCL to go low
    while (gpiod_line_get_value(config->scl_line) == 1) {
        usleep(config->bit_delay / 2);
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