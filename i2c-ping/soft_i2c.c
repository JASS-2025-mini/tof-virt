// soft_i2c_fixed.c - Fixed software I2C implementation
#include "soft_i2c.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>
#include <errno.h>

// Proper implementation of mode switching for SDA
static int sda_set_mode(I2C_Config *config, int mode) {
    gpiod_line_release(config->sda_line);
    
    if (mode == 0) {
        // Output mode
        if (gpiod_line_request_output(config->sda_line, "i2c_sda_out", 1) < 0) {
            fprintf(stderr, "Failed to set SDA as output: %s\n", strerror(errno));
            return -1;
        }
    } else {
        // Input mode
        if (gpiod_line_request_input(config->sda_line, "i2c_sda_in") < 0) {
            fprintf(stderr, "Failed to set SDA as input: %s\n", strerror(errno));
            return -1;
        }
    }
    return 0;
}

// SCL mode switching not needed - master always controls SCL
// static int scl_set_mode(I2C_Config *config, int mode) {
//     gpiod_line_release(config->scl_line);
//     
//     if (mode == 0) {
//         // Output mode
//         if (gpiod_line_request_output(config->scl_line, "i2c_scl_out", 1) < 0) {
//             fprintf(stderr, "Failed to set SCL as output: %s\n", strerror(errno));
//             return -1;
//         }
//     } else {
//         // Input mode
//         if (gpiod_line_request_input(config->scl_line, "i2c_scl_in") < 0) {
//             fprintf(stderr, "Failed to set SCL as input: %s\n", strerror(errno));
//             return -1;
//         }
//     }
//     return 0;
// }

