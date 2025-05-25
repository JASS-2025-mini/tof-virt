# Virtual VL53L0X I2C Implementation

This project implements a virtual VL53L0X Time-of-Flight (ToF) distance sensor using software I2C (bit-banging) between two Raspberry Pi devices. The virtual sensor emulates the VL53L0X protocol and must be indistinguishable from a real sensor to the master device.

## Overview

The system consists of:
- **Master Pi**: Acts as the controller reading from the "VL53L0X sensor"
- **Slave Pi**: Emulates a VL53L0X sensor, responding to I2C commands
- **Software I2C**: Custom bit-banging implementation (hardware I2C not used)

## Hardware Configuration

### Required Hardware
- 2x Raspberry Pi (tested on Pi 3/4)
- 2x Pull-up resistors (4.7kΩ recommended)
- Jumper wires for connections
- Optional: Logic analyzer for debugging

### Pin Connections
```
Master Pi          Slave Pi
---------          --------
GPIO22 (SDA) <---> GPIO22 (SDA)
GPIO23 (SCL) <---> GPIO23 (SCL)
GND          <---> GND
```

**Important**: External pull-up resistors are required on both SDA and SCL lines to 3.3V.

### Network Configuration
- Master Pi: 192.168.0.102 (user: pi)
- Slave Pi: 192.168.0.104 (user: pi)
- Both devices must be accessible via SSH with key-based authentication

## Software Architecture

### Key Components

1. **soft_i2c.c/h** - Software I2C implementation
   - Bit-banging using GPIO
   - Master and slave functions
   - Timing-critical operations

2. **i2c_vl53l0x_master.c** - Master test program
   - Reads sensor identification
   - Performs distance measurements
   - Calculates success statistics

3. **vl53l0x_slave.c** - Virtual VL53L0X implementation
   - Emulates VL53L0X register map
   - Responds to I2C commands
   - Simulates distance measurements

4. **vl53l0x_io.h** - Common constants and configuration

## How It Works

### I2C Communication Flow

1. **Initialization**
   - Master sends START condition
   - Sends slave address (0x29) + write bit
   - Slave ACKs if address matches

2. **Register Write**
   - Master sends register address
   - Optionally sends data byte
   - Slave ACKs each byte

3. **Register Read**
   - Master writes register address
   - Sends repeated START
   - Sends slave address + read bit
   - Slave sends register data
   - Master ACKs/NACKs

### VL53L0X Protocol Emulation

The slave emulates key VL53L0X registers:

| Register | Value | Description |
|----------|-------|-------------|
| 0xC0 | 0xEE | Model ID |
| 0xC2 | 0x10 | Revision ID |
| 0x00 | - | Start measurement |
| 0x13 | 0x07 | Interrupt status (data ready) |
| 0x14 | 0x00 | Range status (valid) |
| 0x1E-0x1F | Distance | 16-bit distance value |

### Measurement Cycle

1. Master writes 0x01 to register 0x00 (start measurement)
2. Master polls register 0x13 until bit 0 is set (data ready)
3. Master reads register 0x14 (range status)
4. Master reads registers 0x1E-0x1F (16-bit distance)
5. Slave increments simulated distance by 10mm each measurement

## Configuration Constants

All timing constants are defined in `vl53l0x_io.h`:

### I2C Timing
- **I2C_BIT_DELAY_US** (2000μs): Delay between bit transitions
  - Lower = faster but less reliable
  - Higher = slower but more stable
  - Affects overall I2C clock speed

### Master Configuration
- **MEASUREMENT_FREQUENCY_HZ** (5Hz): How often to take measurements
- **MAX_MEASUREMENTS** (250): Total measurements to perform
- **WRITE_READ_DELAY_US**: Delay between write and read operations
  - Calculated as 5% of measurement period
  - Gives slave time to process

### Slave Synchronization
- **RETRY_DELAY_US** (2000μs): Pause before each transaction
  - Critical for synchronization
  - Too low = missed START conditions
  - Too high = missed transactions

- **POST_TRANSACTION_DELAY_US** (500μs): Pause after successful transaction
  - Allows lines to stabilize
  - Prevents back-to-back transaction issues

- **MAX_CONSECUTIVE_FAILURES** (2): Failures before bus recovery
  - Triggers longer pause to resynchronize
  - Prevents permanent lockup

## Building and Deployment

### Local Development
```bash
# Edit files locally
vim i2c_vl53l0x_master.c

# Deploy to both Pis
./deploy.sh
```

