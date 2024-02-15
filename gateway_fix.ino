#include <painlessMesh.h>
#include <ArduinoJson.h>
#include "soc/rtc_cntl_reg.h"

#define MESH_PREFIX "XirkaMesh"
#define MESH_PASSWORD "12345678"
#define MESH_PORT 5555

unsigned long t1;
bool flag = 0;
StaticJsonDocument<200> controlLedDoc;

Scheduler userScheduler;
painlessMesh mesh;

uint32_t nodeIdCon;
// uint32_t destId1 = 486277825;
// uint32_t destId2 = 486284217;

const int MAX_NODES_DIR = 10;
uint32_t nodeDir[MAX_NODES_DIR] = { };
int jumlahNode = 0;

const int MAX_NODES_SUB = 10;
uint32_t nodeSub[MAX_NODES_SUB];
int jumlahNodeSub = 0;

void sendLedControlOn() {
  uint8_t msg = 1;
  controlLedDoc["LAMP"] = msg;
  String jsonLedOn;
  serializeJson(controlLedDoc, jsonLedOn);

  for (int i = 0; i < jumlahNode; i++) {
    if (nodeDir[i] != nodeIdCon) {
      mesh.sendSingle(nodeDir[i], jsonLedOn);
      Serial2.println(jsonLedOn);
      Serial.printf("sending message: %s To Node: %u\n", jsonLedOn, nodeDir[i]);
    }
  }
}

void sendLedControlOff() {
  uint8_t msg = 0;
  controlLedDoc["LAMP"] = msg;
  String jsonLedOff;
  serializeJson(controlLedDoc, jsonLedOff);

  for (int i = 0; i < jumlahNode; i++) {
    if (nodeDir[i] != nodeIdCon) {
      mesh.sendSingle(nodeDir[i], jsonLedOff);
      Serial2.println(jsonLedOff);
      Serial.printf("sending message: %s To Node: %u\n", jsonLedOff, nodeDir[i]);
    }
  }
}

void receivedCallback(uint32_t from, String& msg) {
  uint32_t nodeId = mesh.getNodeId();
  Serial.printf("received from %u msg=%s\n", from, msg.c_str());
}

void newConnectionCallback(uint32_t nodeId) {
  Serial.printf("New Connection, nodeId = %u\n", nodeId);

  // bool shouldAddToNodeDir = true;
  // if (nodeId % 2 == 0) {
  //   shouldAddToNodeDir = false;
  // }

  // if (shouldAddToNodeDir && jumlahNode < MAX_NODES_DIR) {
  //   nodeDir[jumlahNode++] = nodeId;
  // } else {
  //   if (jumlahNodeSub < MAX_NODES_SUB) {
  //     nodeSub[jumlahNodeSub++] = nodeId;
  //   }
  // }
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
  Serial2.begin(9600, SERIAL_8N1, 16, 17);  // RX2=16, TX2=17

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

  if (millis() - t1 > 2000 && flag == 0) {
    sendLedControlOn();
    t1 = millis();
    flag = 1;
  }

  if (millis() - t1 > 2000 && flag == 1) {
    sendLedControlOff();
    t1 = millis();
    flag = 0;
  }
}
