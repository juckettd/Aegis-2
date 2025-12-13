#include <XInput.h>
#include <Toggle.h>

uint8_t smallRumbleCounter = 0;
uint8_t bigRumbleCounter = 0;

// Define button pins
uint8_t inputButtonCount = 16;
uint8_t inputButtonPins[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 16, 17, 18, 19};
uint8_t outputButtonCount = 2;
uint8_t outputButtonPins[2] = {22, 23};

int deadZone = 125;
int serialCount = 0;

const int Pin_DpadUp    = 5;
const int Pin_DpadDown  = 3;
const int Pin_DpadLeft  = 2;
const int Pin_DpadRight = 6;

const int Pin_ButtonA = 11;
const int Pin_ButtonB = 10;
const int Pin_ButtonX = 17;
const int Pin_ButtonY = 16;

const int Pin_ButtonLB = 8;
const int Pin_ButtonRB = 18;

const int Pin_ButtonL3 = 4;
const int Pin_ButtonR3 = 12;

const int Pin_TriggerL = 7;
const int Pin_TriggerR = 19;

const int Pin_ButtonStart = 9;
const int Pin_ButtonSelect = 1;

// Setup the debouncing
Toggle ButtonUpPad(Pin_DpadUp);
Toggle ButtonDownPad(Pin_DpadDown);
Toggle ButtonLeftPad(Pin_DpadLeft);
Toggle ButtonRightPad(Pin_DpadRight);

Toggle ButtonA(Pin_ButtonA);
Toggle ButtonB(Pin_ButtonB);
Toggle ButtonX(Pin_ButtonX);
Toggle ButtonY(Pin_ButtonY);
Toggle ButtonLB(Pin_ButtonLB);
Toggle ButtonRB(Pin_ButtonRB);
Toggle ButtonL3(Pin_ButtonL3);
Toggle ButtonR3(Pin_ButtonR3);
Toggle ButtonLT(Pin_TriggerL);
Toggle ButtonRT(Pin_TriggerR);
Toggle ButtonStart(Pin_ButtonStart);
Toggle ButtonSelect(Pin_ButtonSelect);

void setup() {  
  Serial1.setRX(25);
  Serial1.setTX(24);
  Serial1.begin(38400);

  ButtonUpPad.begin(Pin_DpadUp);
  ButtonDownPad.begin(Pin_DpadDown);
  ButtonLeftPad.begin(Pin_DpadLeft);
  ButtonRightPad.begin(Pin_DpadRight);
  ButtonA.begin(Pin_ButtonA);
  ButtonB.begin(Pin_ButtonB);
  ButtonX.begin(Pin_ButtonX);
  ButtonY.begin(Pin_ButtonY);
  ButtonLB.begin(Pin_ButtonLB);
  ButtonRB.begin(Pin_ButtonRB);
  ButtonL3.begin(Pin_ButtonL3);
  ButtonR3.begin(Pin_ButtonR3);
  ButtonLT.begin(Pin_TriggerL);
  ButtonRT.begin(Pin_TriggerR);
  ButtonStart.begin(Pin_ButtonStart);
  ButtonSelect.begin(Pin_ButtonSelect);
/*
  for(int i = 0; i < inputButtonCount; i++) {
    pinMode(inputButtonPins[i], INPUT_PULLUP);
  }
*/
  for(int i = 0; i < outputButtonCount; i++) {
    pinMode(outputButtonPins[i], OUTPUT);
    analogWriteFrequency(outputButtonPins[i], 187500);
  }

  XInput.setTriggerRange(0, 1);
  XInput.setJoystickRange(200, 800);
  XInput.setAutoSend(false);

  XInput.begin();
}


