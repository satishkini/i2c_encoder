/**
 * @file Attiny_Encoder_8Bit.ino
 * @brief Pure 8-Bit Signed (-128 to +127) I2C Slave Rotary Encoder for ATtiny85
 * @address 0x29
 * 
 * Hardware:
 *   PB0 -> SDA (Pull-up to 3.3V)
 *   PB1 -> DT  (On-board LED removed)
 *   PB2 -> SCL (Pull-up to 3.3V)
 *   PB3 -> CLK
 *   PB4 -> SW  (Pushbutton to GND)
 */

#include <TinyWireS.h>

#define I2C_SLAVE_ADDR 0x30

const uint8_t pinCLK = 3; // PB3
const uint8_t pinDT  = 1; // PB1
const uint8_t pinSW  = 4; // PB4

// Clean 7-Byte 8-Bit Register Map
// [0]: VAL  (int8_t)
// [1]: MIN  (int8_t)
// [2]: MAX  (int8_t)
// [3]: STEP (int8_t)
// [4]: DBL_WIN (uint8_t, in 10ms units, e.g. 25 = 250ms)
// [5]: LONG_TH (uint8_t, in 10ms units, e.g. 60 = 600ms)
// [6]: BTN_EVT (uint8_t, 0=None, 1=Click, 2=Long, 3=Double)
volatile uint8_t i2c_regs[7];
volatile uint8_t reg_position = 0;

int8_t current_val = 0;
uint8_t last_encoder_state = 0;
int8_t sub_step_counter = 0;

// Button state
bool lastRawButtonState   = HIGH;
bool debouncedButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long DEBOUNCE_DELAY = 35;

unsigned long buttonPressTime = 0;
unsigned long buttonReleaseTime = 0;
bool isPressed = false;
bool longPressTriggered = false;
bool waitingForDouble = false;

// Quadrature 4-state Gray Code Transition Table
const int8_t ENCODER_TABLE[16] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0
};

void setup() {
    delay(100);

    pinMode(pinCLK, INPUT_PULLUP);
    pinMode(pinDT,  INPUT_PULLUP);
    pinMode(pinSW,  INPUT_PULLUP);

    // Initial 8-bit defaults
    i2c_regs[0] = 0;    // VAL  = 0
    i2c_regs[1] = 0;    // MIN  = 0 
    i2c_regs[2] = 100;  // MAX  = 100
    i2c_regs[3] = 5;    // STEP = 5
    i2c_regs[4] = 25;   // DBL  = 25 * 10ms = 250ms
    i2c_regs[5] = 60;   // LONG = 60 * 10ms = 600ms
    i2c_regs[6] = 0;    // BTN  = 0 (Idle)

    current_val = 0;
    last_encoder_state = ((digitalRead(pinCLK) << 1) | digitalRead(pinDT)) & 0x03;

    TinyWireS.begin(I2C_SLAVE_ADDR);
    TinyWireS.onReceive(receiveEvent);
    TinyWireS.onRequest(requestEvent);
}

void loop() {
    int8_t val_min  = (int8_t)i2c_regs[1];
    int8_t val_max  = (int8_t)i2c_regs[2];
    int8_t val_step = (int8_t)i2c_regs[3];
    unsigned long double_window = (unsigned long)i2c_regs[4] * 10; // Convert to ms
    unsigned long long_window   = (unsigned long)i2c_regs[5] * 10; // Convert to ms
    unsigned long now = millis();

    // 1. ROTARY ENCODER DECODING (4 PULSES PER DETENT CLICK)
    uint8_t curr_clk = digitalRead(pinCLK);
    uint8_t curr_dt  = digitalRead(pinDT);
    uint8_t current_state = ((curr_clk << 1) | curr_dt) & 0x03;

    if (current_state != last_encoder_state) {
        uint8_t table_index = (last_encoder_state << 2) | current_state;
        int8_t movement = ENCODER_TABLE[table_index];

        if (movement != 0) {
            sub_step_counter += movement;

            // 1 Detent Click = 4 transitions
            if (sub_step_counter >= 4) {
                sub_step_counter = 0;
                if (current_val < val_max) {
                    int16_t next = (int16_t)current_val + val_step;
                    // Snapping
                    if (next >= val_max || (val_max - next) < val_step) {
                        current_val = val_max;
                    } else {
                        current_val = (int8_t)next;
                    }
                    i2c_regs[0] = (uint8_t)current_val; // Atomic single byte!
                }
            } else if (sub_step_counter <= -4) {
                sub_step_counter = 0;
                if (current_val > val_min) {
                    int16_t next = (int16_t)current_val - val_step;
                    // Snapping
                    if (next <= val_min || (next - val_min) < val_step) {
                        current_val = val_min;
                    } else {
                        current_val = (int8_t)next;
                    }
                    i2c_regs[0] = (uint8_t)current_val; // Atomic single byte!
                }
            }
        }
        last_encoder_state = current_state;
    }

    // 2. PUSHBUTTON GESTURE ENGINE
    bool rawButtonState = digitalRead(pinSW);
    if (rawButtonState != lastRawButtonState) {
        lastDebounceTime = now;
    }
    lastRawButtonState = rawButtonState;

    if ((now - lastDebounceTime) > DEBOUNCE_DELAY) {
        if (rawButtonState != debouncedButtonState) {
            debouncedButtonState = rawButtonState;

            if (debouncedButtonState == LOW) {
                buttonPressTime = now;
                isPressed = true;
                longPressTriggered = false;
            } else {
                isPressed = false;
                buttonReleaseTime = now;
                if (!longPressTriggered) {
                    if (waitingForDouble) {
                        i2c_regs[6] = 3; // Double Click
                        waitingForDouble = false;
                    } else {
                        waitingForDouble = true;
                    }
                }
            }
        }
    }

    if (isPressed && !longPressTriggered) {
        if (now - buttonPressTime >= long_window) {
            i2c_regs[6] = 2; // Long Press
            longPressTriggered = true;
            waitingForDouble = false;
        }
    }

    if (waitingForDouble && (now - buttonReleaseTime > double_window)) {
        if (!isPressed && !longPressTriggered) {
            i2c_regs[6] = 1; // Single Click
        }
        waitingForDouble = false;
    }

    TinyWireS_stop_check();
}

void receiveEvent(uint8_t howMany) {
    if (howMany < 1) return;
    reg_position = TinyWireS.read();
    howMany--;

    while (howMany > 0) {
        uint8_t incoming = TinyWireS.read();
        howMany--;
        if (reg_position < sizeof(i2c_regs)) {
            i2c_regs[reg_position] = incoming;
            if (reg_position == 0) {
                // Live sync if master writes new position
                current_val = (int8_t)incoming;
            }
            reg_position++;
        }
    }
}

void requestEvent() {
    if (reg_position >= sizeof(i2c_regs)) {
        reg_position = 0;
    }
    TinyWireS.write(i2c_regs[reg_position]);
    reg_position++;
    if (reg_position >= sizeof(i2c_regs)) {
        reg_position = 0;
    }
}