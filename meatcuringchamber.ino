#include <DHT.h>
#include <Adafruit_ST7735.h>
#include <SD.h>
#include <avr/dtostrf.h>
#include "arduino_secrets.h"
#include <ArduinoMqttClient.h>
#include <WiFiNINA.h>

#define pinDHT1 A7
#define pinDHT2 A6
#define typeDHT DHT22
DHT DHTInt = DHT(pinDHT1, typeDHT);
DHT DHTExt = DHT(pinDHT2, typeDHT);

#define TFT_CS        10 // CS
#define TFT_RST        9 // RES
#define TFT_DC         8 // RS/DC
Adafruit_ST7735 TFTscreen = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

#define SD_CS   7

#define pinPushButton 2 // SW
#define pinRotarySwitchChannelA 3 // CLK
#define pinRotarySwitchChannelB 4 // DT

#define pinCooler A5
#define pinHumidifier A4
#define pinVentilator A3

#define pinRed A2
#define pinGreen A1

bool memButton = false;

bool alarmInitializeSD = false;
bool alarmConnectWiFi = false;
bool alarmConnectMQTT = false;
bool alarmRestoreSettings = false;
bool alarmSaveSettings = false;
bool alarmSensor = false;

const bool debugMode = false;
const bool mqttMode = true;

int currentSeconds = 0;

bool memStart = false;
int memSeconds = 0;

int indexLog = 1;
int intervalLog = 10;

char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

char broker[] = SECRET_BROKER;
int port = SECRET_PORT;

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

bool memSettingsSaved = false;
bool memSettingsRestored = false;

void TFTPrintText(int wid, int hei, char *text) {
  TFTscreen.setCursor(wid, hei);
  TFTscreen.print(text);
}

//void TFTPrintFloat(int wid, int hei, float value) {
//  char *text;
//  dtostrf(value,4, 1, text);
//  TFTPrintText(wid, hei, text);
//}

//void TFTPrintDoubleArrow(int wid, int hei) {
//  TFTscreen.setCursor(wid, hei);
//  TFTscreen.print(char(17));
//  TFTscreen.print(char(16));
//}

void SetLED(String color) {
  if (color == "red") {
    digitalWrite(pinRed, HIGH);
    digitalWrite(pinGreen, LOW);
  }
  else if (color == "green") {
    digitalWrite(pinRed, LOW);
    digitalWrite(pinGreen, HIGH);
  }
}

char dummyText[4];

float tempPV =  0.0;
float tempSP =  0.0;
float tempMaxErr = 0.0;
float tempMinErr = 0.0;
float tempExt = 0.0;

float tempOldPV =  0.0;
float tempOldSP =  0.0;
float tempOldMaxErr = 0.0;
float tempOldMinErr = 0.0;
float tempOldExt = 0.0;

float relhumPV = 0.0;
float relhumSP = 0.0;
float relhumMaxErr = 0.0;
float relhumMinErr = 0.0;
float relhumExt = 0.0;

float relhumOldPV = 0.0;
float relhumOldSP = 0.0;
float relhumOldMaxErr = 0.0;
float relhumOldMinErr = 0.0;
float relhumOldExt = 0.0;

int screenIndex = 0;
int screenSubIndex = 0;
bool screenSubIndexChanged = false;

int aState;
int aLastState;

bool runCooler = false;
bool runHumidifier = false;
bool runVentilator = false;

bool runCoolerOld = false;
bool runHumidifierOld = false;
bool runVentilatorOld = false;

