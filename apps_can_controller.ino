#include <Arduino_CAN.h>   // Arduino CAN bus library

/* ===================== CAN CONFIGURATION ===================== */

// CAN message ID sent when a system failure occurs
static uint32_t const CAN_FAIL_ID = 0xFF;

// CAN message ID for normal throttle transmission
static uint32_t const CAN_MSG_ID = 0x20;

// 8-byte CAN payload for throttle data
uint8_t throttleBytes[8] = {0};

/* ================ POTENTIOMETER CONFIGURATION ================ */

// Valid ADC operating ranges for each potentiometer
// These ranges correspond to expected voltage limits
const int POT_ONE_OPERATING_RANGE[2] = {102, 718}; // ~0.5V - 3.5V
const int POT_TWO_OPERATING_RANGE[2] = {205, 820}; // ~1.0V - 4.0V

// Maximum allowed difference (%) between the two throttle sensors
const int POT_DIFF_THRESHOLD = 10;

// Minimum throttle percentage at which plausibility checks are enforced
const int MIN_THROTTLE_ACTIVIATION = 10;

// Analog input pins for the potentiometers
const int POT_ONE_PIN = A0;
const int POT_TWO_PIN = A1;

/* ===================== STATE VARIABLES ===================== */

// Raw ADC readings
int potOneVal = 0;
int potTwoVal = 0;

// Normalized throttle percentages (0–100%)
float potOnePercentage = 0;
float potTwoPercentage = 0;

// Average throttle percentage
float throttlePercentage = 0;

// Fault flags
bool potDisagreement = false;
bool voltageIrregularity = false;
bool systemFailure = false;

/* ===================== FUNCTIONS ===================== */

/**
 * Reads potentiometer values, converts them to percentages,
 * clamps values to valid limits, and computes average throttle.
 */
void updateValues() {
  potOneVal = analogRead(POT_ONE_PIN);
  potTwoVal = analogRead(POT_TWO_PIN);

  // Convert ADC readings to percentage of operating range
  potOnePercentage = (potOneVal - POT_ONE_OPERATING_RANGE[0]) * 100.0 /
                     (POT_ONE_OPERATING_RANGE[1] - POT_ONE_OPERATING_RANGE[0]);

  potTwoPercentage = (potTwoVal - POT_TWO_OPERATING_RANGE[0]) * 100.0 /
                     (POT_TWO_OPERATING_RANGE[1] - POT_TWO_OPERATING_RANGE[0]);

  // Clamp values to 0–100%
  potOnePercentage = constrain(potOnePercentage, 0, 100);
  potTwoPercentage = constrain(potTwoPercentage, 0, 100);

  // Average the two sensors
  throttlePercentage = (potOnePercentage + potTwoPercentage) * 0.5;
}

/**
 * Prints current system state and sensor diagnostics to Serial.
 */
void showDiagnostics() {
  updateValues();

  Serial.println();
  Serial.println(String("System failure (power reset required if 1): ") + systemFailure);
  Serial.println(String("Pot disagreement (power reset required if 1): ") + potDisagreement);
  Serial.println(String("Voltage irregularity (power reset required if 1): ") + voltageIrregularity);

  Serial.println(String("Current pot one reading: ") + potOneVal);
  Serial.println(String("Current pot two reading: ") + potTwoVal);

  Serial.println(String("Pot one percentage: ") + potOnePercentage);
  Serial.println(String("Pot two percentage: ") + potTwoPercentage);

  Serial.println(String("Average throttle percentage: ") + throttlePercentage);
  Serial.println();
}

/**
 * Places the system into a fail-safe state. 
 * Throttle output is disabled and a CAN failure message is sent.
 */
void failSystem() {
  Serial.println("System failure! Throttling is now disabled.");

  // Send CAN failure message
  CanMsg const msg(CanStandardId(CAN_FAIL_ID), sizeof(0), 0);

  if (int const rc = CAN.write(msg); rc < 0) {
    Serial.print("CAN.write(...) failed with error code ");
    Serial.println(rc);
    for (;;) {} // Halt execution
  }

  systemFailure = true;
  throttlePercentage = 0;
  showDiagnostics();
}

/**
 * Checks whether potentiometer readings are within valid voltage ranges.
 * A repeated fault triggers a system failure.
 */
void operatingRangeCheck() {
  if (potOneVal < POT_ONE_OPERATING_RANGE[0] || potOneVal > POT_ONE_OPERATING_RANGE[1] ||
      potTwoVal < POT_TWO_OPERATING_RANGE[0] || potTwoVal > POT_TWO_OPERATING_RANGE[1]) {

    if (voltageIrregularity) {
      failSystem();   // Second consecutive fault
    } else {
      Serial.println("Voltage irregularity detected!");
      voltageIrregularity = true;
    }
    return;
  }

  voltageIrregularity = false;
}

/**
 * Checks for implausible disagreement between the two throttle sensors.
 * Enforced only when throttle is actively pressed.
 */
void potDiffImplausibilityCheck() {
  if (throttlePercentage > MIN_THROTTLE_ACTIVIATION &&
      fabs(potOnePercentage - potTwoPercentage) > POT_DIFF_THRESHOLD) {

    if (potDisagreement) {
      failSystem();   // Second consecutive fault
    } else {
      Serial.println("Potentiometer disagreement detected!");
      potDisagreement = true;
    }
    return;
  }

  potDisagreement = false;
}

/* ===================== ARDUINO SETUP ===================== */

void setup() {
  Serial.begin(115200);
  Serial.println("Starting APPS...");

  while (!Serial) {} // Wait for Serial connection

  // Initialize CAN bus at 250 kbps
  if (!CAN.begin(CanBitRate::BR_250k)) {
    Serial.println("CAN.begin(...) failed.");
    for (;;) {}
  }
}

/* ===================== MAIN LOOP ===================== */

void loop() {
  // If system has failed, only show diagnostics
  if (systemFailure) {
    showDiagnostics();
    delay(3000);
    return;
  }

  updateValues();

  operatingRangeCheck();
  if (systemFailure) return;

  potDiffImplausibilityCheck();
  if (systemFailure) return;

  Serial.println(String("Avg pedal throttle: ") + String(throttlePercentage, 2) + "%");

  // Convert throttle percentage to an integer representation
  int tempValue = round(throttlePercentage * 100.0f);

  // Encode throttle digits into CAN payload
  throttleBytes[0] = (tempValue / 10000) % 10;
  throttleBytes[1] = (tempValue / 1000) % 10;
  throttleBytes[2] = (tempValue / 100) % 10;
  throttleBytes[3] = (tempValue / 10) % 10;
  throttleBytes[4] = tempValue % 10;
  throttleBytes[5] = 0;
  throttleBytes[6] = 0;
  throttleBytes[7] = 0;

  // Transmit throttle data over CAN
  CanMsg const msg(CanStandardId(CAN_MSG_ID), sizeof(throttleBytes), throttleBytes);

  if (int const rc = CAN.write(msg); rc < 0) {
    Serial.print("CAN.write(...) failed with error code ");
    Serial.println(rc);
    for (;;) {}
  }

  delay(100);
}