void loop() {
  ButtonUpPad.poll();
  ButtonDownPad.poll();
  ButtonLeftPad.poll();
  ButtonRightPad.poll();
  ButtonA.poll();
  ButtonB.poll();
  ButtonX.poll();
  ButtonY.poll();
  ButtonLB.poll();
  ButtonRB.poll();
  ButtonL3.poll();
  ButtonR3.poll();
  ButtonLT.poll();
  ButtonRT.poll();
  ButtonStart.poll();
  ButtonSelect.poll();
  
  boolean upPad = ButtonUpPad.isPressed();
  boolean downPad = ButtonDownPad.isPressed();
  boolean leftPad = ButtonLeftPad.isPressed();
  boolean rightPad = ButtonRightPad.isPressed();

  boolean buttonA = ButtonA.isPressed();
  boolean buttonB = ButtonB.isPressed();
  boolean buttonX = ButtonX.isPressed();
  boolean buttonY = ButtonY.isPressed();
  boolean buttonLB = ButtonLB.isPressed();
  boolean buttonRB = ButtonRB.isPressed();
  boolean buttonL3 = ButtonL3.isPressed();
  boolean buttonR3 = ButtonR3.isPressed();
  boolean buttonStart = ButtonStart.isPressed();
  boolean buttonSelect = ButtonSelect.isPressed();

  int buttonLT = ButtonLT.isPressed();
  int buttonRT = ButtonRT.isPressed();

  int leftStickX = analogRead(1);
  int leftStickY = analogRead(0);

  int rightStickX = 1000 - analogRead(7) + 125;
  int rightStickY = 1000 - analogRead(6);

  uint8_t bigRumble = XInput.getRumbleLeft();
  uint8_t smallRumble = XInput.getRumbleRight();
  uint8_t bigRumbleSet = 0;
  uint8_t smallRumbleSet = 0;

  if ((leftStickX > (500 - deadZone)) && (leftStickX < (500 + deadZone)))
    leftStickX = 500;
  if ((leftStickY > (500 - deadZone)) && (leftStickY < (500 + deadZone)))
    leftStickY = 500;

  if ((rightStickX > (500 - deadZone)) && (rightStickX < (500 + deadZone)))
    rightStickX = 500;
  if ((rightStickY > (500 - deadZone)) && (rightStickY < (500 + deadZone)))
    rightStickY = 500;

  XInput.setDpad(upPad, downPad, leftPad, rightPad);
  XInput.setJoystickY(JOY_LEFT, leftStickY, true);
  XInput.setJoystickX(JOY_LEFT, leftStickX);
  XInput.setJoystickY(JOY_RIGHT, rightStickY, true);
  XInput.setJoystickX(JOY_RIGHT, rightStickX);

  XInput.setTrigger(TRIGGER_LEFT, buttonLT);
  XInput.setTrigger(TRIGGER_RIGHT, buttonRT);
    
  XInput.setButton(BUTTON_A, buttonA);
  XInput.setButton(BUTTON_B, buttonB);
  XInput.setButton(BUTTON_X, buttonX);
  XInput.setButton(BUTTON_Y, buttonY);
  XInput.setButton(BUTTON_LB, buttonLB);
  XInput.setButton(BUTTON_RB, buttonRB);
  XInput.setButton(BUTTON_L3, buttonL3);
  XInput.setButton(BUTTON_R3, buttonR3);
  XInput.setButton(BUTTON_START, buttonStart);
  XInput.setButton(BUTTON_BACK, buttonSelect);

  // Lower the rumble values for the motors in the handheld
  if (bigRumble > 25)
  {
    bigRumbleCounter = 0;
    bigRumbleSet = bigRumble / 8 + 45;
  }
  else
  {
    if (bigRumbleCounter > 10)
      bigRumbleSet = 0;
    else
      bigRumbleCounter = bigRumbleCounter + 1;
  }

  if (smallRumble > 25)
  {
    smallRumbleCounter = 0;
    smallRumbleSet = smallRumble / 8 + 45;
  }
  else
  {
    if (smallRumbleCounter > 10)
      smallRumbleSet = 0;
    else
      smallRumbleCounter = smallRumbleCounter + 1;
  }

  analogWrite(23, bigRumbleSet);
  analogWrite(22, smallRumbleSet);

  /*
  serialCount = serialCount + 1;
  if (serialCount > 20)
  {
    serialCount = 0;
    Serial1.print("Left_X: ");
    Serial1.print(leftStickX);
    Serial1.print(", ");
    Serial1.print("Left_Y: ");
    Serial1.print(leftStickY);
    Serial1.print(", ");
    Serial1.print("Right_X: ");
    Serial1.print(rightStickX);  
    Serial1.print(", ");
    Serial1.print("Right_Y: ");
    Serial1.print(rightStickY);
    Serial1.println();
  }
  */
  
  //delay(50);
  XInput.send();
}