### Manual Deployment
```bash
# Copy files to both Pis
scp *.c *.h Makefile pi@192.168.0.102:~/ping/
scp *.c *.h Makefile pi@192.168.0.104:~/ping/

# Build on each Pi
ssh pi@192.168.0.102 "cd ~/ping && make clean && make"
ssh pi@192.168.0.104 "cd ~/ping && make clean && make"
```

## Testing

### Basic Test Procedure

1. **Start the slave first** (on Pi at 192.168.0.104):
   ```bash
   ssh pi@192.168.0.104
   cd ~/ping
   sudo ./vl53l0x_slave
   ```

2. **Start the master** (on Pi at 192.168.0.102):
   ```bash
   ssh pi@192.168.0.102
   cd ~/ping
   sudo ./i2c_vl53l0x_master
   ```

3. **Monitor Output**
   - Master shows progress, measurements, and success rate
   - Slave shows each transaction and any errors
   - Goal is 100% success rate

### Expected Output

Master:
```
=== Device Identification ===
Model ID: 0xEE
Revision ID: 0x10

--- Measurement Cycle 1/250 (0.4%) - Success rate: 0.0% ---
1. Starting measurement...
2. Waiting for measurement completion...
3. Range status: 0x00
4. Distance: 510 mm
```

Slave:
```
Transaction 1: WRITE - Reg 0xC0
Transaction 2: READ - Reg 0xC0 = 0xEE - OK (next: 0xC1)
Transaction 3: WRITE - Reg 0x00 = 0x01 (start measurement)
```

### Troubleshooting

#### Low Success Rate (<90%)
1. Check pull-up resistors are installed
2. Verify GPIO connections
3. Adjust timing constants:
   - Increase `I2C_BIT_DELAY_US` for stability
   - Adjust `RETRY_DELAY_US` (1-3ms range)
   - Reduce `MEASUREMENT_FREQUENCY_HZ`

#### Complete Failure (0%)
1. Verify both Pis are using same GPIO pins
2. Check `I2C_BIT_DELAY_US` not too fast/slow
3. Ensure slave started before master
4. Check for GPIO conflicts with other services

#### Intermittent Failures
1. Indicates timing/synchronization issues
2. Try different `RETRY_DELAY_US` values
3. Check for electrical noise/interference
4. Verify stable power supply

## Test Results

### Success Rate vs RETRY_DELAY_US (at 5Hz)
| RETRY_DELAY_US | Success Rate | Notes |
|----------------|--------------|-------|
| 100μs | 56.7% | Too fast, many missed STARTs |
| 500μs | 60.0% | Still unreliable |
| 1000μs | 76.7% | Better but not stable |
| 1350μs | Testing... | Approaching 100%? |
| 1400μs | 97.6% | New record! |
| 1500μs | 92.8% | Very good |
| 1750μs | 81.2% | Worse than 1500μs |
| 2000μs | 86.8% | Good but not perfect |
| 3000μs | 4.0% | Too slow, misses transactions |

## Performance Tuning

### For Maximum Speed
- Decrease `I2C_BIT_DELAY_US` (minimum ~100μs)
- Increase `MEASUREMENT_FREQUENCY_HZ`
- Reduce all delay constants
- Risk: Lower reliability

### For Maximum Reliability
- Increase `I2C_BIT_DELAY_US` (2000-5000μs)
- Decrease `MEASUREMENT_FREQUENCY_HZ` (1-5Hz)
- Increase `RETRY_DELAY_US` (2000-3000μs)
- Trade-off: Slower operation

### Finding Optimal Settings
1. Start with default values
2. Run 250+ measurement test
3. If <95% success, increase delays
4. If 100% success, try reducing delays
5. Find balance between speed and reliability

## Known Issues and Limitations

1. **No Clock Stretching**: Slave cannot hold SCL low
2. **Fixed Timing**: No adaptive synchronization
3. **No Multi-Master**: Single master only
4. **GPIO Speed**: Limited by Linux GPIO subsystem
5. **No Hardware I2C**: Must use software implementation

## Debug Features

- Transaction counting on slave
- Automatic bus recovery after failures
- Progress and success rate display
- Detailed error messages
- Line state debugging (in soft_i2c.c)

## Future Improvements

1. Implement clock stretching for better synchronization
2. Add adaptive timing based on success rate
3. Implement full VL53L0X register set
4. Add CRC/checksum for data integrity
5. Support for multiple slaves
6. Real-time timing analysis

## References

- VL53L0X Datasheet: https://www.st.com/resource/en/datasheet/vl53l0x.pdf
- I2C Specification: https://www.nxp.com/docs/en/user-guide/UM10204.pdf
- libgpiod Documentation: https://git.kernel.org/pub/scm/libs/libgpiod/libgpiod.git/