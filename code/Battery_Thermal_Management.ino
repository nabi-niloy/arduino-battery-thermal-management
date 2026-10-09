 * ============================================================
 * ARDUINO-BASED INTELLIGENT BATTERY THERMAL MANAGEMENT SYSTEM
 * ============================================================
 *
 * Description:
 * This project monitors lithium-ion battery temperature using
 * two independent temperature sensors:
 *
 *   1. DS18B20  - Contact temperature sensor
 *   2. MLX90614 - Non-contact infrared temperature sensor
 *
 * An Arduino UNO processes the sensor readings and implements
 * a three-stage thermal protection system.
 *
 * ------------------------------------------------------------
 * THERMAL PROTECTION THRESHOLDS
 * ------------------------------------------------------------
 *
 * Temperature >= 32 C:
 *   Activate the buzzer for an audible warning.
 *
 * Temperature >= 35 C:
 *   Activate the cooling fan through Relay 1.
 *
 * Temperature >= 40 C:
 *   Disconnect the motor load through Relay 2.
 *
 * Temperature <= 32 C:
 *   Deactivate fan and buzzer, and reconnect the load.
 *
 * ------------------------------------------------------------
 * SENSOR FAILURE HANDLING
 * ------------------------------------------------------------
 *
 * Both sensors valid:
 *   Use the average of both temperature readings.
 *
 * Only one sensor valid:
 *   Use the remaining available sensor.
 *
 * Both sensors invalid:
 *   Activate fail-safe mode:
 *   - Motor load OFF
 *   - Cooling fan OFF
 *   - Buzzer OFF
 *   - Display sensor failure message
 *
 * ------------------------------------------------------------
 * HARDWARE
 * ------------------------------------------------------------
 *
 * Microcontroller : Arduino UNO
 * Sensors         : DS18B20, MLX90614
 * Display         : 16x2 I2C LCD
 * Actuators       : 12V cooling fan, 12V DC motor
 * Switching       : Two relay modules
 * Alarm           : Buzzer
 *
 * IMPORTANT:
 * The original report firmware uses D11 for the buzzer,
 * while the circuit description specifies D8.
 * Verify the actual hardware connection before uploading.
 *
 * ============================================================
 */


// ============================================================
// 1. REQUIRED LIBRARIES
// ============================================================

#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <LiquidCrystal_I2C.h>
#include <math.h>


// ============================================================
// 2. PIN DEFINITIONS AND CONFIGURATION
// ============================================================

// DS18B20 sensor data pin.
#define SENSOR_PIN   2

// Relay controlling the cooling fan.
#define FAN_RELAY    6

// Relay controlling the motor load.
#define LOAD_RELAY   7

// Buzzer output pin.
// Original firmware uses D11; report wiring table lists D8.
#define BUZZER_PIN   11

// Default I2C address of MLX90614.
#define MLX_ADDR     0x5A

// DS18B20 12-bit temperature conversion time (milliseconds).
#define CONV_DELAY   750

// Buzzer ON/OFF switching interval (milliseconds).
#define BUZZ_PERIOD  200


// ============================================================
// 3. INITIALIZE SENSOR AND DISPLAY OBJECTS
// ============================================================

// Create a OneWire communication bus for DS18B20.
OneWire oneWire(SENSOR_PIN);

// Initialize the DS18B20 sensor using the OneWire bus.
DallasTemperature ds18b20(&oneWire);

// Initialize the MLX90614 infrared temperature sensor.
Adafruit_MLX90614 mlx;

// Initialize the LCD.
// Address: 0x27
// Display: 16 columns x 2 rows.
LiquidCrystal_I2C lcd(0x27, 16, 2);


// ============================================================
// 4. SYSTEM STATE VARIABLES
// ============================================================

// Stores the current cooling fan state.
bool fanState = false;

// Stores the current motor load state.
// The motor starts in the ON state.
bool loadState = true;

// Indicates whether MLX90614 initialization succeeded.
bool mlxReady = false;


// ------------------------------------------------------------
// DS18B20 NON-BLOCKING CONVERSION STATE
// ------------------------------------------------------------

// Tracks whether a temperature conversion has been requested.
bool convRequested = false;

// Time at which the last conversion was requested.
unsigned long lastConvMs = 0;


// ------------------------------------------------------------
// BUZZER TIMING STATE
// ------------------------------------------------------------

// Time of the previous buzzer state change.
unsigned long lastBuzzMs = 0;

// false = buzzer silent
// true  = buzzer active
bool buzzPhase = false;


// ------------------------------------------------------------
// CACHED TEMPERATURE READINGS
// ------------------------------------------------------------