void RestoreSettings() {
  if (SD.exists("settings.csv")) {
    File settingsFile = SD.open("settings.csv", FILE_READ);

    if (!settingsFile) {
      if (debugMode) Serial.println(F("SD - File for restoring settings not opened!"));
      alarmRestoreSettings = true;
    }
    else {
      int i = 0;
      char dummyChar;
      String dummyString[6] = {"", "", "", ""};

      while (settingsFile.available()) {
        dummyChar = settingsFile.read();
        if (dummyChar != ';') {
          dummyString[i] += dummyChar;
        }
        else if (dummyChar == ';') {
          i++;
        }
      }
      settingsFile.close();

      tempSP = dummyString[0].toFloat();
      tempMaxErr = dummyString[1].toFloat();
      tempMinErr = dummyString[2].toFloat();
      relhumSP = dummyString[3].toFloat();
      relhumMaxErr = dummyString[4].toFloat();
      relhumMinErr = dummyString[5].toFloat();

      if (debugMode) Serial.println(F("SD - Settings restored..."));
      alarmRestoreSettings = false;
      memSettingsRestored = true;
    }
  }
  else {
    if (debugMode) Serial.println(F("SD - File for restoring settings not found!"));
    alarmRestoreSettings = true;
  }
}

void SaveSettings() {
  if (SD.exists("settings.csv")) {
    SD.remove("settings.csv");
  }

  File settingsFile = SD.open("settings.csv", FILE_WRITE);

  if (!settingsFile) {
    if (debugMode) Serial.println(F("SD - File for saving settings not opened!"));
    alarmSaveSettings = true;
  }
  else {
    String settingsString = "";

    dtostrf(tempSP, 4, 1, dummyText);
    settingsString += dummyText;
    settingsString += ";";
    dtostrf(tempMaxErr, 4, 1, dummyText);
    settingsString += dummyText;
    settingsString += ";";
    dtostrf(tempMinErr, 4, 1, dummyText);
    settingsString += dummyText;
    settingsString += ";";
    dtostrf(relhumSP, 4, 1, dummyText);
    settingsString += dummyText;
    settingsString += ";";
    dtostrf(relhumMaxErr, 4, 1, dummyText);
    settingsString += dummyText;
    settingsString += ";";
    dtostrf(relhumMinErr, 4, 1, dummyText);
    settingsString += dummyText;

    settingsFile.println(settingsString);
    settingsFile.close();

    if (debugMode) Serial.println(F("SD - Settings saved..."));
    alarmSaveSettings = false;
    memSettingsSaved = true;
  }
}

void PublishMQTT() {
  mqttClient.beginMessage("sensor/int");
  mqttClient.print("sensorInt tempPV=");
  mqttClient.print(String(tempPV));
  mqttClient.endMessage();

  mqttClient.beginMessage("sensor/int");
  mqttClient.print("sensorInt relhumPV=");
  mqttClient.print(String(relhumPV));
  mqttClient.endMessage();

  mqttClient.beginMessage("sensor/ext");
  mqttClient.print("sensorExt tempExt=");
  mqttClient.print(String(tempExt));
  mqttClient.endMessage();

  mqttClient.beginMessage("sensor/ext");
  mqttClient.print("sensorExt relhumExt=");
  mqttClient.print(String(relhumExt));
  mqttClient.endMessage();

  mqttClient.beginMessage("sensor/setpoint");
  mqttClient.print("sensorSP tempSP=");
  mqttClient.print(String(tempSP));
  mqttClient.endMessage();

  mqttClient.beginMessage("sensor/setpoint");
  mqttClient.print("sensorSP relhumSP=");
  mqttClient.print(String(relhumSP));
  mqttClient.endMessage();

  mqttClient.beginMessage("sensor/setpoint");
  mqttClient.print("sensorMaxErr tempMaxErr=");
  mqttClient.print(String(tempSP + tempMaxErr));
  mqttClient.endMessage();

  mqttClient.beginMessage("sensor/setpoint");
  mqttClient.print("sensorMinErr tempMinErr=");
  mqttClient.print(String(tempSP - tempMinErr));
  mqttClient.endMessage();

  mqttClient.beginMessage("sensor/setpoint");
  mqttClient.print("sensorMaxErr relhumMaxErr=");
  mqttClient.print(String(relhumSP + relhumMaxErr));
  mqttClient.endMessage();

  mqttClient.beginMessage("sensor/setpoint");
  mqttClient.print("sensorMinErr relhumMinErr=");
  mqttClient.print(String(relhumSP - relhumMinErr));
  mqttClient.endMessage();

  if (debugMode) Serial.println(F("MQTT - Data published..."));
}

