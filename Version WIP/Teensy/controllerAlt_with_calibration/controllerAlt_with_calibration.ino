#include <XInput.h>
#include <EEPROM.h>

// === Calibration addresses ===
#define CALIB_MAGIC  0x4A4F5943ul /* 'JOY C' */
#define CALIB_VER    0x0001
#define CALIB_EEADDR 0  // start at 0; blob is small

// === Helper additions for conditional sending and polling ===
const uint32_t REPORT_INTERVAL_MS = 5;   // send at most every ~5ms if no changes

// Suggested timings:
// - 5–10 ms for press confirm kills phantom spikes
// - 20–40 ms for release confirm masks brief dropouts while held
static constexpr uint16_t PRESS_CONFIRM_MS   = 20;
static constexpr uint16_t RELEASE_CONFIRM_MS = 30;

// === Robust Button presses to avoid the extra noise on the lines ===
struct RobustButton 
{
  uint8_t pin;
  bool activeLow;               // true for INPUT_PULLUP wiring
  bool latched = false;
  bool lastRaw = false;
  elapsedMillis stableT = 0;

  void begin(uint8_t p, bool active_low, uint8_t mode = INPUT_PULLUP) 
  {
    pin = p; activeLow = active_low;
    pinMode(pin, mode);
    lastRaw = raw();
    stableT = 0;
  }

  inline bool raw() const 
  {
    int v = digitalRead(pin);
    return activeLow ? (v == LOW) : (v == HIGH);
  }

  // Call each loop
  bool update() 
  {
    bool r = raw();
    if (r != lastRaw) // input flipped -> reset timer
    {            
      lastRaw = r;
      stableT = 0;
    }
    if (r && !latched && stableT >= PRESS_CONFIRM_MS) 
      latched = true;
    else if (!r && latched && stableT >= RELEASE_CONFIRM_MS) 
      latched = false;
    return latched;
  }

  bool isPressed() const { return latched; }
};

// === Joystick Auto-Calibration Helpers ===
struct AxisCal 
{
  int min = 500;
  int max = 500;
  int floorVal = 200;
  int ceilVal  = 800;
  int center = 500;
  int deadzone = 75; // output units (around 500 center)
  bool flipped = false;

  void reset() 
  { 
    min = 500; 
    max = 500;
    floorVal = 200;
    ceilVal  = 800;
    center = 500;
    deadzone = 75; // output units (around 500 center)
    flipped = false;
  }

  void sample(int raw) 
  { 
    if (raw < min) 
      min = raw; 
    if (raw > max) 
      max = raw; 
  }

  void flip()
  {
    flipped = !flipped;
  }

  void deadzoneAdd(int increment)
  {
    deadzone += increment;
    if (deadzone < 0)
      deadzone = 0;
    else if (deadzone > 250)
      deadzone = 250;
  }
  
  void finalize() 
  {
    int span = max - min;
    int cut = (span > 0) ? (span / 10) : 0; // 10%
    floorVal = min + cut;
    ceilVal  = max - cut;
    if (ceilVal <= floorVal) 
    { 
      floorVal = min; 
      ceilVal = max; 
    }
    center = (floorVal + ceilVal) / 2;
  }

  int mapToOut(int raw, int outMin = 0, int outMax = 1000) const 
  {
    int outCenter = (outMin + outMax) / 2;
    if ((raw > (center - deadzone)) && (raw < (center + deadzone)))
      return outCenter;

    long naive = 0;
    if (raw > center)
      naive = map(raw, center + deadzone, ceilVal, outCenter, outMax);
    else if (raw < center)
      naive = map(raw, floorVal, center - deadzone, outMin, outCenter);

    if (flipped)
      return outMax - constrain((int)naive, outMin, outMax);
    else
      return constrain((int)naive, outMin, outMax);
  }
};

struct JoystickCal 
{
  AxisCal lx, ly, rx, ry;
  bool active = false;
  elapsedMillis timer;
  uint32_t minDurationMs = 1000;

  void start() 
  { 
    active = true; 
    timer = 0; 
    lx.reset(); 
    ly.reset(); 
    rx.reset(); 
    ry.reset(); 
  }
  
  void sample(int rawLX, int rawLY, int rawRX, int rawRY) 
  {
    if (!active) 
      return;

    lx.sample(rawLX); 
    ly.sample(rawLY); 
    rx.sample(rawRX); 
    ry.sample(rawRY);
  }

  void deadzoneAddLeft(int lxd, int lyd)
  {
    lx.deadzoneAdd(lxd);
    ly.deadzoneAdd(lyd);
    timer = 0;
  }

  void deadzoneAddRight(int rxd, int ryd)
  {
    rx.deadzoneAdd(rxd);
    ry.deadzoneAdd(ryd);
    timer = 0;
  }