// Store the most recent valid temperature readings.
//
// NAN indicates that no valid measurement is available.

float ds_temp = NAN;
float mlx_temp = NAN;


// ============================================================
// 5. I2C BUS RECOVERY
// ============================================================

/*
 * Attempts to recover the I2C communication bus.
 *
 * I2C communication may become stuck if a device fails
 * or holds the data line LOW.
 *
 * Recovery sequence:
 *
 * 1. Configure SDA and SCL pins.
 * 2. Generate up to nine clock pulses.
 * 3. Attempt to generate an I2C STOP condition.
 * 4. Reinitialize the Wire communication interface.
 *
 * On Arduino UNO:
 *
 * A4 = SDA
 * A5 = SCL
 */

void recoverI2CBus() {

  pinMode(A4, INPUT);
  pinMode(A5, OUTPUT);

  // Generate up to nine SCL pulses.
  for (int i = 0; i < 9; i++) {

    // Exit when SDA is released.
    if (digitalRead(A4) == HIGH) {
      break;
    }

    digitalWrite(A5, HIGH);
    delayMicroseconds(5);

    digitalWrite(A5, LOW);
    delayMicroseconds(5);
  }

  // Attempt to generate the I2C STOP condition.
  pinMode(A4, OUTPUT);

  digitalWrite(A4, LOW);
  delayMicroseconds(5);

  digitalWrite(A5, HIGH);
  delayMicroseconds(5);

  digitalWrite(A4, HIGH);
  delayMicroseconds(5);

  // Reinitialize I2C communication.
  Wire.begin();
}


// ============================================================
// 6. CHECK I2C DEVICE AVAILABILITY
// ============================================================

/*
 * Checks whether a device responds at the specified
 * I2C address.
 *
 * Returns:
 *
 * true  -> Device acknowledged communication.
 * false -> No acknowledgement received.
 */

bool i2cDevicePresent(uint8_t address) {

  Wire.beginTransmission(address);

  return (Wire.endTransmission() == 0);
}


// ============================================================
// 7. FAIL-SAFE PROTECTION
// ============================================================

/*
 * Called when both temperature sensors are unavailable.
 *
 * The original firmware uses the following fail-safe state:
 *
 * Motor load: OFF
 * Cooling fan: OFF
 * Buzzer: OFF
 *
 * Note:
 * These are the original firmware's output states.
 * Their suitability for a safety-critical battery system
 * requires separate hardware and safety validation.
 */

void applyFailSafe() {

  // Update internal system states.
  loadState = false;
  fanState = false;
  buzzPhase = false;

  // Disconnect motor load.
  digitalWrite(LOAD_RELAY, HIGH);

  // Disable cooling fan.
  digitalWrite(FAN_RELAY, LOW);

  // Silence buzzer.
  digitalWrite(BUZZER_PIN, LOW);
}


// ============================================================
// 8. SYSTEM INITIALIZATION
// ============================================================

