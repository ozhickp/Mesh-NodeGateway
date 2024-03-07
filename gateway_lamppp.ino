#include <painlessMesh.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include "soc/rtc_cntl_reg.h"

#define MESH_PREFIX "XirkaMesh"
#define MESH_PASSWORD "12345678"
#define MESH_PORT 5555

uint8_t nodeNumber;
char buffValue[100];
StaticJsonDocument<200> doc;
StaticJsonDocument<200> newDoc;
StaticJsonDocument<200> controlLedDoc;

unsigned long t1;
unsigned long tl;
uint32_t newNodes;
bool flag = 0;

float vsolar;
float vbat;

Scheduler userScheduler;
painlessMesh mesh;

const int MAX_NODES_DIR = 1;
uint32_t nodeDir[MAX_NODES_DIR] = {};
int jumlahNodeDir = 0;


void ledSetup() {
  uint8_t msg = 1;
  controlLedDoc["LAMP"] = msg;
  String jsonLedOn;
  serializeJson(controlLedDoc, jsonLedOn);
  Serial2.println(jsonLedOn);
}

// void updateLamp() {
//   if (vsolar >= 4.00) {
//     uint8_t on = 1;
//     controlLedDoc["LAMP"] = on;
//     String jsonLedOn;
//     Serial2.println(jsonLedOn);
//   } else {
//     uint8_t off = 0;
//     controlLedDoc["LAMP"] = off;
//     String jsonLedOff;
//     Serial2.println(jsonLedOff);
//   }
// }

//function for receive data from microcontroller and send data via mesh
void receiveSerial2() {
  while (Serial2.available() > 0) {
    static uint8_t x = 0;
    buffValue[x] = (char)Serial2.read();
    x += 1;
    if (buffValue[x - 1] == '\n') {
      DeserializationError error = deserializeJson(doc, buffValue);
      if (!error) {
        vsolar = doc["VSOLAR"];
        vbat = doc["VBAT"];

        //updateLamp();
        nodeNumber = 0;

        newDoc["Node"] = nodeNumber;
        newDoc["VSOLAR"] = round(vsolar * 100.0) / 100.0;
        newDoc["VBAT"] = round(vbat * 100.0) / 100.0;
        newDoc["LAMP"] = (controlLedDoc["LAMP"] == 1) ? "ON" : "OFF";

        String jsonString;
        serializeJson(newDoc, jsonString);
        // mesh.sendSingle(newNodes, jsonString);
        Serial.println("Data: " + jsonString);
      }
      x = 0;
      memset(buffValue, 0, sizeof(buffValue));
      break;
    }
  }
}

void sendMessage() {
  String msg = "a";
  mesh.sendSingle(newNodes, msg);
  //Serial.println(msg);
}

void addNodeConnection() {
  if (jumlahNodeDir < MAX_NODES_DIR) {
    nodeDir[jumlahNodeDir++] = newNodes;
    Serial.printf("Jumlah Node Direct: %d\tNode direct:%d\n", jumlahNodeDir, nodeDir[MAX_NODES_DIR - 1]);
  } else {
    Serial.println("Jumlah node direct penuh.");
  }
}

void lostConnect() {
  if (!mesh.isConnected(nodeDir[MAX_NODES_DIR - 1])) {
    Serial.println("Node Direct disconnected.");
    jumlahNodeDir = 0;
    Serial.printf("Jumlah Node Direct: %d\n", jumlahNodeDir);
  }
}

void receivedCallback(uint32_t from, String& msg) {
  uint32_t nodeId = mesh.getNodeId();
  Serial.printf("received from %u msg=%s\n", from, msg.c_str());
}

void newConnectionCallback(uint32_t nodeId) {
  Serial.printf("New Connection, nodeId = %u\n", nodeId);
  newNodes = nodeId;
  addNodeConnection();
}

void changedConnectionCallback() {
  Serial.printf("Changed connections\n");
  lostConnect();
}

void nodeTimeAdjustedCallback(int32_t offset) {
  Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(), offset);
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(9600);
  Serial2.begin(9600, SERIAL_8N1, 16, 17);

  ledSetup();

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

  if (millis() - t1 > 1000) {
    receiveSerial2();
    t1 = millis();
  }

  if (millis() - tl > 1000) {
    sendMessage();
    tl = millis();
  }
}