void InitializeSD() {
  if (!SD.begin(SD_CS)) {
    if (debugMode) Serial.println(F("SD - Card initialization failed!"));
    alarmInitializeSD = true;
  }
  else {
    if (debugMode) Serial.println(F("SD - Card initialized..."));
    alarmInitializeSD = false;
  }
}

void ConnectWiFi() {
  while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
    delay(5000);
  }
  if (debugMode) Serial.println(F("MQTT - WiFi connected..."));
}

void ConnectMQTT() {
  if (!mqttClient.connect(broker, port)) {
    if (debugMode) Serial.println(F("MQTT - MQTT broker connection failed..."));
    alarmConnectMQTT = true;
  }
  else {
    if (debugMode) Serial.println(F("MQTT - MQTT broker connected..."));
    alarmConnectMQTT = false;
  }
}

void GenerateHumidifierPulses() {
  if(!memStart) {
    memStart = true;
    memSeconds = currentSeconds;
  }
  
  if((currentSeconds-memSeconds) < 10) {
    digitalWrite(pinHumidifier, LOW);
  }
  else if ((currentSeconds-memSeconds) < 30) {
    digitalWrite(pinHumidifier, HIGH);
  }
  else {
    memStart = false;
    digitalWrite(pinHumidifier, HIGH);
  }
}

void setup() {
  if (debugMode) {
    Serial.begin(9600);
    while (!Serial) {
    }
  }

  if (debugMode) Serial.println(F("System - Set-up started..."));

  pinMode (pinDHT1, INPUT);
  pinMode (pinDHT2, INPUT);
  DHTInt.begin();
  DHTExt.begin();

  pinMode(pinRed, OUTPUT);
  digitalWrite(pinRed, LOW);
  pinMode(pinGreen, OUTPUT);
  digitalWrite(pinGreen, LOW);

  pinMode (pinPushButton, INPUT_PULLUP);
  pinMode (pinRotarySwitchChannelA, INPUT);
  pinMode (pinRotarySwitchChannelB, INPUT);
  aLastState = digitalRead(pinRotarySwitchChannelA);

  pinMode(pinCooler, OUTPUT);
  digitalWrite(pinCooler, HIGH);
  pinMode(pinHumidifier, OUTPUT);
  digitalWrite(pinHumidifier, HIGH);
  pinMode(pinVentilator, OUTPUT);
  digitalWrite(pinVentilator, HIGH);

  InitializeSD();
  RestoreSettings();
  memSettingsRestored = false;

  TFTscreen.initR(INITR_BLACKTAB);
  TFTscreen.setRotation(1);
  TFTscreen.fillScreen(ST7735_BLACK);

  if (mqttMode) {
    ConnectWiFi();
    ConnectMQTT();
  }

  if (debugMode) Serial.println(F("System - Set-up completed..."));
}

