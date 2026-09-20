#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

#define ATTY_ADDR 0x30
#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9

// ============================================================================
// VIRTUAL REGISTER DEFINITIONS (0x00 .. 0x06)
// ============================================================================
#define REG_VAL     0x00  // Current position (int8_t: -128 .. +127)
#define REG_MIN     0x01  // Lower boundary limit (int8_t: -128 .. +127)
#define REG_MAX     0x02  // Upper boundary limit (int8_t: -128 .. +127)
#define REG_STEP    0x03  // Step increment per detent click (int8_t: 1 .. 127)
#define REG_DBL_CLK 0x04  // Double-click window (uint8_t: x10ms, e.g. 25 = 250ms)
#define LONG_PRESS  0x05  // Long-press threshold (uint8_t: x10ms, e.g. 60 = 600ms)
#define REG_BTN_EVT 0x06  // Button event status (0=None, 1=Click, 2=Long, 3=Double)

// ============================================================================
// BUTTON EVENT CONSTANTS
// ============================================================================
#define BTN_EVT_NONE   0  // Idle / Event acknowledged
#define BTN_EVT_CLICK  1  // Single click
#define BTN_EVT_LONG   2  // Long press
#define BTN_EVT_DOUBLE 3  // Double click

// Hardware I2C Display
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

// Write 1 single 8-bit register
bool writeReg(uint8_t reg, int8_t val) {
  Wire.beginTransmission(ATTY_ADDR);
  Wire.write(reg);
  Wire.write((uint8_t)val);
  return (Wire.endTransmission() == 0);
}

// Read 1 single 8-bit register
bool readReg(uint8_t reg, int8_t &val) {
  Wire.beginTransmission(ATTY_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission() != 0) return false;

  delayMicroseconds(50);
  if (Wire.requestFrom((uint8_t)ATTY_ADDR, (uint8_t)1) != 1) return false;
  val = (int8_t)Wire.read();
  return true;
}

// Configure 8-bit limits & initial position using clean symbolic names
void configureEncoder(int8_t minVal, int8_t maxVal, int8_t stepVal, int8_t initialVal = 0) {
  writeReg(REG_MIN, minVal);
  delayMicroseconds(100); 
  writeReg(REG_MAX, maxVal);
  delayMicroseconds(100); 
  writeReg(REG_STEP, stepVal);
  delayMicroseconds(100); 
  writeReg(REG_VAL, initialVal);
  delayMicroseconds(100); 
}


int currentMode = 1;
const int TOTAL_MODES = 4;
String modeName = "";

void setMode(int m) {
  switch (m) {
    case 1:
      modeName = "1:0..100 S:33";
      configureEncoder(0, 100, 33);
      break;
    case 2:
      modeName = "2:-100..100 S:5";
      configureEncoder(-100, 100, 5);
      break;
    case 3:
      modeName = "3:0..100 V:50";
      configureEncoder(0, 100, 5, 50);
      break;
    case 4:
      modeName = "4:0..100 S:1";
      configureEncoder(0, 100, 1);
      break;
    default:
      break;
  }
}

void setup() {
  Serial.begin(115200);
  // CRITICAL: Prevents native USB-CDC from blocking when Serial Monitor is closed!
  Serial.setTxTimeoutMs(0);

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 100000);
  Wire.setTimeOut(200);  // 200ms accommodates ATtiny85 USI clock stretching

  u8g2.begin();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  Wire.setTimeOut(200);

  setMode(currentMode);
}

int8_t currentVal = 0;
String lastBtn = "NONE";
bool isConnected = false;
uint8_t failCount = 0;
unsigned long lastDisplay = 0;

void loop() {
  int8_t tempVal = 0;
  int8_t btnRaw = 0;

  // Single 1-byte reads using symbolic constants
  bool valOk = readReg(REG_VAL, tempVal);
  delayMicroseconds(50);
  bool btnOk = readReg(REG_BTN_EVT, btnRaw);

  // Connection Health Monitor
  if (valOk && btnOk) {
    failCount = 0;
    isConnected = true;
    currentVal = tempVal;
  } else {
    failCount++;
    if (failCount >= 3) isConnected = false;
  }

  // Button Events
  if (btnRaw != BTN_EVT_NONE) {
    writeReg(REG_BTN_EVT, BTN_EVT_NONE);  // Acknowledge & clear flag

    if (btnRaw == BTN_EVT_CLICK) {
      lastBtn = "CLICK (NEXT)";
      Serial.println("Button: Single Click");
      currentMode++;
      if (currentMode > TOTAL_MODES)
        currentMode = 1;
      setMode(currentMode);
    } else if (btnRaw == BTN_EVT_LONG) {
      lastBtn = "LONG PRESS (RESET)";
      Serial.println("Button: Long Press -> Reset");

      if (currentMode == 3)
        writeReg(REG_VAL, 50);  // Reset position to 0
      else
        writeReg(REG_VAL, 0);  // Reset position to 0

    } else if (btnRaw == BTN_EVT_DOUBLE) {
      lastBtn = "DOUBLE CLICK (MIDPOINT)";
      Serial.println("Button: Double Click -> Midpoint");
      writeReg(REG_VAL, 25);
    }
  }

  // Refresh OLED Display at 10Hz
  unsigned long now = millis();
  if (now - lastDisplay >= 100) {
    lastDisplay = now;

    u8g2.clearBuffer();
    u8g2.drawStr(0, 12, modeName.c_str());

    if (isConnected) {
      String s = "Val: " + String(currentVal);
      u8g2.drawStr(0, 32, s.c_str());
      u8g2.drawStr(80, 32, "[OK]");
    } else {
      u8g2.drawStr(0, 32, "Val: ---");
      u8g2.drawStr(70, 32, "[DISCNT]");
    }

    String b = "Btn: " + lastBtn;
    u8g2.drawStr(0, 52, b.c_str());
    u8g2.sendBuffer();
  }

  delay(20);  // 50 Hz poll loop
}
