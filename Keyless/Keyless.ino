/////////////////////////////////////////////////////////////////////////////////
// Keyless StandAlone - system to open/close doors unsing a gearmotor
// Controls MICROMOTORS E192-2S.12.49 through power driver L6201 @ 12V.
// Runs on ATMega328P (Duemilanove, DUE, or similar boards)
//
// Code By Guido Scognamiglio - https://github.com/ZioGuido
// Last update: Sept 2019
//
// Notes on the gear-motor MICROMOTORS E192-2S.12.49:
// The encode reads 3 pulses every complete motor revolution.
// The gear ratio is 49,29:1 so every gearmotor revolution = 49,29 x 3 = 147,87 pulses.
//
// Notes on the L6201:
// When both inputs are set low and enable is still high, the motor stops immediately.
// When enable is set low when the motor is spinning, power is removed abruptedly and motor slows down by inertia.
//

// Libraries
#include <EEPROM.h>

// Pin definitions
#define PIN_J_RESET        6    // LOW for normal operation, HIGH for auto-set
#define PIN_J_ERROR_YES    8    // HIGH = disables motor operation in case of error
#define PIN_J_NO_RELEASE  13    // HIGH = no release lock, LOW = release lock
#define PIN_BUTTON         5    // Pushbutton to open door
#define PIN_SENSOR         2    // Reed switch so sense door close/open
#define PIN_DAYNIGHT       4    // LOW = lock door if close, HIGH = do nothing
#define PIN_MOTOR_ENABLE  10    // Controls L6201 operation
#define PIN_MOTOR_1       12    // Motor
#define PIN_MOTOR_2       11    // Motor
#define PIN_MOTOR_HALL     3    // Hall sensor from motor encoder, to track motor revolutions
#define PIN_LED_OK         7    // Blinks to indicates normal operation

// Other definitions
#define MOTOR_TIMEOUT     250   // Milliseconds to wait when an obstacle is found during motor movement before stopping the rotation
#define SECURITY_TIMEOUT  3000  // Milliseconds to wait until the port is open after pushing the button, otherwise close or lock again
#define CLOSE_WAIT        1500  // Milliseconds to wait before starting the close direction
#define LOCK_WAIT         1000  // Milliseconds to wait before locking the door
#define EEPROM_LOC_CONF   0     // EEPROM location where to start read/write the configuration structure
#define EEPROM_LOC_LAST   64    // Used to remember the last motor position after a power loss

// Global variables
int DayAndNight;                // Stores D&N switch status
int DoorStatus;                 // Stores door sensor value
int MotorIsMoving;              // Stores the motor status
volatile int MotorCounter;      // This is updated in an ISR
int PrevCounter;                // This is used to check that the motor is actually spinning
unsigned long ObstacleTimeout;  // A timeout to check if an obstacle is stopping the motor
unsigned long SecurityTimeout;  // To release or re-lock if the button is pushed but the door isn't open
int RequestedPulses;            // This variable is reassigned according to the amount of pulses to wait before stopping the motor
int CurrentMotorPosition;       // Tells the current motor position (see enum below)
int PreviousMotorPosition;      // Remembers the previous motor position (see enum below)
int LastOperation;              // Stores the last operation (see enum below)
int CalibrationStep;            // Used during the self-calibration

// Some enumerators
enum eOperations        { opNothing = 0, opOpen, opRelease, opLock, opUnlock };
enum eMPositions        { posLocked, posOpen, posReleased };
enum eDirections        { dirClose, dirOpen };
enum eCalibrationSteps  { calNothing = 0, calOpen, calClose, calFinish };

// Configuration variables
struct ConfStuct
{
  int len_open;
  int len_close;
  int len_release;
} configuration;

