#include <painlessMesh.h>
#include <Wire.h>
#include <RTClib.h>
#include <ArduinoJson.h>
#include "soc/rtc_cntl_reg.h"

#define MESH_PREFIX "XirkaMesh"
#define MESH_PASSWORD "12345678"
#define MESH_PORT 5555

#define btn_1_pin 32  // button down
#define btn_2_pin 35  // button cancel
#define lamp_control_pin 12
#define RX_stm32 16
#define TX_stm32 17

#define MINVAL_VCELL 0
#define MAXVAL_VCELL 4.00
#define MINVAL_PWM 0
#define MAXVAL_PWM 100

#define lamp_ON analogWrite(lamp_control_pin, 255);
#define lamp_OFF analogWrite(lamp_control_pin, 0);

#define H11 39600
#define H1130 41400
#define H1300 46800

char buffValue[100];
StaticJsonDocument<200> doc;
StaticJsonDocument<200> newDoc;

RTC_DS3231 rtc;
unsigned long t1;
unsigned long tl;
bool flag = 0;
bool lampStatus = false;
bool lampStatusPrevious = false;

struct LampSetting {
  uint8_t percentage;
};

LampSetting currentSetting;

Scheduler userScheduler;
painlessMesh mesh;

//function for receive data from microcontroller and send data via mesh
void receiveSerial2() {
  while (Serial2.available() > 0) {
    static uint8_t x = 0;
    buffValue[x] = (char)Serial2.read();
    x += 1;
    if (buffValue[x - 1] == '\n') {
      DeserializationError error = deserializeJson(doc, buffValue);
      if (!error) {
        float vsolar = doc["VSOLAR"];
        float vbat = doc["VBAT"];
        bool stat = doc["STAT"];
        Serial.print(String("VSOLAR = ") + vsolar);
        Serial.print(String(" VBAT = ") + vbat);
        Serial.println(String(" STATUS_CHARGE = ") + (stat ? "true" : "false"));

        newDoc["VSOLAR"] = round(vsolar * 100.0) / 100.0;
        newDoc["VBAT"] = round(vbat * 100.0) / 100.0;  // Round vbat to two decimal places
        newDoc["STAT"] = stat;

        String jsonString;
        serializeJson(newDoc, jsonString);
        mesh.sendSingle(443009725, jsonString);
        // Serial.println("Sending message: " + jsonString);

        updateLamp(vsolar);

        // String route = mesh.subConnectionJson();
        // Serial.println("connection :" + route);
      }
      x = 0;
      memset(buffValue, 0, sizeof(buffValue));
      break;
    }
  }
}

// function for lamp condition
void initLamp() {
  pinMode(lamp_control_pin, OUTPUT);
}

void setPercentLamp(uint8_t percentage) {
  uint8_t dim = map(percentage, 0, 255, 0, 100);
  analogWrite(lamp_control_pin, dim);
}

// void activeLamp() {
//   if (millis() - t1 > 1000 && flag == 0) {
//     analogWrite(lamp_control_pin, 100);
//     t1 = millis();
//     flag = 1;
//   }
// }

void deactiveLamp() {
  setPercentLamp(0);
}

void updateLamp(float vsolar) {
  DateTime now = rtc.now();
  uint8_t hour = now.hour();
  uint8_t minute = now.minute();
  uint8_t second = now.second();
  int setTime = (hour * 3600) + (minute * 60) + second;
  if (vsolar <= 4.00) {
    if (setTime >= 55800 && setTime < 56700) {
      currentSetting.percentage = 20;
      setPercentLamp(currentSetting.percentage);
    } else if (setTime >= 56700 && setTime < 56700) {
      currentSetting.percentage = 50;
      setPercentLamp(currentSetting.percentage);
    } else if (setTime >= 56700 && setTime < 57600) {
      currentSetting.percentage = 80;
      setPercentLamp(currentSetting.percentage);
    } else if (setTime >= 57600) {
      currentSetting.percentage = 20;
      setPercentLamp(currentSetting.percentage);
    }
  } else {
    currentSetting.percentage = 0;
    setPercentLamp(currentSetting.percentage);
  }

  StaticJsonDocument<200> lampDoc;
  lampDoc["percentage"] = currentSetting.percentage;
  String jsonLamp;
  serializeJson(lampDoc, jsonLamp);
  mesh.sendSingle(443009725, jsonLamp);
  Serial.println("Sending message: " + jsonLamp);
}

// function for get real-time clock
void getTimeRtc() {
  DateTime now = rtc.now();
  Serial.print(dayStr(now.dayOfTheWeek()));
  Serial.print(", ");
  print2digits(now.day());
  Serial.print("/");
  print2digits(now.month());
  Serial.print("/");
  Serial.print(now.year());
  Serial.print(" ");

  print2digits(now.hour());
  Serial.print(":");
  print2digits(now.minute());
  Serial.print(":");
  print2digits(now.second());
  Serial.println();
}

void print2digits(int number) {
  if (number < 10) {
    Serial.print("0");
  }
  Serial.print(number);
}

const char* dayStr(uint8_t dow) {
  switch (dow) {
    case 1: return "Monday";
    case 2: return "Tuesday";
    case 3: return "Wednesday";
    case 4: return "Thursday";
    case 5: return "Friday";
    case 6: return "Saturday";
    case 7: return "Sunday";
    default: return "Unknown";
  }
}

void receivedCallback(uint32_t from, String& msg) {
  uint32_t nodeId = mesh.getNodeId();
  Serial.printf("received from %u msg=%s\n", from, msg.c_str());
}

void newConnectionCallback(uint32_t nodeId) {
  Serial.printf("New Connection, nodeId = %u\n", nodeId);
}

void changedConnectionCallback() {
  Serial.printf("Changed connections\n");
}

void nodeTimeAdjustedCallback(int32_t offset) {
  Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(), offset);
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(9600);
  Serial2.begin(9600, SERIAL_8N1, 16, 17);


  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1)
      ;
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, let's set the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  initLamp();
  deactiveLamp();

  mesh.setDebugMsgTypes(ERROR | STARTUP);
  mesh.init(MESH_PREFIX, MESH_PASSWORD);
  mesh.setRoot(true);
  mesh.setContainsRoot(true);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
}

void loop() {
  mesh.update();
  userScheduler.execute();
  if (millis() - t1 > 1) {
    receiveSerial2();
    t1 = millis();
  }
  if (millis() - tl > 1000) {
    getTimeRtc();
    tl = millis();
  }
}