/*
  OpenHome.h - Library for OpenHome.
  Created by Frank Fundel, November 23, 2019.
  Released into the public domain.
*/

#include "Arduino.h"
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>

using namespace websockets;

namespace OpenHome
{
WebsocketsClient client;

void (*onDisconnectCallback_)(void);
void (*onErrorCallback_)(String);
void (*onEventCallback_)(String, String, JsonObject, String);

void onMessageCallback(WebsocketsMessage message) {
  Serial.print("Got Message: "); Serial.println(message.data());

  DynamicJsonDocument doc(2048);
  deserializeJson(doc, message.data());

  onEventCallback_(doc["type"], doc["event"], doc["data"], doc["sid"]);
}

void onEventsCallback(WebsocketsEvent event, String data) {
  if (event == WebsocketsEvent::ConnectionOpened) {
    //nothing
  } else if (event == WebsocketsEvent::ConnectionClosed) {
    onDisconnectCallback_();
  } else if (event == WebsocketsEvent::GotPing) {
    client.pong();
  } else if (event == WebsocketsEvent::GotPong) {
    client.ping();
  }
}

void init() {
  // Setup Callbacks
  client.onMessage(onMessageCallback);
  client.onEvent(onEventsCallback);
}

void connect(String endpoint, String uid, String did = "") {
  // Connect to server
  String url = endpoint + "?uid=" + uid;
  if (did != "") url += "&did=" + did;
  client.connect(url);
}

void disconnect() {
  client.close();
}

void loop() {
  client.poll();
}

void set(String type, String event, JsonObject data) {
  DynamicJsonDocument doc(2048);
  doc["type"] = type;
  doc["event"] = event;
  JsonObject dat = doc.createNestedObject("data");
  dat.set(data);

  String output;
  serializeJson(doc, output);
  Serial.println(output);
  client.send(output);
}

void setChild(String type, JsonObject data) {
  set(type, "child", data);
}

void setValue(String type, JsonObject data) {
  set(type, "value", data);
}

void onEvent(void (*onEventCallback)(String, String, JsonObject, String)) {
  onEventCallback_ = onEventCallback;
}

void onDisconnect(void (*onDisconnectCallback)(void)) {
  onDisconnectCallback_ = onDisconnectCallback;
}

void onError(void (*onErrorCallback)(String)) {
  onErrorCallback_ = onErrorCallback;
}
}
