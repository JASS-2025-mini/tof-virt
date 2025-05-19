#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <sys/time.h>
#include <gpiod.h>

// GPIO configuration
#define GPIO_CHIP           "gpiochip0"
#define SDA_PIN             23  // GPIO pin for SDA
#define SCL_PIN             24  // GPIO pin for SCL

// VL53L0X I2C address
#define VL53L0X_ADDR        0x29

// VL53L0X registers
#define REG_IDENTIFICATION_MODEL_ID       0xC0
#define REG_IDENTIFICATION_REVISION_ID    0xC2
#define REG_VHV_CONFIG_PAD_SCL_SDA_EXTSUP_HV 0x89
#define REG_SYSTEM_INTERRUPT_CONFIG_GPIO 0x0A
#define REG_GPIO_HV_MUX_ACTIVE_HIGH      0x84
#define REG_SYSTEM_INTERRUPT_CLEAR       0x0B
#define REG_RESULT_INTERRUPT_STATUS      0x13
#define REG_SYSRANGE_START               0x00
#define REG_RESULT_RANGE_STATUS          0x14

// Device state
#define STATE_IDLE          0
#define STATE_MEASURING     1
#define STATE_ADDR_MATCHED  2
#define STATE_REG_SELECTED  3
#define STATE_REGISTER_READ 4

// I2C state machine
#define I2C_IDLE            0
#define I2C_START           1
#define I2C_ADDR_BITS       2
#define I2C_ADDR_ACK        3
#define I2C_REG_BITS        4
#define I2C_REG_ACK         5
#define I2C_DATA_BITS       6
#define I2C_DATA_ACK        7
#define I2C_STOP            8
#define I2C_REGISTER_READ   9

// Register array size
#define REGISTER_SIZE       256

// Global variables
struct gpiod_chip *chip;
struct gpiod_line *sda_line;
struct gpiod_line *scl_line;
uint8_t registers[REGISTER_SIZE];
int device_state = STATE_IDLE;
int i2c_state = I2C_IDLE;
uint16_t current_distance = 1000;  // Default distance in mm
volatile int running = 1;
uint8_t current_register = 0;
uint8_t current_byte = 0;
int bit_count = 0;
int rw_flag = 0;  // 0 = write, 1 = read
unsigned long measurement_start_time = 0;
int measurement_in_progress = 0;

// Prototypes
void init_registers();
void start_measurement();
void check_measurement();
void update_distance();
void sigint_handler(int sig);
void setup_gpio();
void cleanup_gpio();
void i2c_slave_loop();
unsigned long get_timestamp_ms();

// Helper function to set GPIO as input (with pull-up)
void set_input_pullup(struct gpiod_line *line) {
    gpiod_line_release(line);
    gpiod_line_request_input_flags(line, "vl53l0x-emu", GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP);
}

// Helper function to set GPIO as output low
void set_output_low(struct gpiod_line *line) {
    gpiod_line_release(line);
    gpiod_line_request_output(line, "vl53l0x-emu", 0);
}

// Helper function to set GPIO as output high
void set_output_high(struct gpiod_line *line) {
    gpiod_line_release(line);
    gpiod_line_request_output(line, "vl53l0x-emu", 1);
}

// Read GPIO value
int read_gpio(struct gpiod_line *line) {
    return gpiod_line_get_value(line);
}

// Get current time in milliseconds
unsigned long get_timestamp_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000UL) + (tv.tv_usec / 1000UL);
}

// Initialize registers with default values
void init_registers() {
    memset(registers, 0, REGISTER_SIZE);
    
    registers[REG_IDENTIFICATION_MODEL_ID] = 0xEE;  // Model ID
    registers[REG_IDENTIFICATION_REVISION_ID] = 0x10;  // Revision ID
    registers[REG_VHV_CONFIG_PAD_SCL_SDA_EXTSUP_HV] = 0x00;
    registers[REG_SYSTEM_INTERRUPT_CONFIG_GPIO] = 0x00;
    registers[REG_GPIO_HV_MUX_ACTIVE_HIGH] = 0x01;
    registers[REG_SYSTEM_INTERRUPT_CLEAR] = 0x00;
    registers[REG_RESULT_INTERRUPT_STATUS] = 0x00;
    registers[REG_SYSRANGE_START] = 0x00;
    registers[REG_RESULT_RANGE_STATUS] = 0x00;
    
    // Initialize distance value (2 bytes, little endian)
    registers[REG_RESULT_RANGE_STATUS + 10] = (current_distance >> 8) & 0xFF;  // High byte
    registers[REG_RESULT_RANGE_STATUS + 11] = current_distance & 0xFF;         // Low byte
}

