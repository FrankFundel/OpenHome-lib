#include "DHTesp.h"
#include <Arduino.h>
#include <analogWrite.h>
#include <WiFi.h>
#include "OpenHome.h"

#define DHTpin 33

#include "time.h"
#include <SPI.h>
#define  ENABLE_GxEPD2_display 0
#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <U8g2_for_Adafruit_GFX.h>

#define SCREEN_WIDTH   296
#define SCREEN_HEIGHT  128

static const uint8_t EPD_BUSY = 25;
static const uint8_t EPD_CS   = 15;
static const uint8_t EPD_RST  = 26; 
static const uint8_t EPD_DC   = 27; 
static const uint8_t EPD_SCK  = 13;
static const uint8_t EPD_MISO = 12;
static const uint8_t EPD_MOSI = 14;

GxEPD2_3C<GxEPD2_290c, GxEPD2_290c::HEIGHT> display(GxEPD2_290c(/*CS=D8*/ EPD_CS, /*DC=D3*/ EPD_DC, /*RST=D4*/ EPD_RST, /*BUSY=D2*/ EPD_BUSY));

DHTesp dht;
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

const int LED_PIN = 19;

const char* ssid = "WLAN-9C5E53";
const char* password =  "0570634118259165";

const char* oh_endpoint = "http://open-home.herokuapp.com/";
const char* oh_uid = "hmIqS0i5ACTRQDqpoH4ptKGttGH2";
const char* oh_did = "-LuHvsoSDF9v0kBR5tB8";

bool text_mode = false;
int lights = 50;
String text = "And still no arrests?";

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;
unsigned long clockTime;
String time_string = "00:51";

unsigned long sensorTime = 0;

void onDisconnectCallback() {
  Serial.println("Disonnected from Server");
}

void onParam(String event, JsonObject data, String sid) {
  if("child") {
    if(data["cid"] == "0" && data["pid"] == "value") {
      lights = data["value"];
      Serial.print("Set lights to "); Serial.println(lights);
      analogWrite(LED_PIN, lights, 100);
    } else if(data["cid"] == "1" && data["pid"] == "text") {
      text = data["value"].as<String>();
      Serial.print("Set text to "); Serial.println(text);
      if(text_mode) drawText();
    } else if(data["cid"] == "2" && data["pid"] == "state") {
      text_mode = data["value"].as<bool>();
      Serial.print("Set text_mode to "); Serial.println(text_mode);
      if(text_mode) drawText(); else drawClock();
    }
  } else if("value") {
    
  }
}

void onEventCallback(String type, String event, JsonObject data, String sid) {
  if(type == "param") {
    onParam(event, data, sid);
  } else if(type == "device") {
    
  } else {
    if(event == "connect") {
      JsonObject info = data["info"];
      JsonObject device = data["devices"][oh_did];
      JsonObject log = data["log"];

      text_mode = device["controls"][2]["params"]["state"].as<bool>();
      text = device["controls"][1]["params"]["text"].as<String>();
      lights = device["controls"][0]["params"]["value"];
      analogWrite(LED_PIN, lights, 100);
      if(text_mode) drawText(); else drawClock();
      
      Serial.println("Connected to Server");
    }
  }
}

void updateSensors() {
  Serial.println("updating sensors");
  float humid = getHumidity();
  float temp = getTemperature();
  
  StaticJsonDocument<200> doc;
  doc["did"] = oh_did;
  doc["cid"] = 3;
  doc["pid"] = "value";
  doc["value"] = temp;
  doc["log"] = false;

  OpenHome::setChild("param", doc.as<JsonObject>());
  
  doc["cid"] = 4;
  doc["value"] = humid;
  OpenHome::setChild("param", doc.as<JsonObject>());
}

void drawClock() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%H:%M");
  char buf[6];
  strftime(buf, sizeof(buf), "%H:%M", &timeinfo);
  time_string = buf;

  //draw
  u8g2Fonts.setFont(u8g2_font_inr46_mf);
  
  display.fillScreen(GxEPD_RED);
  int x = 148;
  int y = 64;
  int16_t  x1, y1; uint16_t w, h;
  display.setTextWrap(false);
  display.getTextBounds(time_string, x, y, &x1, &y1, &w, &h);
  u8g2Fonts.setCursor(x - w * 2 - 30, y + h * 3);
  u8g2Fonts.print(time_string);
  display.display(false);
}

void drawText() {
  Serial.println(text);
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);

  display.fillScreen(GxEPD_RED);
  int x = 148;
  int y = 64;
  int16_t  x1, y1; uint16_t w, h;
  display.setTextWrap(false);
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
  u8g2Fonts.setCursor(x - w / 2 - 30, y + h / 2);
  u8g2Fonts.print(text);
  display.display(false);
}

float getHumidity() {
  float humidity = dht.getHumidity();
  Serial.print("Humidity: "); Serial.println(humidity);
  return humidity;
}

float getTemperature() {
  float temperature = dht.getTemperature();
  Serial.print("Temperature: "); Serial.println(temperature);
  return temperature;
}

void setup() {
  Serial.begin(115200);
  
  dht.setup(DHTpin, DHTesp::DHT11);
  pinMode(LED_PIN, OUTPUT);
  
  // Connect to wifi
  WiFi.begin(ssid, password);

  // Wait some time to connect to wifi
  for(int i = 0; i < 10 && WiFi.status() != WL_CONNECTED; i++) {
      Serial.print(".");
      delay(1000);
  }

  if(WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected to Wifi");
  
    OpenHome::init();
    OpenHome::onDisconnect(onDisconnectCallback);
    OpenHome::onEvent(onEventCallback);
    
    OpenHome::connect(oh_endpoint, oh_uid, oh_did);
  }
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  clockTime = millis();
   
  InitializeDisplay();
}

void loop() {
  OpenHome::loop();
  
  if(!text_mode && millis() - clockTime >= 60000) {
    drawClock();
    clockTime = millis();
  }
  if(millis() - sensorTime >= 60000) {
    updateSensors();
    sensorTime = millis();
  }
}

void InitializeDisplay() {
  display.init(0);
  SPI.end();
  SPI.begin(EPD_SCK, EPD_MISO, EPD_MOSI, EPD_CS);
  display.setRotation(1);
  u8g2Fonts.begin(display);
  u8g2Fonts.setForegroundColor(GxEPD_BLACK);
  u8g2Fonts.setBackgroundColor(GxEPD_RED);
  display.fillScreen(GxEPD_RED);
  display.setFullWindow();
}
