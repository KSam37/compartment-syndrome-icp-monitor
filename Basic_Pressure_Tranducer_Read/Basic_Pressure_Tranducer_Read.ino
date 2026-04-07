#include <Wire.h>

// Sensor and Pin Definitions
#define I2C_ADDR 0x28   // Default I2C address for ABP2...2A3
#define SDA_PIN D4      // XIAO ESP32-C5 SDA
#define SCL_PIN D5      // XIAO ESP32-C5 SCL

// Honeywell ABP2 Transfer Function Constants (10% to 90% calibration)
const float P_MIN = 0.0;          // Minimum pressure in psi
const float P_MAX = 1.0;          // Maximum pressure in psi
const float OUT_MIN = 1677722.0;  // 10% of 2^24 counts
const float OUT_MAX = 15099494.0; // 90% of 2^24 counts

void setup() {
  Serial.begin(115200);
  
  // Wait for serial monitor to open
  while (!Serial) {
    delay(10);
  }

  // Initialize the I2C bus
  Wire.begin(SDA_PIN, SCL_PIN);
  Serial.println("Honeywell ABP2 Pressure Sensor Initialized.");
}

void loop() {
  // Step 1: Send the command to trigger a measurement
  Wire.beginTransmission(I2C_ADDR);
  Wire.write(0xAA); // Command to measure
  Wire.write(0x00); 
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) {
    Serial.println("Sensor not found. Check wiring and pull-up resistors!");
    delay(2000);
    return;
  }

  // Step 2: Wait for the sensor to process the measurement
  delay(10); 

  // Step 3: Request 7 bytes of data from the sensor
  Wire.requestFrom((uint8_t)I2C_ADDR, (uint8_t)7);
  
  if (Wire.available() == 7) {
    uint8_t status = Wire.read();
    
    // Read 24-bit pressure data
    uint8_t p1 = Wire.read();
    uint8_t p2 = Wire.read();
    uint8_t p3 = Wire.read();
    
    // Read 24-bit temperature data
    uint8_t t1 = Wire.read();
    uint8_t t2 = Wire.read();
    uint8_t t3 = Wire.read();

    // Step 4: Check ABP2 Status Byte
    // Bit 6 (0x40) = 1 means "Powered On"
    // Bit 5 (0x20) = 1 means "Busy"
    // A perfectly clean, ready-to-read status byte is exactly 0x40.
    if (status != 0x40) {
      Serial.print("Data not ready or error. Status byte: 0x");
      Serial.println(status, HEX);
      delay(1000);
      return; // Skip this loop iteration and try again
    }

    // Combine the 3 bytes into a single 32-bit integer
    uint32_t press_counts = ((uint32_t)p1 << 16) | ((uint32_t)p2 << 8) | p3;
    uint32_t temp_counts = ((uint32_t)t1 << 16) | ((uint32_t)t2 << 8) | t3;

    // Step 5: Apply the transfer function to calculate actual PSI
    float pressure_psi = ((press_counts - OUT_MIN) * (P_MAX - P_MIN)) / (OUT_MAX - OUT_MIN) + P_MIN;
    
    // Calculate temperature in Celsius (Standard ABP2 formula)
    float temp_c = ((temp_counts * 200.0) / 16777215.0) - 50.0;

    // Step 6: Print to Serial Monitor
    Serial.print("Pressure: ");
    Serial.print(pressure_psi, 4); // Print with 4 decimal places
    Serial.print(" psi  |  Temp: ");
    Serial.print(temp_c, 2);
    Serial.println(" °C");
    
  } else {
    Serial.println("Failed to read enough bytes from sensor.");
  }

  delay(500);
}