// Check if a measurement has completed
void check_measurement() {
    if (measurement_in_progress) {
        unsigned long current_time = get_timestamp_ms();
        
        // Simulate measurement time (approx 75ms)
        if (current_time - measurement_start_time >= 75) {
            // Measurement is complete
            
            // Update distance value registers (2 bytes, little endian)
            registers[REG_RESULT_RANGE_STATUS + 10] = (current_distance >> 8) & 0xFF;  // High byte
            registers[REG_RESULT_RANGE_STATUS + 11] = current_distance & 0xFF;         // Low byte
            
            // Set interrupt status to indicate measurement complete
            registers[REG_RESULT_INTERRUPT_STATUS] = 0x07;
            measurement_in_progress = 0;
            device_state = STATE_IDLE;
            printf("Measurement complete: %u mm\n", current_distance);
            
            // Update distance for next measurement
            update_distance();
        }
    }
}

// Start measurement
void start_measurement() {
    printf("Starting measurement...\n");
    device_state = STATE_MEASURING;
    measurement_in_progress = 1;
    measurement_start_time = get_timestamp_ms();
    registers[REG_SYSRANGE_START] = 0x00;  // Auto-clear the start bit
    registers[REG_RESULT_INTERRUPT_STATUS] = 0x00;  // Clear interrupt status
}

// Update distance (simulate random changes)
void update_distance() {
    // Simulate random distance changes (between 100mm and 2000mm)
    int change = (rand() % 101) - 50;  // -50 to +50
    int new_distance = current_distance + change;
    if (new_distance < 100) new_distance = 100;
    if (new_distance > 2000) new_distance = 2000;
    current_distance = new_distance;
}

// Signal handler for Ctrl+C
void sigint_handler(int sig) {
    running = 0;
}

// Setup GPIO
void setup_gpio() {
    // Open GPIO chip
    chip = gpiod_chip_open_by_name(GPIO_CHIP);
    if (!chip) {
        perror("Failed to open GPIO chip");
        exit(1);
    }
    
    // Get SDA and SCL lines
    sda_line = gpiod_chip_get_line(chip, SDA_PIN);
    if (!sda_line) {
        perror("Failed to get SDA line");
        gpiod_chip_close(chip);
        exit(1);
    }
    
    scl_line = gpiod_chip_get_line(chip, SCL_PIN);
    if (!scl_line) {
        perror("Failed to get SCL line");
        gpiod_line_release(sda_line);
        gpiod_chip_close(chip);
        exit(1);
    }
    
    // Set SDA and SCL as inputs with pull-up initially
    set_input_pullup(sda_line);
    set_input_pullup(scl_line);
}

// Cleanup GPIO
void cleanup_gpio() {
    // Release GPIO lines
    gpiod_line_release(sda_line);
    gpiod_line_release(scl_line);
    
    // Close GPIO chip
    gpiod_chip_close(chip);
}