  void flipLeft()
  {
    lx.flip();
    ly.flip();
    timer = 0;
  }

  void flipRight()
  {
    rx.flip();
    ry.flip();
    timer = 0;
  }

  bool canAlter() const 
  { 
    return timer >= minDurationMs; 
  }
  
  void finish() 
  { 
    if (!active) 
      return; 
    
    lx.finalize(); 
    ly.finalize(); 
    rx.finalize(); 
    ry.finalize(); 
    active = false; 
    timer = 0;
  }

  void resetTimer()
  {
    timer = 0;
  }
};
JoystickCal JoyCal;

// === Calibration Persistence (EEPROM) ===
// We store a compact blob with a magic, version, data, and checksum.
// This lets calibration survive power cycles on Teensy LC.
struct AxisPersist 
{
  int16_t min;
  int16_t max;
  int16_t floorVal;
  int16_t ceilVal;
  int16_t center;
  int16_t deadzone;
  int8_t flipped;
};

struct CalibBlob 
{
  uint32_t magic;
  uint16_t version;
  AxisPersist ax[4];  // 0: LX, 1: LY, 2: RX, 3: RY
  uint32_t checksum;  // simple 32-bit sum over bytes (excluding this field)
};

static uint32_t sum32(const uint8_t* p, size_t n) 
{
  uint32_t s = 0;
  for (size_t i=0;i<n;i++) 
    s += p[i];
  
  return s;
}

void axisToPersist(const AxisCal& a, AxisPersist& out) 
{
  out.min        = a.min;
  out.max        = a.max;
  out.floorVal   = a.floorVal;
  out.ceilVal    = a.ceilVal;
  out.center     = a.center;
  out.deadzone   = a.deadzone;
  out.flipped    = a.flipped ? 1 : 0;
}

void persistToAxis(const AxisPersist& in, AxisCal& a) 
{
  a.min         = in.min;
  a.max         = in.max;
  a.floorVal    = in.floorVal;
  a.ceilVal     = in.ceilVal;
  a.center      = in.center;
  a.deadzone    = in.deadzone;
  a.flipped     = (in.flipped != 0);
}

bool saveCalibration() 
{
  CalibBlob blob;
  blob.magic = CALIB_MAGIC;
  blob.version = CALIB_VER;
  axisToPersist(JoyCal.lx, blob.ax[0]);
  axisToPersist(JoyCal.ly, blob.ax[1]);
  axisToPersist(JoyCal.rx, blob.ax[2]);
  axisToPersist(JoyCal.ry, blob.ax[3]);
  // compute checksum over everything except the checksum field
  blob.checksum = 0;
  uint32_t s = sum32((const uint8_t*)&blob, sizeof(CalibBlob)-sizeof(uint32_t));
  blob.checksum = s;

  // Bounds sanity before writing
  auto okAxis = [](const AxisPersist& a)->bool 
  {
    if (a.min < 0 || a.min > 1023) 
      return false;
    if (a.max < 0 || a.max > 1023) 
      return false;
    if (a.floorVal < 0 || a.floorVal > 1023) 
      return false;
    if (a.ceilVal  < 0 || a.ceilVal  > 1023) 
      return false;
    if (a.center < 0 || a.center > 1023) 
      return false;
    if (a.min > a.max) 
      return false;
    return true;
  };

  if (!(okAxis(blob.ax[0]) && okAxis(blob.ax[1]) && okAxis(blob.ax[2]) && okAxis(blob.ax[3]))) {
    return false;
  }

  // Write
  int addr = CALIB_EEADDR;
  const uint8_t* p = (const uint8_t*) & blob;
  for (size_t i = 0; i < sizeof(CalibBlob); ++i) 
  {
    EEPROM.update(addr + i, p[i]);
  }
  return true;
}

bool loadCalibration() 
{
  CalibBlob blob;
  // Read
  int addr = CALIB_EEADDR;
  uint8_t* p = (uint8_t*)&blob;

  for (size_t i=0;i<sizeof(CalibBlob);++i) 
  {
    p[i] = EEPROM.read(addr+i);
  }
  
  if (blob.magic != CALIB_MAGIC || blob.version != CALIB_VER) 
    return false;
  
  uint32_t s = sum32((const uint8_t*)&blob, sizeof(CalibBlob)-sizeof(uint32_t));
  if (s != blob.checksum) 
    return false;

  // Basic bounds / monotonic checks
  auto okAxis = [](const AxisPersist& a)->bool 
  {
    if (a.min < 0 || a.min > 1023) 
      return false;
    if (a.max < 0 || a.max > 1023) 
      return false;
    if (a.floorVal < 0 || a.floorVal > 1023) 
      return false;
    if (a.ceilVal  < 0 || a.ceilVal  > 1023) 
      return false;
    if (a.center < 0 || a.center > 1023) 
      return false;
    if (a.min > a.max) 
      return false;
    return true;
  };

  if (!(okAxis(blob.ax[0]) && okAxis(blob.ax[1]) && okAxis(blob.ax[2]) && okAxis(blob.ax[3]))) 
  {
    return false;
  }

  // Load into runtime
  persistToAxis(blob.ax[0], JoyCal.lx);
  persistToAxis(blob.ax[1], JoyCal.ly);
  persistToAxis(blob.ax[2], JoyCal.rx);
  persistToAxis(blob.ax[3], JoyCal.ry);

  return true;
}