// Called to start motor rotation
void StartMotor()
{
  if (MotorIsMoving) return;

  int motor_direction;

  switch (LastOperation)
  {
  case opOpen:
    Serial.println("Operation: OPEN");
    motor_direction = dirOpen;
    //RequestedPulses = configuration.len_release;

    // Turn until the limit is reached, this will beat the spring's force and keep the door open.
    // The obstacle protection will properly stop the motor.
    RequestedPulses = 750;
    break;

  case opRelease:
    Serial.println("Operation: RELEASE");
    motor_direction = dirClose;
    RequestedPulses = configuration.len_release;
    break;

  case opLock:
    // Don't lock door if it's open
    delay(LOCK_WAIT); if (DoorStatus) return;
    
    Serial.println("Operation: LOCK");
    motor_direction = dirClose;
    RequestedPulses = configuration.len_close;
    if (digitalRead(PIN_J_NO_RELEASE) == LOW) 
      RequestedPulses += configuration.len_release;
    break;

  case opUnlock:
    Serial.println("Operation: UNLOCK");
    motor_direction = dirOpen;
    RequestedPulses = configuration.len_close;
    if (digitalRead(PIN_J_NO_RELEASE) == LOW) 
      RequestedPulses += configuration.len_release;
    break;
  }

  MotorIsMoving = 1;
  PrevCounter = -1;
  MotorCounter = 0;
  ObstacleTimeout = millis();

  digitalWrite(PIN_MOTOR_1,  motor_direction);
  digitalWrite(PIN_MOTOR_2, !motor_direction);
  if (motor_direction == dirClose) delay(CLOSE_WAIT);
  digitalWrite(PIN_MOTOR_ENABLE, 1);

  Serial.println("START MOTOR");
  Serial.print("motor_direction : "); Serial.println(motor_direction);
  Serial.println();
}

// Called to stop motor rotation when the target has been reached or if an obstacle is encountered
void StopMotor()
{
  digitalWrite(PIN_MOTOR_1, 0);
  digitalWrite(PIN_MOTOR_2, 0);

  MotorIsMoving = 0;

  // Disable motor driver after 100 milliseconds
  delay(100);
  digitalWrite(PIN_MOTOR_ENABLE, 0);

  PreviousMotorPosition = CurrentMotorPosition;

  switch(LastOperation)
  {
  case opUnlock:
  case opOpen:
    CurrentMotorPosition = posOpen;
    SecurityTimeout = millis();
    break;

  case opRelease:
    CurrentMotorPosition = posReleased;
    break;

  case opLock:
    CurrentMotorPosition = posLocked;
    break;
  }

  // Store last motor position in memory
  EEPROM.write(EEPROM_LOC_LAST, CurrentMotorPosition);

  Serial.println("STOP MOTOR");
  Serial.print("LastOperation : "); Serial.println(LastOperation);
  Serial.print("CurrentMotorPosition : "); Serial.println(CurrentMotorPosition);
  Serial.println();

  LastOperation = opNothing;
}

// Called to stop motor if an obstacle has been encountered.
// Can either happen accidentally during normal operation, or intentionally during the self-calibration phase.
void StopByObstacle()
{
  // First thing, stop the motor!
  StopMotor();

  switch (CalibrationStep)
  {
  case calOpen:
    if (digitalRead(PIN_J_NO_RELEASE) == LOW)
      configuration.len_release = MotorCounter;
    else
      configuration.len_open = MotorCounter;

    CalibrationStep = calClose;
    DoCalibration();
    return;

  case calClose:
    configuration.len_close = MotorCounter;
    CalibrationStep = calFinish;
    DoCalibration();
    return;
  }

  // In normal operation, open door if an obstacle has been found and this jumper is removed
  if (digitalRead(PIN_J_ERROR_YES) == HIGH)
    OpenDoor();
}

// Called when the open button is pushed
void OpenDoor()
{
  // Debounce
  delay(50); if (digitalRead(PIN_BUTTON)) return;

  // What to do with the open button?
  switch (CurrentMotorPosition)
  {
  case posReleased: LastOperation = opOpen; break;
  case posLocked: LastOperation = opUnlock; break;
  }

  StartMotor();
}

// ISR to increment the motor encoder pulse counter
void GetEncoder()
{
  MotorCounter++;
}