// Main I2C slave processing loop
void i2c_slave_loop() {
    int scl_prev = 1;
    int sda_prev = 1;
    int scl_curr, sda_curr;
    uint8_t addr = 0;
    
    i2c_state = I2C_IDLE;
    
    while (running) {
        // Check if measurement has completed
        check_measurement();
        
        // Read current SCL and SDA states
        scl_curr = read_gpio(scl_line);
        sda_curr = read_gpio(sda_line);
        
        // Detect START condition (SDA falling while SCL high)
        if (scl_curr && scl_prev && sda_prev && !sda_curr) {
            i2c_state = I2C_START;
            addr = 0;
            bit_count = 0;
            printf("I2C START detected\n");
        }
        
        // Detect STOP condition (SDA rising while SCL high)
        else if (scl_curr && scl_prev && !sda_prev && sda_curr) {
            i2c_state = I2C_IDLE;
            printf("I2C STOP detected\n");
            
            // Release SDA pin (set to input)
            set_input_pullup(sda_line);
        }
        
        // Process bits on SCL rising edge
        else if (!scl_prev && scl_curr) {
            // SCL rising edge - read data bit
            switch (i2c_state) {
                case I2C_START:
                    i2c_state = I2C_ADDR_BITS;
                    bit_count = 0;
                    addr = 0;
                    break;
                
                case I2C_ADDR_BITS:
                    // Receive 7-bit address + R/W bit
                    if (bit_count < 7) {
                        addr = (addr << 1) | (read_gpio(sda_line) ? 1 : 0);
                    } else {
                        // 8th bit is R/W flag (0=write, 1=read)
                        rw_flag = read_gpio(sda_line) ? 1 : 0;
                        printf("Address received: 0x%02X, R/W: %d\n", addr, rw_flag);
                        
                        // Check if this is our address
                        if (addr == VL53L0X_ADDR) {
                            i2c_state = I2C_ADDR_ACK;
                            device_state = STATE_ADDR_MATCHED;
                        } else {
                            i2c_state = I2C_IDLE;
                        }
                    }
                    bit_count++;
                    break;
                
                case I2C_REG_BITS:
                    // Receive register address
                    if (bit_count < 8) {
                        current_register = (current_register << 1) | (read_gpio(sda_line) ? 1 : 0);
                        bit_count++;
                    } else {
                        i2c_state = I2C_REG_ACK;
                        printf("Register selected: 0x%02X\n", current_register);
                        device_state = STATE_REG_SELECTED;
                    }
                    break;
                
                case I2C_DATA_BITS:
                    // Receive data byte
                    if (bit_count < 8) {
                        current_byte = (current_byte << 1) | (read_gpio(sda_line) ? 1 : 0);
                        bit_count++;
                    } else {
                        i2c_state = I2C_DATA_ACK;
                        printf("Data received: 0x%02X\n", current_byte);
                        
                        // Handle register write
                        if (current_register == REG_SYSRANGE_START && current_byte == 0x01) {
                            // Start measurement
                            if (device_state == STATE_IDLE || device_state == STATE_REG_SELECTED) {
                                start_measurement();
                            }
                        } else {
                            // Store value in register
                            registers[current_register] = current_byte;
                        }
                        
                        // Auto-increment register address
                        current_register++;
                    }
                    break;
                
                case I2C_REGISTER_READ:
                    // Send next bit of register value if master is reading
                    if (bit_count < 8) {
                        // Output bit on SDA
                        if (registers[current_register] & (0x80 >> bit_count)) {
                            set_input_pullup(sda_line);  // Let pull-up set high
                        } else {
                            set_output_low(sda_line);
                        }
                        bit_count++;
                    } else {
                        i2c_state = I2C_DATA_ACK;
                        // Release SDA for master ACK/NACK
                        set_input_pullup(sda_line);
                        
                        printf("Sent register 0x%02X value: 0x%02X\n", current_register, registers[current_register]);
                        
                        // Special handling for interrupt status register
                        if (current_register == REG_RESULT_INTERRUPT_STATUS && 
                            registers[REG_RESULT_INTERRUPT_STATUS] == 0x07) {
                            // Clear interrupt status after read
                            registers[REG_RESULT_INTERRUPT_STATUS] = 0x00;
                        }
                        
                        // Auto-increment register address
                        current_register++;
                    }
                    break;
            }
        }
        
        // Process ACK on SCL falling edge after 8 bits
        else if (scl_prev && !scl_curr) {
            // SCL falling edge - prepare for ACK/data output
            switch (i2c_state) {
                case I2C_ADDR_ACK:
                    if (device_state == STATE_ADDR_MATCHED) {
                        // Send ACK (pull SDA low)
                        set_output_low(sda_line);
                        
                        // Prepare for next state
                        if (rw_flag == 0) {
                            // Write operation - expect register address
                            i2c_state = I2C_REG_BITS;
                            current_register = 0;
                            bit_count = 0;
                        } else {
                            // Read operation - send register value
                            i2c_state = I2C_REGISTER_READ;
                            bit_count = 0;
                        }
                    }
                    break;
                
                case I2C_REG_ACK:
                    // Send ACK for register address
                    set_output_low(sda_line);
                    
                    // Prepare for data bits
                    i2c_state = I2C_DATA_BITS;
                    current_byte = 0;
                    bit_count = 0;
                    break;
                
                case I2C_DATA_ACK:
                    if (!rw_flag) {
                        // Send ACK for received data
                        set_output_low(sda_line);
                        
                        // Prepare for more data
                        i2c_state = I2C_DATA_BITS;
                        current_byte = 0;
                        bit_count = 0;
                    } else {
                        // Read master's ACK/NACK
                        if (read_gpio(sda_line)) {
                            // NACK received - end transmission
                            i2c_state = I2C_IDLE;
                            set_input_pullup(sda_line);
                        } else {
                            // ACK received - send next byte
                            i2c_state = I2C_REGISTER_READ;
                            bit_count = 0;
                        }
                    }
                    break;
            }
        }
        
        // Remember previous SCL and SDA states
        scl_prev = scl_curr;
        sda_prev = sda_curr;
        
        // Very short delay to prevent CPU hogging
        usleep(1);
    }
}

int main(int argc, char *argv[]) {
    // Set up signal handler
    signal(SIGINT, sigint_handler);
    
    // Initialize random seed
    srand(time(NULL));
    
    // Initialize registers
    init_registers();
    
    // Setup GPIO
    setup_gpio();
    
    printf("VL53L0X GPIO emulator starting (using libgpiod)\n");
    printf("Using GPIO %d for SDA and GPIO %d for SCL\n", SDA_PIN, SCL_PIN);
    printf("Press Ctrl+C to exit\n");
    
    // Run I2C slave loop
    i2c_slave_loop();
    
    printf("\nEmulator stopped\n");
    cleanup_gpio();
    
    return 0;
}