// Initialize GPIO using libgpiod
int i2c_init(I2C_Config *config) {
    // Open GPIO chip
    config->chip = gpiod_chip_open_by_name("gpiochip0");
    if (!config->chip) {
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
    
    // Configure pins as outputs initially
    if (gpiod_line_request_output(config->sda_line, "i2c_sda", 1) < 0) {
        fprintf(stderr, "Failed to configure SDA line as output: %s\n", strerror(errno));
        gpiod_chip_close(config->chip);
        return -1;
    }
    
    if (gpiod_line_request_output(config->scl_line, "i2c_scl", 1) < 0) {
        fprintf(stderr, "Failed to configure SCL line as output: %s\n", strerror(errno));
        gpiod_chip_close(config->chip);
        return -1;
    }
    
    // Default bit delay if not specified  
    if (config->bit_delay == 0) {
        config->bit_delay = 2000;  // 2000 microseconds
    }
    
    printf("GPIO initialized: SDA=GPIO%d, SCL=GPIO%d, bit_delay=%dus\n", 
           config->sda_pin, config->scl_pin, config->bit_delay);
    
    return 0;
}

// Initialize I2C for slave (input mode for reading)
int i2c_init_slave(I2C_Config *config) {
    config->chip = gpiod_chip_open("/dev/gpiochip0");
    if (!config->chip) {
        fprintf(stderr, "Failed to open GPIO chip: %s\n", strerror(errno));
        return -1;
    }
    
    config->sda_line = gpiod_chip_get_line(config->chip, config->sda_pin);
    config->scl_line = gpiod_chip_get_line(config->chip, config->scl_pin);
    
    if (!config->sda_line || !config->scl_line) {
        fprintf(stderr, "Failed to get GPIO lines\n");
        gpiod_chip_close(config->chip);
        return -1;
    }
    
    // Configure pins as inputs for slave
    if (gpiod_line_request_input(config->sda_line, "i2c_sda_slave") < 0) {
        fprintf(stderr, "Failed to configure SDA line as input: %s\n", strerror(errno));
        gpiod_chip_close(config->chip);
        return -1;
    }
    
    if (gpiod_line_request_input(config->scl_line, "i2c_scl_slave") < 0) {
        fprintf(stderr, "Failed to configure SCL line as input: %s\n", strerror(errno));
        gpiod_chip_close(config->chip);
        return -1;
    }
    
    if (config->bit_delay == 0) {
        config->bit_delay = 2000;
    }
    
    printf("GPIO initialized for slave: SDA=GPIO%d, SCL=GPIO%d, bit_delay=%dus\n", 
           config->sda_pin, config->scl_pin, config->bit_delay);
    
    return 0;
}

// Helper function for slave to send ACK/NACK
int i2c_slave_send_ack(I2C_Config *config, int ack) {
    // Reconfigure SDA as output to send ACK
    if (sda_set_mode(config, 0) < 0) {
        return -1;
    }
    
    gpiod_line_set_value(config->sda_line, ack ? 1 : 0);
    
    // Wait for master to bring SCL high
    int timeout = 0;
    while (gpiod_line_get_value(config->scl_line) == 0 && timeout < 1000) {
        usleep(config->bit_delay / 10);
        timeout++;
    }
    
    // Wait for master to bring SCL low
    timeout = 0;
    while (gpiod_line_get_value(config->scl_line) == 1 && timeout < 1000) {
        usleep(config->bit_delay / 10);
        timeout++;
    }
    
    // Release SDA and reconfigure back as input
    if (sda_set_mode(config, 1) < 0) {
        return -1;
    }
    
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

// Generate I2C start condition
int i2c_start(I2C_Config *config) {
    // Ensure both lines are high initially
    gpiod_line_set_value(config->sda_line, 1);
    gpiod_line_set_value(config->scl_line, 1);
    usleep(config->bit_delay);
    
    // START: SDA goes low while SCL is high
    gpiod_line_set_value(config->sda_line, 0);
    usleep(config->bit_delay);
    
    // Then bring SCL low
    gpiod_line_set_value(config->scl_line, 0);
    usleep(config->bit_delay);
    
    return 0;
}

// Generate I2C stop condition
void i2c_stop(I2C_Config *config) {
    // Ensure SDA is low and SCL is low
    gpiod_line_set_value(config->sda_line, 0);
    gpiod_line_set_value(config->scl_line, 0);
    usleep(config->bit_delay);
    
    // Bring SCL high first
    gpiod_line_set_value(config->scl_line, 1);
    usleep(config->bit_delay);
    
    // STOP: SDA goes high while SCL is high
    gpiod_line_set_value(config->sda_line, 1);
    usleep(config->bit_delay);
}

// Write a byte to I2C bus
int i2c_write_byte(I2C_Config *config, uint8_t byte) {
    int i;
    
    // Make sure SDA is in output mode
    if (sda_set_mode(config, 0) < 0) {
        return -1;
    }
    
    // Send 8 bits, MSB first
    for (i = 7; i >= 0; i--) {
        int bit = (byte >> i) & 1;
        gpiod_line_set_value(config->sda_line, bit);
        usleep(config->bit_delay);
        
        gpiod_line_set_value(config->scl_line, 1);
        usleep(config->bit_delay);
        
        gpiod_line_set_value(config->scl_line, 0);
        usleep(config->bit_delay);
    }
    
    // Switch to input mode to read ACK
    if (sda_set_mode(config, 1) < 0) {
        return -1;
    }
    
    // Clock ACK bit
    gpiod_line_set_value(config->scl_line, 1);
    usleep(config->bit_delay);
    
    int ack = gpiod_line_get_value(config->sda_line);
    
    gpiod_line_set_value(config->scl_line, 0);
    usleep(config->bit_delay);
    
    // Switch back to output mode
    if (sda_set_mode(config, 0) < 0) {
        return -1;
    }
    
    return ack ? -1 : 0;  // Return 0 on ACK, -1 on NACK
}

// Read a byte from I2C bus
uint8_t i2c_read_byte(I2C_Config *config, int ack) {
    int i;
    uint8_t byte = 0;
    
    // Switch SDA to input mode
    if (sda_set_mode(config, 1) < 0) {
        return 0xFF;
    }
    
    // Read 8 bits
    for (i = 7; i >= 0; i--) {
        gpiod_line_set_value(config->scl_line, 1);
        usleep(config->bit_delay);
        
        if (gpiod_line_get_value(config->sda_line)) {
            byte |= (1 << i);
        }
        
        gpiod_line_set_value(config->scl_line, 0);
        usleep(config->bit_delay);
    }
    
    // Switch to output mode to send ACK/NACK
    if (sda_set_mode(config, 0) < 0) {
        return byte;
    }
    
    gpiod_line_set_value(config->sda_line, ack ? 1 : 0);
    
    gpiod_line_set_value(config->scl_line, 1);
    usleep(config->bit_delay);
    gpiod_line_set_value(config->scl_line, 0);
    usleep(config->bit_delay);
    
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
    
    // Wait for activity
    int activity_detected = 0;
    int timeout_count = 0;
    
    while (!activity_detected && timeout_count < 10000) {
        int sda_val = gpiod_line_get_value(config->sda_line);
        int scl_val = gpiod_line_get_value(config->scl_line);
        
        if (sda_val == 0 || scl_val == 0) {
            activity_detected = 1;
            break;
        }
        
        usleep(config->bit_delay / 4);
        timeout_count++;
    }
    
    if (!activity_detected) {
        return -1;
    }
    
    usleep(config->bit_delay);
    
    // Read address byte
    for (i = 7; i >= 0; i--) {
        // Wait for SCL high
        int timeout = 0;
        while (gpiod_line_get_value(config->scl_line) == 0 && timeout < 1000) {
            usleep(config->bit_delay / 10);
            timeout++;
        }
        
        if (timeout >= 1000) {
            return -1;
        }
        
        // Read bit
        if (gpiod_line_get_value(config->sda_line)) {
            address |= (1 << i);
        }
        
        // Wait for SCL low
        timeout = 0;
        while (gpiod_line_get_value(config->scl_line) == 1 && timeout < 1000) {
            usleep(config->bit_delay / 10);
            timeout++;
        }
    }
    
    read_write_bit = address & 0x01;
    address >>= 1;
    
    if (address != config->slave_address) {
        return -1;
    }
    
    // Send ACK
    if (i2c_slave_send_ack(config, 0) < 0) {
        return -1;
    }
    
    return read_write_bit;
}

// Slave reads a byte
int i2c_slave_read_byte(I2C_Config *config) {
    int i;
    uint8_t byte = 0;
    
    // Make sure SDA is in input mode
    if (sda_set_mode(config, 1) < 0) {
        return -1;
    }
    
    // Read 8 bits
    for (i = 7; i >= 0; i--) {
        // Wait for SCL high
        while (gpiod_line_get_value(config->scl_line) == 0) {
            usleep(1);
        }
        
        // Read bit
        if (gpiod_line_get_value(config->sda_line)) {
            byte |= (1 << i);
        }
        
        // Wait for SCL low
        while (gpiod_line_get_value(config->scl_line) == 1) {
            usleep(1);
        }
    }
    
    // Send ACK
    if (i2c_slave_send_ack(config, 0) < 0) {
        return -1;
    }
    
    return byte;
}

// Slave writes a byte
int i2c_slave_write_byte(I2C_Config *config, uint8_t byte) {
    int i;
    
    // Switch to output mode
    if (sda_set_mode(config, 0) < 0) {
        return -1;
    }
    
    // Write 8 bits
    for (i = 7; i >= 0; i--) {
        int bit = (byte >> i) & 1;
        gpiod_line_set_value(config->sda_line, bit);
        
        // Wait for SCL high
        while (gpiod_line_get_value(config->scl_line) == 0) {
            usleep(1);
        }
        
        // Wait for SCL low
        while (gpiod_line_get_value(config->scl_line) == 1) {
            usleep(1);
        }
    }
    
    // Switch to input mode for ACK
    if (sda_set_mode(config, 1) < 0) {
        return -1;
    }
    
    // Wait for ACK clock
    while (gpiod_line_get_value(config->scl_line) == 0) {
        usleep(1);
    }
    
    int ack = gpiod_line_get_value(config->sda_line);
    
    while (gpiod_line_get_value(config->scl_line) == 1) {
        usleep(1);
    }
    
    return ack ? -1 : 0;
}

// Master writes multiple bytes
int i2c_master_write(I2C_Config *config, uint8_t *data, int length) {
    int i;
    
    i2c_start(config);
    
    // Send address with write bit
    if (i2c_write_byte(config, (config->slave_address << 1) | 0) != 0) {
        i2c_stop(config);
        return -1;
    }
    
    // Send data
    for (i = 0; i < length; i++) {
        if (i2c_write_byte(config, data[i]) != 0) {
            i2c_stop(config);
            return -1;
        }
    }
    
    i2c_stop(config);
    return 0;
}

// Master reads multiple bytes
int i2c_master_read(I2C_Config *config, uint8_t *buffer, int length) {
    int i;
    
    i2c_start(config);
    
    // Send address with read bit
    if (i2c_write_byte(config, (config->slave_address << 1) | 1) != 0) {
        i2c_stop(config);
        return -1;
    }
    
    // Read data
    for (i = 0; i < length - 1; i++) {
        buffer[i] = i2c_read_byte(config, 0); // ACK
    }
    buffer[length - 1] = i2c_read_byte(config, 1); // NACK
    
    i2c_stop(config);
    return 0;
}

// Function to read byte with STOP condition check
int i2c_slave_read_byte_with_stop_check(I2C_Config *config, uint8_t *byte) {
    // For now, just use regular read
    int result = i2c_slave_read_byte(config);
    if (result < 0) {
        return -1;
    }
    *byte = (uint8_t)result;
    return 0;
}

// Slave write/read stubs for completeness
int i2c_slave_write(I2C_Config *config, uint8_t *data, int length) {
    (void)config;
    (void)data;
    (void)length;
    return -1;  // Not implemented
}

int i2c_slave_read(I2C_Config *config, uint8_t *buffer, int length) {
    (void)config;
    (void)buffer;
    (void)length;
    return -1;  // Not implemented
}

// Debug function
void i2c_debug_status(I2C_Config *config) {
    int sda_state = gpiod_line_get_value(config->sda_line);
    int scl_state = gpiod_line_get_value(config->scl_line);
    printf("DEBUG: SDA=%d, SCL=%d\n", sda_state, scl_state);
}