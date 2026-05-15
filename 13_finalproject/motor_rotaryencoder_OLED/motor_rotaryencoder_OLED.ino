#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ezButton.h>

// OLED DISPLAY
// Define OLED screen size and initialize display object using I2C communication
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ROTARY ENCODER
// Initialize pins: These pins read rotation (CLK + DT) and button press (SW)
#define CLK_PIN D2
#define DT_PIN  D1
#define SW_PIN  D0

bool chosen_hook = false;

// Creates a debounced button object for the encoder switch
ezButton button(SW_PIN);

// STEPPER MOTOR
// Initialize STEP & DIR pin
const int stepPin = D3;
const int dirPin  = D6;

// Number of motor steps required to move between each hook
int stepDist = 200;

// HOOK POSITION STATES
int hookIndex = 1; // what the user is scrolling through in the menu
int selectedHook = 0; // confirmed selection after button press
int targetPos = 0; // where the motor should move to
int currentPos = 0; // where the motor currently is

// MENU OFFSET (scroll window)
int menuOffset = 0;

// Track rotary encoder movement
int CLK_state;
int prev_CLK_state;

unsigned long lastStepTime = 0; // // Stores the last time a motor step was executed
const unsigned long stepInterval = 8; // Time interval between each motor step pulse replacing delay(8)

int stepsRemaining = 0; // Tracks how many individual step pulses are still needed to reach the target position
bool motorRunning = false; // Indicates whether the motor is currently in motion
int motorDir = LOW; // Keeps track of motor direction (forward or backward)
bool continuousSpin = true;

void setup() {
 Serial.begin(9600);

 // Initialize OLED display; stop program if initialization fails
 if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
   Serial.println("OLED failed");
   while (1);

  pinMode(stepPin, OUTPUT);
  pinMode(dirPin, OUTPUT);

  digitalWrite(dirPin, LOW);
  lastStepTime = millis();
  continuousSpin = true; // start spinning immediately
}

 oled.clearDisplay();

 // Define rotary encoder input pins
 pinMode(CLK_PIN, INPUT);
 pinMode(DT_PIN, INPUT);

 // Waits 50 ms after a button press to make sure the signal is stable
 button.setDebounceTime(50);
 
 // Store initial encoder state
 prev_CLK_state = digitalRead(CLK_PIN);

 // Define motor control output pins
 pinMode(stepPin, OUTPUT);
 pinMode(dirPin, OUTPUT);
 
 // Draw initial menu on OLED screen
 drawUI();
 runMotor();
}

void loop() {
 button.loop(); // Update button state
 readEncoder(); // Read rotary encoder movement (scroll menu)
 readButton(); // Check if encoder button was pressed (selection)

  if (continuousSpin && !motorRunning && !chosen_hook) {
    runMotor();
  }
 moveMotor(); // Move motor if a new target position is selected

}

// ROTARY SCROLLING: detects rotation direction and updates menu selection
void readEncoder() {  
 CLK_state = digitalRead(CLK_PIN);

 // Detect one step of knob turning
 if (CLK_state != prev_CLK_state && CLK_state == HIGH) {
  
   // Determine rotation direction using DT pin
   if (digitalRead(DT_PIN) == HIGH) {
     hookIndex--; // rotate one direction
   } else {
     hookIndex++; // rotate opposite direction
   }

   // Cycles between 1 and 4
   if (hookIndex > 8) hookIndex = 1;
   if (hookIndex < 1) hookIndex = 8;
   
   // Update OLED display with new selection
   drawUI();
 }
 // Save current state for next comparison
 prev_CLK_state = CLK_state;
}

// BUTTON PRESS (CONFIRM SELECTION): When encoder button is pressed, lock in selection and trigger motor move
void readButton() {
 if (button.isPressed()) {
  if (chosen_hook == false) {
    chosen_hook = true;

    selectedHook = hookIndex;
    targetPos = selectedHook*stepDist;   // replaces 4 old buttons

    Serial.print("Moving to Hook: ");
    Serial.println(targetPos);

    // Confirmation screen on OLED
    showSelected();

    // START MOTOR MOVE (replaces old blocking loop trigger)
    startMotorMove();
  }
  else {
    chosen_hook = false;

  }
 }
}

void startMotorMove() {
// Initializes motor movement from current position to target position by
// setting up the state for moveMotor()

  if (currentPos == targetPos) return;

  motorRunning = true; // tells system the motor is running
  stepsRemaining = abs(targetPos - currentPos);  // Convert position difference into number of physical step pulses needed

  if (targetPos > currentPos) {
    motorDir = LOW;
    digitalWrite(dirPin, LOW);
  } else {
    motorDir = HIGH;
    digitalWrite(dirPin, HIGH);

    Serial.println(currentPos);
    Serial.println(targetPos);
  }

  // Initialize timing reference
  lastStepTime = millis();
}

// OLED MENU DISPLAY: Displays scrollable list of hooks with arrow indicator
void drawUI() {
 oled.clearDisplay();

 oled.setTextSize(1);
 oled.setTextColor(WHITE);

 oled.setCursor(0, 0);
 oled.println("Select Hook");

 // Display 4 hooks on the screen each time you scroll
 for (int i = 0; i < 4; i++) {

   int hookNum = menuOffset + i + 1;
   if (hookNum > 8) break;

   oled.setCursor(0, 12 * (i + 1));

   if (hookNum == hookIndex) oled.print("> ");
   else oled.print("  ");

   oled.print("Hook ");
   oled.println(hookNum);
 }

 oled.display();
}

// CONFIRMATION SCREEN: Displays selected hook and "moving" message
void showSelected() {
 oled.clearDisplay();

 oled.setTextSize(2);
 oled.setCursor(10, 20);
 oled.print("Hook ");
 oled.println(selectedHook);

 oled.setTextSize(1);
 oled.setCursor(10, 45);
 oled.println("Moving...");

 oled.display();
}

// STEPPER MOTOR CONTROL: Moves motor from current position to target position
void moveMotor() {

 if (!motorRunning) return; // exit immediately if motor is not supposed to be moving
  Serial.println(stepsRemaining);
  unsigned long currentMillis = millis();  // get the current system time

  if (currentMillis - lastStepTime >= stepInterval && stepsRemaining > 0) { //  Checks if enough time has passed since last step AND if steps remain

    lastStepTime = currentMillis; // update last step time to current time

     // Generate one step pulse
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(2);
    digitalWrite(stepPin, LOW);

    stepsRemaining--; // decrease remaining step count

    if (stepsRemaining <= 0) { // stop motor after all steps are complete
      motorRunning = false;
      currentPos = targetPos;

    }
  }
}

void runMotor() {
  static unsigned long lastStep = 0;
  digitalWrite(dirPin, LOW); // keep direction fixed

  if (millis() - lastStep >= stepInterval) {   // timing control instead of delay(8)
    lastStep = millis();

    // step pulse
    digitalWrite(stepPin, HIGH);
    // delayMicroseconds(8);
    digitalWrite(stepPin, LOW);

    currentPos+=1;
    if (currentPos >= 3200){
      currentPos = 0;
    }
  }
}