void setup() {

  // ----------------------------------------------------------
  // Configure output pins.
  // ----------------------------------------------------------

  pinMode(FAN_RELAY, OUTPUT);
  pinMode(LOAD_RELAY, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Start with the motor load enabled.
  // The original firmware uses active-LOW load relay logic.
  digitalWrite(LOAD_RELAY, LOW);


  // ----------------------------------------------------------
  // Initialize I2C communication.
  // ----------------------------------------------------------

  Wire.begin();


  // ----------------------------------------------------------
  // Initialize LCD display.
  // ----------------------------------------------------------

  lcd.init();
  lcd.backlight();


  // ----------------------------------------------------------
  // Initialize DS18B20.
  // ----------------------------------------------------------

  ds18b20.begin();

  // Do not block while waiting for temperature conversion.
  ds18b20.setWaitForConversion(false);


  // ----------------------------------------------------------
  // Initialize MLX90614.
  // ----------------------------------------------------------

  mlxReady = mlx.begin(MLX_ADDR, &Wire);
}


// ============================================================
// 9. MAIN CONTROL LOOP
// ============================================================

void loop() {

  // Current Arduino uptime in milliseconds.
  unsigned long now = millis();


  // ==========================================================
  // SECTION A: DS18B20 TEMPERATURE ACQUISITION
  // ==========================================================

  /*
   * The DS18B20 uses a non-blocking conversion process.
   *
   * First:
   *   Request a temperature conversion.
   *
   * Then:
   *   Wait until the conversion interval has elapsed.
   *
   * Finally:
   *   Read and validate the temperature.
   *
   * Cached values are used while conversion is in progress.
   */

  bool ds_ok = false;


  // ----------------------------------------------------------
  // Request temperature conversion.
  // ----------------------------------------------------------

  if (!convRequested) {

    // Verify that at least one DS18B20 device is detected.
    if (ds18b20.getDeviceCount() > 0) {

      ds18b20.requestTemperatures();

      // Record when conversion was requested.
      lastConvMs = now;

      convRequested = true;
    }
  }


  // ----------------------------------------------------------
  // Read completed temperature conversion.
  // ----------------------------------------------------------

  if (convRequested &&
      (now - lastConvMs >= CONV_DELAY)) {

    convRequested = false;

    // Read temperature in degrees Celsius.
    float reading = ds18b20.getTempCByIndex(0);


    // --------------------------------------------------------
    // Validate sensor measurement.
    // --------------------------------------------------------

    /*
     * Reject:
     *
     * - Disconnected sensor reading
     * - 85 C startup / invalid conversion value
     * - Values below -20 C
     * - Values above 100 C
     *
     * The -20 to 100 C limits are firmware validation limits,
     * not the full manufacturer's sensor measurement range.
     */

    bool bad =
        (reading == DEVICE_DISCONNECTED_C)
        || (reading == 85.0)
        || (reading < -20.0)
        || (reading > 100.0);


    if (!bad) {

      // Store valid reading.
      ds_temp = reading;

      ds_ok = true;

    } else {

      // Mark the sensor reading as invalid.
      ds_temp = NAN;
    }

  } else if (convRequested) {

    /*
     * A conversion is still in progress.
     *
     * Continue using the cached temperature reading
     * if one is available.
     */

    ds_ok = !isnan(ds_temp);

  } else {

    // No conversion and no valid sensor measurement.
    ds_ok = false;

    ds_temp = NAN;
  }


  // ==========================================================
  // SECTION B: MLX90614 INFRARED TEMPERATURE ACQUISITION
  // ==========================================================

  /*
   * The MLX90614 measures object temperature without
   * requiring direct physical contact.
   *
   * Communication occurs through the I2C bus.
   */

  bool mlx_ok = false;


  // ----------------------------------------------------------
  // Check whether the MLX90614 responds over I2C.
  // ----------------------------------------------------------

  if (!i2cDevicePresent(MLX_ADDR)) {

    // Attempt I2C bus recovery.
    recoverI2CBus();

    // Mark the sensor as unavailable.
    mlxReady = false;
    mlx_temp = NAN;

    // Reinitialize LCD after the recovery attempt.
    lcd.init();
    lcd.backlight();

  } else {

    // --------------------------------------------------------
    // Reinitialize MLX90614 if necessary.
    // --------------------------------------------------------

    if (!mlxReady) {

      mlxReady = mlx.begin(MLX_ADDR, &Wire);
    }


    // --------------------------------------------------------
    // Acquire and validate infrared temperature.
    // --------------------------------------------------------

    if (mlxReady) {

      // Read object temperature in degrees Celsius.
      double reading = mlx.readObjectTempC();


      /*
       * Reject:
       *
       * - NaN readings
       * - Temperature below -40 C
       * - Temperature above 125 C
       */

      bool bad =
          isnan(reading)
          || reading < -40.0
          || reading > 125.0;


      if (!bad) {

        mlx_temp = (float)reading;

        mlx_ok = true;

      } else {

        mlx_temp = NAN;

        mlxReady = false;
      }
    }
  }


  // ==========================================================
  // SECTION C: EFFECTIVE TEMPERATURE CALCULATION
  // ==========================================================

  /*
   * Sensor fusion strategy:
   *
   * Case 1: Both sensors valid
   *         -> Average the two readings.
   *
   * Case 2: Only DS18B20 valid
   *         -> Use DS18B20 reading.
   *
   * Case 3: Only MLX90614 valid
   *         -> Use MLX90614 reading.
   *
   * Case 4: Both sensors invalid
   *         -> Effective temperature remains NAN.
   */

  float effectiveTemp = NAN;


  if (ds_ok && mlx_ok) {

    // Both sensors operational.
    effectiveTemp = (ds_temp + mlx_temp) / 2.0;

  } else if (ds_ok) {

    // Only DS18B20 operational.
    effectiveTemp = ds_temp;

  } else if (mlx_ok) {

    // Only MLX90614 operational.
    effectiveTemp = mlx_temp;
  }


  // ==========================================================
  // SECTION D: SENSOR FAILURE PROTECTION
  // ==========================================================

  /*
   * If neither sensor provides a valid reading,
   * the controller cannot determine the battery temperature.
   *
   * Apply the original firmware's fail-safe output states
   * and show a warning on the LCD.
   */

  if (isnan(effectiveTemp)) {

    // Display fault warning.
    lcd.setCursor(0, 0);
    lcd.print("BOTH SENS FAIL ");

    lcd.setCursor(0, 1);
    lcd.print("LOAD: OFF       ");

    // Activate fail-safe logic.
    applyFailSafe();

    // Skip remaining control operations for this iteration.
    return;
  }


  // ==========================================================
  // SECTION E: STAGE 1 - EARLY WARNING
  // ==========================================================

  /*
   * Warning threshold: 32 C
   *
   * When effective temperature reaches 32 C:
   *
   * - Activate buzzer warning.
   * - Toggle buzzer ON and OFF periodically.
   *
   * The warning stops when temperature drops below 32 C.
   *
   * Buzzer timing uses millis() rather than delay().
   */

  if (effectiveTemp >= 32.0) {

    if (now - lastBuzzMs >= BUZZ_PERIOD) {

      lastBuzzMs = now;

      // Toggle the buzzer phase.
      buzzPhase = !buzzPhase;
    }

    digitalWrite(
        BUZZER_PIN,
        buzzPhase ? HIGH : LOW
    );

  } else {

    // Temperature returned below warning threshold.
    buzzPhase = false;

    digitalWrite(BUZZER_PIN, LOW);
  }


  // ==========================================================
  // SECTION F: STAGE 2 - ACTIVE COOLING
  // ==========================================================

  /*
   * Cooling fan hysteresis:
   *
   * Fan ON  -> Temperature >= 35 C
   * Fan OFF -> Temperature <= 32 C
   *
   * Between 32 C and 35 C, the previous fan state is retained.
   *
   * This prevents repeated relay switching near the threshold.
   */

  if (effectiveTemp >= 35.0) {

    fanState = true;

  } else if (effectiveTemp <= 32.0) {

    fanState = false;
  }


  // Apply fan relay state.
  // Original firmware uses HIGH to enable this relay.
  digitalWrite(
      FAN_RELAY,
      fanState ? HIGH : LOW
  );


  // ==========================================================
  // SECTION G: STAGE 3 - EMERGENCY LOAD SHUTDOWN
  // ==========================================================

  /*
   * Motor load hysteresis:
   *
   * Load OFF -> Temperature >= 40 C
   * Load ON  -> Temperature <= 32 C
   *
   * Between 32 C and 40 C, the previous load state is retained.
   *
   * The motor automatically reconnects when temperature
   * decreases to 32 C or below.
   */

  if (effectiveTemp >= 40.0) {

    // Disconnect the motor load.
    loadState = false;

  } else if (effectiveTemp <= 32.0) {

    // Restore motor operation.
    loadState = true;
  }


  /*
   * The original firmware uses active-LOW relay logic
   * for the motor load:
   *
   * LOW  -> Load connected
   * HIGH -> Load disconnected
   */

  digitalWrite(
      LOAD_RELAY,
      loadState ? LOW : HIGH
  );


  // ==========================================================
  // SECTION H: REAL-TIME LCD STATUS DISPLAY
  // ==========================================================

  /*
   * LCD row 1:
   *
   * Effective temperature and sensor availability.
   *
   * D = DS18B20 operational
   * M = MLX90614 operational
   * * = Sensor unavailable
   *
   * Example:
   *
   * T:34.5C DM
   *
   * LCD row 2:
   *
   * Motor load status or sensor failure warning.
   */


  // ----------------------------------------------------------
  // LCD row 1: Temperature and sensor status.
  // ----------------------------------------------------------

  lcd.setCursor(0, 0);

  lcd.print("T:");

  // Display temperature with one decimal place.
  lcd.print(effectiveTemp, 1);

  lcd.print("C ");

  // Display DS18B20 availability.
  lcd.print(ds_ok ? "D" : "*");

  // Display MLX90614 availability.
  lcd.print(mlx_ok ? "M " : "* ");


  // ----------------------------------------------------------
  // LCD row 2: Motor load and sensor fault messages.
  // ----------------------------------------------------------

  lcd.setCursor(0, 1);


  if (!ds_ok && mlx_ok) {

    // DS18B20 failed, MLX90614 remains operational.

    lcd.print(
        loadState
        ? "ON DS:FAIL    "
        : "OFF DS:FAIL     "
    );

  } else if (ds_ok && !mlx_ok) {

    // MLX90614 failed, DS18B20 remains operational.

    lcd.print(
        loadState
        ? "ON IR:FAIL    "
        : "OFF IR:FAIL     "
    );

  } else {

    // Both sensors operational.

    lcd.print(
        loadState
        ? "LOAD: ON      "
        : "LOAD: OFF       "
    );
  }
}

// ============================================================
// END OF FIRMWARE
// ============================================================