void loop() {
  tempPV = DHTInt.readTemperature();
  relhumPV = DHTInt.readHumidity();

  tempExt = DHTExt.readTemperature();
  relhumExt = DHTExt.readHumidity();

  if (isnan(tempPV) || isnan(relhumPV)) {
    alarmSensor = true;
  }
  else {
    alarmSensor = false;
  }

  currentSeconds = millis() / 1000;
  if (currentSeconds >= (indexLog * intervalLog)) {
    PublishMQTT();
    indexLog++;
  }

  // Control cooler, humidifier and ventilator
  if (alarmSensor) {
    runCooler = true;
    runHumidifier = false;
    runVentilator = false;
  }
  else {
  // + Cooler
    if (tempPV >= (tempSP + tempMaxErr)) {
      runCooler = true;
    }
    else if (tempPV <= (tempSP - tempMinErr)) {
      runCooler = false;
    }
  // + Humidifier
    if (relhumPV <= (relhumSP - relhumMinErr)) {
      runHumidifier = true;
    }
    else if (relhumPV >= (relhumSP + relhumMaxErr)) {
      runHumidifier = false;
    }
  // + Ventilator
    if (runHumidifier) {
      runVentilator = true;
    }
    else {
      runVentilator = false;
    }
  }

  if (runCooler) digitalWrite(pinCooler, LOW);
  else digitalWrite(pinCooler, HIGH);
  
  if (runHumidifier) GenerateHumidifierPulses();
  else digitalWrite(pinHumidifier, HIGH);
  
  if (runVentilator) digitalWrite(pinVentilator, LOW);
  else digitalWrite(pinVentilator, HIGH);
  
  // Rotary switch
  aState = digitalRead(pinRotarySwitchChannelA);
  if (aState != aLastState) {
    if (digitalRead(pinRotarySwitchChannelB) != aState) {
      if (debugMode) Serial.println(F("Debug - Rotary switch rotated clockwise..."));
      if (screenIndex == 0) {
        screenIndex = 1;
        screenSubIndex = 0;
        TFTscreen.fillScreen(ST7735_BLACK);
      }
      else if (screenIndex == 1) {
        if (screenSubIndex == 0) {
          screenIndex = 2;
          screenSubIndex = 0;
          TFTscreen.fillScreen(ST7735_BLACK);
        }
        else if (screenSubIndex == 1) tempSP += 0.5;
        else if (screenSubIndex == 2) tempMaxErr += 0.5;
        else if (screenSubIndex == 3) tempMinErr += 0.5;
      }
      else if (screenIndex == 2) {
        if (screenSubIndex == 0) {
          screenIndex = 3;
          screenSubIndex = 0;
          TFTscreen.fillScreen(ST7735_BLACK);
        }
        else if (screenSubIndex == 1) relhumSP += 0.5;
        else if (screenSubIndex == 2) relhumMaxErr += 0.5;
        else if (screenSubIndex == 3) relhumMinErr += 0.5;
      }
      else if (screenIndex == 3) {
        if (screenSubIndex == 0) {
          screenIndex = 0;
          screenSubIndex = 0;
          TFTscreen.fillScreen(ST7735_BLACK);
        }
        else if (screenSubIndex == 1) {
          SaveSettings();
        }
        else if (screenSubIndex == 2) {
          RestoreSettings();
        }
      }
    }
    else {
      if (debugMode) Serial.println(F("Debug - Rotary switch rotated counterclockwise..."));
      if (screenIndex == 0) {
        screenIndex = 3;
        screenSubIndex = 0;
        TFTscreen.fillScreen(ST7735_BLACK);
      }
      else if (screenIndex == 1) {
        if (screenSubIndex == 0) {
          screenIndex = 0;
          screenSubIndex = 0;
          TFTscreen.fillScreen(ST7735_BLACK);
        }
        else if (screenSubIndex == 1) tempSP -= 0.5;
        else if (screenSubIndex == 2) tempMaxErr -= 0.5;
        else if (screenSubIndex == 3) tempMinErr -= 0.5;
      }
      else if (screenIndex == 2) {
        if (screenSubIndex == 0) {
          screenIndex = 1;
          screenSubIndex = 0;
          TFTscreen.fillScreen(ST7735_BLACK);
        }
        if (screenSubIndex == 1) relhumSP -= 0.5;
        else if (screenSubIndex == 2) relhumMaxErr -= 0.5;
        else if (screenSubIndex == 3) relhumMinErr -= 0.5;
      }
      else if (screenIndex == 3) {
        if (screenSubIndex == 0) {
          screenIndex = 2;
          screenSubIndex = 0;
          TFTscreen.fillScreen(ST7735_BLACK);
        }
        else if (screenSubIndex == 1) {
          SaveSettings();
        }
        else if (screenSubIndex == 2) {
          RestoreSettings();
        }
      }
    }
  }
  aLastState = aState;

  // Pushbutton
  screenSubIndexChanged = false;
  if (!digitalRead(pinPushButton) && !memButton) {
    memButton = true;
    if (debugMode) Serial.println(F("Debug - Button pressed..."));

    if (screenIndex == 1  || screenIndex == 2) {
      screenSubIndex++;
      if (screenSubIndex > 3) screenSubIndex = 0;
    }

    if (screenIndex == 3) {
      screenSubIndex++;
      if (screenSubIndex > 2) screenSubIndex = 0;
    }

    screenSubIndexChanged = true;
  }

  if (digitalRead(pinPushButton) && memButton) {
    memButton = false;
  }

  // Display data
  // + Overview screen
  if (screenIndex == 0) {
    TFTscreen.setTextSize(2);
    TFTscreen.setTextColor(ST7735_BLACK);

    if (tempPV != tempOldPV) {
      dtostrf(tempOldPV, 4, 1, dummyText);
      TFTPrintText(45, 20, dummyText);
      tempOldPV = tempPV;
    }
    if (relhumPV != relhumOldPV) {
      dtostrf(relhumOldPV, 4, 1, dummyText);
      TFTPrintText(50, 60, dummyText);
      relhumOldPV = relhumPV;
    }

    TFTscreen.setTextSize(1);
    if (screenSubIndexChanged) {
      TFTscreen.setCursor(145, 5);
      TFTscreen.print(char(17));
      TFTscreen.print(char(16));
    }

    TFTscreen.setTextSize(2);
    TFTscreen.setTextColor(ST7735_WHITE);

    dtostrf(tempPV, 4, 1, dummyText);
    TFTPrintText(45, 20, dummyText);
    TFTscreen.print(char(247));
    TFTscreen.print("C");
    dtostrf(relhumPV, 4, 1, dummyText);
    TFTPrintText(50, 60, dummyText);
    TFTscreen.print("%");
    TFTscreen.setTextSize(1);
    if (screenSubIndex == 0) {
      TFTscreen.setCursor(145, 5);
      TFTscreen.print(char(17));
      TFTscreen.print(char(16));
    }
  }
  // + Temperature screen
  if (screenIndex == 1) {
    TFTscreen.setTextSize(1);
    TFTscreen.setTextColor(ST7735_BLACK);

    if (tempSP != tempOldSP  || screenSubIndexChanged) {
      dtostrf(tempOldSP, 4, 1, dummyText);
      TFTPrintText(65, 25, dummyText);
      tempOldSP = tempSP;
      if (screenSubIndexChanged) {
        TFTscreen.print(" ");
        TFTscreen.print(char(17));
      }
    }
    if (tempMaxErr != tempOldMaxErr || screenSubIndexChanged) {
      dtostrf(tempOldMaxErr, 4, 1, dummyText);
      TFTPrintText(65, 45, dummyText);
      tempOldMaxErr = tempMaxErr;
      if (screenSubIndexChanged) {
        TFTscreen.print(" ");
        TFTscreen.print(char(17));
      }
    }
    if (tempMinErr != tempOldMinErr || screenSubIndexChanged) {
      dtostrf(tempOldMinErr, 4, 1, dummyText);
      TFTPrintText(65, 65, dummyText);
      tempOldMinErr = tempMinErr;
      if (screenSubIndexChanged) {
        TFTscreen.print(" ");
        TFTscreen.print(char(17));
      }
    }
    if (tempPV != tempOldPV) {
      dtostrf(tempOldPV, 4, 1, dummyText);
      TFTPrintText(65, 85, dummyText);
      tempOldPV = tempPV;
    }
    if (tempExt != tempOldExt) {
      dtostrf(tempOldExt, 4, 1, dummyText);
      TFTPrintText(65, 105, dummyText);
      tempOldExt = tempExt;
    }
    if (screenSubIndexChanged) {
      TFTscreen.setCursor(145, 5);
      TFTscreen.print(char(17));
      TFTscreen.print(char(16));
    }

    TFTscreen.setTextSize(1);
    TFTscreen.setTextColor(ST7735_WHITE);

    TFTPrintText(5, 5, "Temperature (");
    TFTscreen.print(char(247));
    TFTscreen.print("C)");

    TFTPrintText(5, 25, "SP:");
    TFTPrintText(5, 45, "MaxErr:");
    TFTPrintText(5, 65, "MinErr:");
    TFTPrintText(5, 85, "PV:");
    TFTPrintText(5, 105, "Ext:");

    dtostrf(tempSP, 4, 1, dummyText);
    TFTPrintText(65, 25, dummyText);
    if (screenSubIndex == 1) {
      TFTscreen.print(" ");
      TFTscreen.print(char(17));
    }
    dtostrf(tempMaxErr, 4, 1, dummyText);
    TFTPrintText(65, 45, dummyText);
    if (screenSubIndex == 2) {
      TFTscreen.print(" ");
      TFTscreen.print(char(17));
    }
    dtostrf(tempMinErr, 4, 1, dummyText);
    TFTPrintText(65, 65, dummyText);
    if (screenSubIndex == 3) {
      TFTscreen.print(" ");
      TFTscreen.print(char(17));
    }
    dtostrf(tempPV, 4, 1, dummyText);
    TFTPrintText(65, 85, dummyText);
    dtostrf(tempExt, 4, 1, dummyText);
    TFTPrintText(65, 105, dummyText);
    if (screenSubIndex == 0) {
      TFTscreen.setCursor(145, 5);
      TFTscreen.print(char(17));
      TFTscreen.print(char(16));
    }
  }
  // + Relative Humidity screen
  if (screenIndex == 2) {
    TFTscreen.setTextSize(1);
    TFTscreen.setTextColor(ST7735_BLACK);

    if (relhumSP != relhumOldSP  || screenSubIndexChanged) {
      dtostrf(relhumOldSP, 4, 1, dummyText);
      TFTPrintText(65, 25, dummyText);
      relhumOldSP = relhumSP;
      if (screenSubIndexChanged) {
        TFTscreen.print(" ");
        TFTscreen.print(char(17));
      }
    }
    if (relhumMaxErr != relhumOldMaxErr || screenSubIndexChanged) {
      dtostrf(relhumOldMaxErr, 4, 1, dummyText);
      TFTPrintText(65, 45, dummyText);
      relhumOldMaxErr = relhumMaxErr;
      if (screenSubIndexChanged) {
        TFTscreen.print(" ");
        TFTscreen.print(char(17));
      }
    }
    if (relhumMinErr != relhumOldMinErr || screenSubIndexChanged) {
      dtostrf(relhumOldMinErr, 4, 1, dummyText);
      TFTPrintText(65, 65, dummyText);
      relhumOldMinErr = relhumMinErr;
      if (screenSubIndexChanged) {
        TFTscreen.print(" ");
        TFTscreen.print(char(17));
      }
    }
    if (relhumPV != relhumOldPV) {
      dtostrf(relhumOldPV, 4, 1, dummyText);
      TFTPrintText(65, 85, dummyText);
      relhumOldPV = relhumPV;
    }
    if (relhumExt != relhumOldExt) {
      dtostrf(relhumOldExt, 4, 1, dummyText);
      TFTPrintText(65, 105, dummyText);
      relhumOldExt = relhumExt;
    }
    if (screenSubIndexChanged) {
      TFTscreen.setCursor(145, 5);
      TFTscreen.print(char(17));
      TFTscreen.print(char(16));
    }

    TFTscreen.setTextSize(1);
    TFTscreen.setTextColor(ST7735_WHITE);

    TFTPrintText(5, 5, "Relative Humidity (%)");

    TFTPrintText(5, 25, "SP:");
    TFTPrintText(5, 45, "MaxErr:");
    TFTPrintText(5, 65, "MinErr:");
    TFTPrintText(5, 85, "PV:");
    TFTPrintText(5, 105, "Ext:");

    dtostrf(relhumSP, 4, 1, dummyText);
    TFTPrintText(65, 25, dummyText);
    if (screenSubIndex == 1) {
      TFTscreen.print(" ");
      TFTscreen.print(char(17));
    }
    dtostrf(relhumMaxErr, 4, 1, dummyText);
    TFTPrintText(65, 45, dummyText);
    if (screenSubIndex == 2) {
      TFTscreen.print(" ");
      TFTscreen.print(char(17));
    }
    dtostrf(relhumMinErr, 4, 1, dummyText);
    TFTPrintText(65, 65, dummyText);
    if (screenSubIndex == 3) {
      TFTscreen.print(" ");
      TFTscreen.print(char(17));
    }
    dtostrf(relhumPV, 4, 1, dummyText);
    TFTPrintText(65, 85, dummyText);
    dtostrf(relhumExt, 4, 1, dummyText);
    TFTPrintText(65, 105, dummyText);
    if (screenSubIndex == 0) {
      TFTscreen.setCursor(145, 5);
      TFTscreen.print(char(17));
      TFTscreen.print(char(16));
    }
  }
  // + Control systems screen
  if (screenIndex == 3) {
    TFTscreen.setTextSize(1);
    TFTscreen.setTextColor(ST7735_BLACK);

    if (runCooler != runCoolerOld) {
      if (runCooler) TFTPrintText(85, 25, "Running");
      else TFTPrintText(85, 25, "Off");
      runCoolerOld = runCooler;
    }
    if (runHumidifier != runHumidifierOld) {
      if (runHumidifier) TFTPrintText(85, 45, "Running");
      else TFTPrintText(85, 45, "Off");
      runHumidifierOld = runHumidifier;
    }
    if (screenSubIndexChanged || memSettingsSaved) {
      TFTPrintText(5, 90, "Save Settings?");
      TFTscreen.print(" ");
      TFTscreen.print(char(17));
      TFTPrintText(5, 90, "Settings Saved!");
      TFTscreen.print(" ");
      TFTscreen.print(char(17));
    }
    if (screenSubIndexChanged || memSettingsRestored) {
      TFTPrintText(5, 100, "Restore Settings?");
      TFTscreen.print(" ");
      TFTscreen.print(char(17));
      TFTPrintText(5, 100, "Settings Restored!");
      TFTscreen.print(" ");
      TFTscreen.print(char(17));
    }
    if (screenSubIndexChanged) {
      TFTscreen.setCursor(145, 5);
      TFTscreen.print(char(17));
      TFTscreen.print(char(16));
    }

    TFTscreen.setTextSize(1);
    TFTscreen.setTextColor(ST7735_WHITE);

    TFTPrintText(5, 5, "Control Systems");
    TFTPrintText(5, 25, "Cooler:");
    TFTPrintText(5, 45, "Humidifier");

    if (runCooler) TFTPrintText(85, 25, "Running");
    else TFTPrintText(85, 25, "Off");
    if (runHumidifier) TFTPrintText(85, 45, "Running");
    else TFTPrintText(85, 45, "Off");
    if (screenSubIndex == 0) {
      TFTscreen.setCursor(145, 5);
      TFTscreen.print(char(17));
      TFTscreen.print(char(16));
    }
    if (screenSubIndex == 1) {
      if (screenSubIndexChanged) {
        TFTPrintText(5, 90, "Save Settings?");
        TFTscreen.print(" ");
        TFTscreen.print(char(17));
      }
      else if (memSettingsSaved) {
        TFTPrintText(5, 90, "Settings Saved!");
        TFTscreen.print(" ");
        TFTscreen.print(char(17));
        memSettingsSaved = false;
      }
    }
    if (screenSubIndex == 2) {
      if (screenSubIndexChanged) {
        TFTPrintText(5, 100, "Restore Settings?");
        TFTscreen.print(" ");
        TFTscreen.print(char(17));
      }
      else if (memSettingsRestored) {
        TFTPrintText(5, 100, "Settings Restored!");
        TFTscreen.print(" ");
        TFTscreen.print(char(17));
        memSettingsRestored = false;
      }
    }
  }

  if (alarmInitializeSD || alarmConnectWiFi || alarmConnectMQTT || alarmRestoreSettings || alarmSaveSettings  || alarmSensor) {
    SetLED("red");
  }
  else {
    SetLED("green");
  }
}
