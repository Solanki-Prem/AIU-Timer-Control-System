#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <MD_Parola.h>
#include <MD_MAX72XX.h>
#include <SPI.h>
#include <esp_mac.h>

// BLE UUIDs (Standard Nordic UART Service)
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic;
String rxValue = "";
bool deviceConnected = false;
bool authenticated = false;

// Display Config
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CS_PIN    5
#define CLK_PIN   18
#define DATA_PIN  23

MD_Parola P = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

// RGB Pins
#define BLUE_PIN   14
#define GREEN_PIN  26
#define RED_PIN    21
#define COMMON_ANODE true

void setRGB(bool r, bool g, bool b) {
  if (COMMON_ANODE) {
    digitalWrite(RED_PIN, !r); digitalWrite(GREEN_PIN, !g); digitalWrite(BLUE_PIN, !b);
  } else {
    digitalWrite(RED_PIN, r); digitalWrite(GREEN_PIN, g); digitalWrite(BLUE_PIN, b);
  }
}

void setYellow() { setRGB(1, 1, 0); }

unsigned long SETUP_TIME = 0, MIN_TIME = 0, PERF_TIME = 0;
int displayBrightness = 5;

enum SystemState { DONE, START_BLINK, SETUP_COUNTDOWN, WAIT_FOR_MAIN, COUNTDOWN, GRACE_COUNTUP, GRACE_END_FLASH, TESTING };
int graceFlashCount = 0;
bool graceFlashOn = false;
SystemState currentState = DONE;

// Scrolling idle message
const char scrollMsg[] = "   Welcome to Pashimanchal AIU 39th Inter University West Zone Youth Festival at CVM University   ";
bool inScrollMode = true;

// Test state machine
int testStep = 0;
unsigned long testStepStart = 0;

unsigned long startTime, stateStartTime, lastDisplayUpdate = 0;
unsigned long minBlinkStart = 0, lastSecBlinkStart = 0;
int blinkCounter = 0;
bool blinkState = false, minBlinked = false, isMinBlinking = false, isLastSecBlinking = false;
int lastSecTrack = 0;
bool setupDone = false;
char txtM1[2], txtM2[2], txtS1[2], txtS2[2];

// Grace end blink
unsigned long graceEndBlinkStart = 0;

void sendResponse(const char* msg) {
  if (deviceConnected) {
    pTxCharacteristic->setValue(msg);
    pTxCharacteristic->notify();
  }
}