void DoCalibration()
{
  int motor_direction;

  switch (CalibrationStep)
  {
  case calOpen:
    motor_direction = dirOpen;
    Serial.println("CALIBRATION PHASE 1: OPEN");
    break;

  case calClose:
    motor_direction = dirClose;
    Serial.println("CALIBRATION PHASE 2: CLOSE");
    break;

  case calFinish:
    CalibrationStep = calNothing;
    EEPROM.put(EEPROM_LOC_CONF, configuration);
    Serial.println("CALIBRATION TERMINATED");
    return;
  }

  if (CalibrationStep > 0)
  {
    MotorIsMoving = 1;
    PrevCounter = -1;
    MotorCounter = 0;
    RequestedPulses = 1000;
    ObstacleTimeout = millis();

    digitalWrite(PIN_MOTOR_1,  motor_direction);
    digitalWrite(PIN_MOTOR_2, !motor_direction);
    if (motor_direction == dirClose) delay(CLOSE_WAIT);
    digitalWrite(PIN_MOTOR_ENABLE, 1);
  }
}

void setup()
{
  Serial.begin(115200);

  // Get configuration from EEPROM
  EEPROM.get(EEPROM_LOC_CONF, configuration);

  // Motor
  pinMode(PIN_MOTOR_ENABLE, OUTPUT);
  pinMode(PIN_MOTOR_1, OUTPUT);
  pinMode(PIN_MOTOR_2, OUTPUT);
  pinMode(PIN_MOTOR_HALL, INPUT_PULLUP);

  // LED
  pinMode(PIN_LED_OK, OUTPUT);

  // Jumpers
  pinMode(PIN_J_RESET, INPUT_PULLUP);
  pinMode(PIN_J_ERROR_YES, INPUT_PULLUP);
  pinMode(PIN_J_NO_RELEASE, INPUT_PULLUP);

  // Terminals
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_SENSOR, INPUT_PULLUP);
  pinMode(PIN_DAYNIGHT, INPUT_PULLUP);

  // Initialize
  StopMotor();
  attachInterrupt(digitalPinToInterrupt(PIN_MOTOR_HALL), GetEncoder, FALLING);
  CurrentMotorPosition = EEPROM.read(EEPROM_LOC_LAST);
  PreviousMotorPosition = CurrentMotorPosition;

  // If the Jumper RESET is removed at boot, do the self-calibration
  CalibrationStep = calNothing;
  if (digitalRead(PIN_J_RESET) == HIGH)
  {
    CalibrationStep = calOpen;
    DoCalibration();
  }
}


void loop()
{
  // Read inputs
  DoorStatus        = digitalRead(PIN_SENSOR);      // LOW = door is closed
  DayAndNight       = digitalRead(PIN_DAYNIGHT);    // HIGH = keep the door locked

  if (MotorIsMoving)
  {
    // Check that the motor is spinning
    if (PrevCounter != MotorCounter)
    {
      PrevCounter = MotorCounter;
      ObstacleTimeout = millis();
      if (MotorCounter >= RequestedPulses) StopMotor();
      //Serial.println(MotorCounter);
    }
    // If no data is coming from the encoder, an obstacle is halting the motor.
    else
    {
      if (millis() - ObstacleTimeout > MOTOR_TIMEOUT)
        StopByObstacle();
    }
  }

  // Things to do when the motor is not spinning
  else
  {
    if (digitalRead(PIN_BUTTON) == LOW)
    {
      OpenDoor();
    }

    // Lock if motor is in released position, D&N is on, door is close
    if (CurrentMotorPosition == posReleased && DayAndNight == HIGH && DoorStatus == LOW)
    {
      LastOperation = opLock;
      StartMotor();
    }

    // Things to do when the motor is in open position
    if (CurrentMotorPosition == posOpen)
    {
      // Release after opening when the door is actually open
      if (DoorStatus == HIGH)
      {
        LastOperation = opRelease;
        StartMotor();
      }
      // If the door is still close
      else
      {
        // Close or lock door if the button has been pushed but the door hasn't been open within the specified TIMEOUT
        if (millis() - SecurityTimeout > SECURITY_TIMEOUT)
        {
          LastOperation = DayAndNight ? opLock : opRelease;
          StartMotor();
        }
      }
    }
  }

  /* Don't blink...
  digitalWrite(PIN_LED_OK, HIGH);
  delay(50);
  digitalWrite(PIN_LED_OK, LOW);
  delay(950);
  */
}