void clearCalibration() 
{
  // Clear magic; minimal write
  EEPROM.update(CALIB_EEADDR + 0, 0x00);
  EEPROM.update(CALIB_EEADDR + 1, 0x00);
  EEPROM.update(CALIB_EEADDR + 2, 0x00);
  EEPROM.update(CALIB_EEADDR + 3, 0x00);
}

uint32_t computeReportHash(
  bool upPad, bool downPad, bool leftPad, bool rightPad,
  bool buttonA, bool buttonB, bool buttonX, bool buttonY,
  bool buttonLB, bool buttonRB, bool buttonL3, bool buttonR3,
  bool buttonStart, bool buttonSelect,
  int leftStickX, int leftStickY, int rightStickX, int rightStickY,
  int buttonLT, int buttonRT) 
{
  uint32_t h = 0;
  // pack buttons
  uint16_t b = 0;
  b |= (upPad       ? 1u : 0u) << 0;
  b |= (downPad     ? 1u : 0u) << 1;
  b |= (leftPad     ? 1u : 0u) << 2;
  b |= (rightPad    ? 1u : 0u) << 3;
  b |= (buttonA     ? 1u : 0u) << 4;
  b |= (buttonB     ? 1u : 0u) << 5;
  b |= (buttonX     ? 1u : 0u) << 6;
  b |= (buttonY     ? 1u : 0u) << 7;
  b |= (buttonLB    ? 1u : 0u) << 8;
  b |= (buttonRB    ? 1u : 0u) << 9;
  b |= (buttonL3    ? 1u : 0u) << 10;
  b |= (buttonR3    ? 1u : 0u) << 11;
  b |= (buttonStart ? 1u : 0u) << 12;
  b |= (buttonSelect? 1u : 0u) << 13;
  h ^= b;

  // fold in axes and triggers (10-bit typical)
  h ^= (uint32_t)(leftStickX & 0x3FF)  << 16;
  h ^= (uint32_t)(leftStickY & 0x3FF)  << 6;
  h ^= (uint32_t)(rightStickX & 0x3FF) << 22;
  h ^= (uint32_t)(rightStickY & 0x3FF) << 12;
  h ^= (uint32_t)((buttonLT & 0x3FF) ^ ((buttonRT & 0x3FF) << 10));
  return h;
}

uint8_t smallRumbleCounter = 0;
uint8_t bigRumbleCounter = 0;

// Define button pins
uint8_t outputButtonCount = 2;
uint8_t outputButtonPins[2] = {22, 23};

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
RobustButton ButtonUpPad;
RobustButton ButtonDownPad;
RobustButton ButtonLeftPad;
RobustButton ButtonRightPad;

RobustButton ButtonA;
RobustButton ButtonB;
RobustButton ButtonX;
RobustButton ButtonY;

RobustButton ButtonLB;
RobustButton ButtonRB;

RobustButton ButtonL3;
RobustButton ButtonR3;

RobustButton ButtonLT;
RobustButton ButtonRT;

RobustButton ButtonStart;
RobustButton ButtonSelect;

void rumble()
{
  uint8_t bigRumble = XInput.getRumbleLeft();
  uint8_t smallRumble = XInput.getRumbleRight();
  uint8_t bigRumbleSet = 0;
  uint8_t smallRumbleSet = 0;

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
}

void setup() 
{  
  //Serial1.setRX(25);
  //Serial1.setTX(24);
  //Serial1.begin(38400);

  loadCalibration();

  ButtonUpPad.begin(Pin_DpadUp, true, INPUT_PULLUP);
  ButtonDownPad.begin(Pin_DpadDown, true, INPUT_PULLUP);
  ButtonLeftPad.begin(Pin_DpadLeft, true, INPUT_PULLUP);
  ButtonRightPad.begin(Pin_DpadRight, true, INPUT_PULLUP);
  ButtonA.begin(Pin_ButtonA, true, INPUT_PULLUP);
  ButtonB.begin(Pin_ButtonB, true, INPUT_PULLUP);
  ButtonX.begin(Pin_ButtonX, true, INPUT_PULLUP);
  ButtonY.begin(Pin_ButtonY, true, INPUT_PULLUP);
  ButtonLB.begin(Pin_ButtonLB, true, INPUT_PULLUP);
  ButtonRB.begin(Pin_ButtonRB, true, INPUT_PULLUP);
  ButtonL3.begin(Pin_ButtonL3, true, INPUT_PULLUP);
  ButtonR3.begin(Pin_ButtonR3, true, INPUT_PULLUP);
  ButtonLT.begin(Pin_TriggerL, true, INPUT_PULLUP);
  ButtonRT.begin(Pin_TriggerR, true, INPUT_PULLUP);
  ButtonStart.begin(Pin_ButtonStart, true, INPUT_PULLUP);
  ButtonSelect.begin(Pin_ButtonSelect, true, INPUT_PULLUP);

  for(int i = 0; i < outputButtonCount; i++) {
    pinMode(outputButtonPins[i], OUTPUT);
    analogWriteFrequency(outputButtonPins[i], 187500);
  }

  XInput.setTriggerRange(0, 1);
  XInput.setJoystickRange(0, 1000);
  XInput.setAutoSend(false);

  XInput.begin();
}

