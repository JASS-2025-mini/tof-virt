// vl53l0x_io.h - Common constants for VL53L0X I2C communication
#ifndef VL53L0X_IO_H
#define VL53L0X_IO_H

// GPIO pins
#define SDA_PIN 22                      // GPIO pin for SDA
#define SCL_PIN 23                      // GPIO pin for SCL

// I2C configuration
#define VL53L0X_ADDR 0x29               // VL53L0X I2C address
#define I2C_BIT_DELAY_US 2000           // Bit delay for I2C communication (2ms)

// Master timing constants
#define MEASUREMENT_FREQUENCY_HZ 5       // Measurement frequency in Hz
#define MAX_MEASUREMENTS 500             // Number of measurements to perform
#define MEASUREMENT_DELAY_US (1000000 / MEASUREMENT_FREQUENCY_HZ)  // Auto-calculated delay
#define WRITE_READ_DELAY_US (MEASUREMENT_DELAY_US / 20)  // 5% of measurement period

// Slave timing constants
#define START_WAIT_TIMEOUT 100000        // Timeout for waiting START condition
#define START_WAIT_DELAY 10              // Delay in microseconds for START detection
#define SCL_STABLE_COUNT 10              // Number of stable SCL readings required
#define DATA_CHECK_LOOPS 100             // Loops to check for incoming data
#define RETRY_DELAY_US 1400              // Delay before retry in microseconds
#define MAX_TRANSACTIONS 10              // Re-sync after this many transactions
#define MAX_CONSECUTIVE_FAILURES 2       // Force reset after this many failures
#define POST_TRANSACTION_DELAY_US 500    // Delay after successful transaction

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

#endif // VL53L0X_IO_H