void switchToScrollMode() {
  P.begin(1);
  P.setIntensity(0, displayBrightness);
  P.displayText(scrollMsg, PA_LEFT, 40, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
  inScrollMode = true;
}

void switchToTimerMode() {
  // Clear stale display buffers to avoid old time flashing for one frame
  sprintf(txtM1, "0"); sprintf(txtM2, "0");
  sprintf(txtS1, "0"); sprintf(txtS2, "0");
  P.begin(4);
  for (int i = 0; i < 4; i++) { P.setZone(i, i, i); P.setIntensity(i, displayBrightness); }
  P.displayZoneText(0, txtS2, PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
  P.displayZoneText(1, txtS1, PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
  P.displayZoneText(2, txtM2, PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
  P.displayZoneText(3, txtM1, PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
  inScrollMode = false;
}

// BLE Callbacks
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      authenticated = false;
    };
    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      authenticated = false;
      pServer->getAdvertising()->start();
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      String value = pCharacteristic->getValue();
      if (value.length() > 0) rxValue = value;
    }
};

unsigned long convertToMillis(String t) {
  int colonIdx = t.indexOf(':');
  if (colonIdx < 1) return 0;
  return (((unsigned long)t.substring(0, colonIdx).toInt() * 60UL) + t.substring(colonIdx + 1).toInt()) * 1000UL;
}

void drawColon() {
  MD_MAX72XX *mx = P.getGraphicObject();
  mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
  mx->setPoint(2, 15, true); mx->setPoint(5, 15, true);
  mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
}

void updateMatrixDisplay(long totalMillis) {
  if (totalMillis < 0) totalMillis = 0;
  int totalSeconds = totalMillis / 1000;
  int m = totalSeconds / 60;
  int s = totalSeconds % 60;
  if (m >= 100) {
    // H:MM format for large times (drop seconds precision)
    int h = m / 60;
    int hm = m % 60;
    if (h >= 10) sprintf(txtM1, "%d", h / 10); else sprintf(txtM1, " ");
    sprintf(txtM2, "%d", h % 10);
    sprintf(txtS1, "%d", hm / 10);
    sprintf(txtS2, "%d", hm % 10);
  } else {
    sprintf(txtM1, "%d", m / 10); sprintf(txtM2, "%d", m % 10);
    sprintf(txtS1, "%d", s / 10); sprintf(txtS2, "%d", s % 10);
  }
  for(int i=0; i<4; i++) P.displayReset(i);
}

void displayRaw(const char* m1, const char* m2, const char* s1, const char* s2) {
  sprintf(txtM1, "%s", m1); sprintf(txtM2, "%s", m2);
  sprintf(txtS1, "%s", s1); sprintf(txtS2, "%s", s2);
  for(int i=0; i<4; i++) P.displayReset(i);
}

void setup() {
  Serial.begin(115200);

  // Read MAC address and build unique device name
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_BT);
  char deviceName[16];
  sprintf(deviceName, "NOT_CNT_%02X%02X", mac[4], mac[5]);
  Serial.print("Device name: ");
  Serial.println(deviceName);

  // Initialize BLE
  BLEDevice::init(deviceName);
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pTxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
  pTxCharacteristic->addDescriptor(new BLE2902());
  BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
  pRxCharacteristic->setCallbacks(new MyCallbacks());
  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  BLEDevice::startAdvertising();

  pinMode(RED_PIN, OUTPUT); pinMode(GREEN_PIN, OUTPUT); pinMode(BLUE_PIN, OUTPUT);
  setRGB(0,0,0);

  // Start in scroll mode
  switchToScrollMode();
}

void loop() {
  unsigned long now = millis();

  // Handle scrolling in DONE state
  if (inScrollMode) {
    if (P.displayAnimate()) {
      P.displayReset(); // Restart scroll when complete
    }
  } else {
    P.displayAnimate();
    drawColon();
  }

  // Process Received BLE Commands
  if (rxValue.length() > 0) {
    String data = rxValue; rxValue = ""; data.trim();

    // RESET always works (even unauthenticated, even during test)
    if (data == "RESET") {
      currentState = DONE; setupDone = false; setRGB(0,0,0);
      switchToScrollMode();
    }
    // Auth command — always processed
    else if (data.startsWith("AUTH=")) {
      if (data == "AUTH=##2026##") {
        authenticated = true;
        sendResponse("AUTH_OK");
      } else {
        sendResponse("AUTH_FAIL");
      }
    }
    // All other commands require authentication
    else if (authenticated) {
      if (data == "START") {
        if (currentState == DONE || currentState == WAIT_FOR_MAIN) {
          if (inScrollMode) switchToTimerMode();
          stateStartTime = now; blinkCounter = 0; blinkState = true;
          setRGB(0, 1, 0); currentState = START_BLINK;
        }
      }
      else if (data == "STOP") {
        if (currentState == GRACE_COUNTUP) {
          graceFlashCount = 0;
          graceFlashOn = true;
          setRGB(1, 0, 0);
          graceEndBlinkStart = millis();
          currentState = GRACE_END_FLASH;
        }
      }
      else if (data.startsWith("SETUP=")) { SETUP_TIME = convertToMillis(data.substring(6)); setupDone = false; }
      else if (data.startsWith("MIN="))   MIN_TIME = convertToMillis(data.substring(4));
      else if (data.startsWith("PERF="))  PERF_TIME = convertToMillis(data.substring(5));
      else if (data.startsWith("BRIGHT=")) {
        int val = data.substring(7).toInt();
        if (val >= 0 && val <= 15) {
          displayBrightness = val;
          if (inScrollMode) {
            P.setIntensity(0, val);
          } else {
            for(int i=0; i<4; i++) P.setIntensity(i, val);
          }
        }
      }
      else if (data == "TEST") {
        if (inScrollMode) switchToTimerMode();
        currentState = TESTING;
        testStep = 0;
        testStepStart = now;
        setRGB(1, 0, 0); // Start with red
      }
    }
  }

  switch (currentState) {
    case DONE:
      // Scrolling handled above
      break;

    case START_BLINK:
      if (now - stateStartTime >= 300) {
        stateStartTime = now; blinkState = !blinkState; setRGB(0, blinkState, 0);
        if (!blinkState && ++blinkCounter >= 2) {
          if (SETUP_TIME > 0 && !setupDone) currentState = SETUP_COUNTDOWN;
          else { currentState = COUNTDOWN; minBlinked = false; isMinBlinking = false; isLastSecBlinking = false; lastSecTrack = 0; }
          startTime = millis();
        }
      }
      break;

    case SETUP_COUNTDOWN:
      {
        long remSetup = (long)SETUP_TIME - (now - startTime);
        if (now - lastDisplayUpdate >= 100) { updateMatrixDisplay(remSetup > 0 ? (remSetup + 999) : 0); lastDisplayUpdate = now; }
        if (remSetup <= 0) { setYellow(); setupDone = true; currentState = WAIT_FOR_MAIN; }
      }
      break;

    case WAIT_FOR_MAIN: break;

    case COUNTDOWN:
      {
        unsigned long elapsed = now - startTime;
        long remaining = (long)PERF_TIME - elapsed;
        if (now - lastDisplayUpdate >= 100) { updateMatrixDisplay(remaining > 0 ? (remaining + 999) : 0); lastDisplayUpdate = now; }
        if (!minBlinked && elapsed >= MIN_TIME) { isMinBlinking = true; minBlinkStart = now; minBlinked = true; setRGB(0,0,1); }
        if (isMinBlinking && (now - minBlinkStart >= 400)) { setRGB(0,0,0); isMinBlinking = false; }
        int currentSecLeft = (remaining + 999) / 1000;
        if (!isLastSecBlinking && currentSecLeft <= 3 && currentSecLeft > 0 && lastSecTrack != currentSecLeft) {
            lastSecTrack = currentSecLeft; isLastSecBlinking = true; lastSecBlinkStart = now; setRGB(0, 0, 1);
        }
        if (isLastSecBlinking && (now - lastSecBlinkStart >= 400)) { setRGB(0, 0, 0); isLastSecBlinking = false; }
        if (remaining <= 0) {
          // Auto-start grace countup immediately
          setRGB(0, 0, 0); // No lights during grace
          startTime = millis();
          currentState = GRACE_COUNTUP;
          sendResponse("GRACE_STARTED");
        }
      }
      break;

    case GRACE_COUNTUP:
      {
        unsigned long elapsedGrace = now - startTime;
        if (now - lastDisplayUpdate >= 100) { updateMatrixDisplay(elapsedGrace); lastDisplayUpdate = now; }
        // No red light during grace — don't distract performers
      }
      break;

    case GRACE_END_FLASH:
      {
        // Flash red LED 5 times: 300ms on, 300ms off per flash
        unsigned long flashElapsed = now - graceEndBlinkStart;
        if (graceFlashCount >= 5) {
          // Done flashing
          currentState = DONE; setupDone = false; setRGB(0,0,0);
          switchToScrollMode();
          sendResponse("CYCLE_DONE");
        } else {
          if (graceFlashOn && flashElapsed >= 300) {
            setRGB(0, 0, 0); graceFlashOn = false; graceEndBlinkStart = now;
          } else if (!graceFlashOn && flashElapsed >= 300) {
            graceFlashCount++;
            if (graceFlashCount < 5) {
              setRGB(1, 0, 0); graceFlashOn = true; graceEndBlinkStart = now;
            }
          }
        }
      }
      break;

    case TESTING:
      {
        unsigned long elapsed = now - testStepStart;
        switch (testStep) {
          case 0: // Red LED on
            if (elapsed >= 400) { setRGB(0, 0, 0); testStep = 1; testStepStart = now; }
            break;
          case 1: // Red off, Green on
            setRGB(0, 1, 0); testStep = 2; testStepStart = now;
            break;
          case 2: // Green LED on
            if (elapsed >= 400) { setRGB(0, 0, 0); testStep = 3; testStepStart = now; }
            break;
          case 3: // Green off, Blue on
            setRGB(0, 0, 1); testStep = 4; testStepStart = now;
            break;
          case 4: // Blue LED on
            if (elapsed >= 400) { setRGB(0, 0, 0); testStep = 5; testStepStart = now; displayRaw("8", "8", "8", "8"); }
            break;
          case 5: // Display 88:88
            if (elapsed >= 600) { testStep = 6; testStepStart = now; }
            break;
          case 6: // 10-second countdown
            {
              long remaining = 10000 - (long)elapsed;
              if (now - lastDisplayUpdate >= 100) {
                updateMatrixDisplay(remaining > 0 ? (remaining + 999) : 0);
                lastDisplayUpdate = now;
              }
              if (remaining <= 0) {
                updateMatrixDisplay(0);
                currentState = DONE;
                switchToScrollMode();
                sendResponse("TEST_OK");
              }
            }
            break;
        }
      }
      break;
  }
}