void loop() 
{
  boolean upPad = ButtonUpPad.update();
  boolean downPad = ButtonDownPad.update();
  boolean leftPad = ButtonLeftPad.update();
  boolean rightPad = ButtonRightPad.update();
  boolean buttonA = ButtonA.update();
  boolean buttonB = ButtonB.update();
  boolean buttonX = ButtonX.update();
  boolean buttonY = ButtonY.update();
  boolean buttonLB = ButtonLB.update();
  boolean buttonRB = ButtonRB.update();
  boolean buttonL3 = ButtonL3.update();
  boolean buttonR3 = ButtonR3.update();
  boolean buttonStart = ButtonStart.update();
  boolean buttonSelect = ButtonSelect.update();
  int buttonLT = ButtonLT.update();
  int buttonRT = ButtonRT.update();
  
  // Calibration trigger: hold START+BACK to start, release to finish
  bool calibStartCombo = buttonLB && buttonRB && downPad && buttonA && buttonLT == 1 && buttonRT == 1;
  bool calibStopCombo = buttonLB && buttonRB && upPad && buttonY && buttonLT == 1 && buttonRT == 1;
  static elapsedMillis calibHold;
  if (calibStartCombo)
  {
    if (calibHold > 2000 && !JoyCal.active && JoyCal.canAlter())
      JoyCal.start();
  }
  else
    calibHold = 0;
  
  // Raw reads
  int rawLX = analogRead(1);
  int rawLY = analogRead(0);
  int rawRX = analogRead(7);
  int rawRY = analogRead(6);

  // During calibration, learn the envelope and return so no signals get sent to the computer
  if (JoyCal.active) 
  {
    if (JoyCal.canAlter())
    {
      bool clearCombo = buttonStart && buttonSelect;
      bool flipRight = buttonR3 && buttonSelect;
      bool flipLeft = buttonL3 && buttonSelect;
      bool increaseDeadzone = upPad && buttonSelect;
      bool decreaseDeadzone = downPad && buttonSelect;

      if (calibStopCombo) 
      {
        JoyCal.finish();
        saveCalibration();
      }
      else if (clearCombo)
      {
        JoyCal.resetTimer();
        clearCalibration();
      }
      else if (flipRight)
      {
        JoyCal.flipRight();
      }
      else if (flipLeft)
      {
        JoyCal.flipLeft();
      }
      else if (increaseDeadzone)
      {
        JoyCal.deadzoneAddLeft(25, 25);
        JoyCal.deadzoneAddRight(25, 25);
      }
      else if (decreaseDeadzone)
      {
        JoyCal.deadzoneAddLeft(-25, -25);
        JoyCal.deadzoneAddRight(-25, -25);
      }
    }

    JoyCal.sample(rawLX, rawLY, rawRX, rawRY);
    return;
  }

  // Map to output range using current calibration (or naive until finalized)
  int leftStickX  = JoyCal.lx.mapToOut(rawLX);
  int leftStickY  = JoyCal.ly.mapToOut(rawLY);
  int rightStickX = JoyCal.rx.mapToOut(rawRX);
  int rightStickY = JoyCal.ry.mapToOut(rawRY);
 
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

  rumble();  

  // === Conditional send: only when state changed or every ~5ms ===
  static elapsedMillis xinputReportTimer;
  static uint32_t lastHash = 0;

  uint32_t h = computeReportHash(upPad, downPad, leftPad, rightPad, buttonA, buttonB, buttonX, buttonY, buttonLB, buttonRB, buttonL3, buttonR3, 
                                  buttonStart, buttonSelect, leftStickX, leftStickY, rightStickX, rightStickY, buttonLT, buttonRT);

  if (h != lastHash || xinputReportTimer >= REPORT_INTERVAL_MS) 
  {
    XInput.send();
    lastHash = h;
    xinputReportTimer = 0;
  }
}
