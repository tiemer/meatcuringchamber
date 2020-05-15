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

bool sdAlarm = false;
bool controlAlarm = false;
bool systemAlarm = false;
bool mqttAlarm = false;

const bool debugMode = false;
const bool mqttMode = true;
 
int currentSeconds = 0;

int indexLog = 1;
int intervalLog = 60;

char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

const char broker[] = "192.168.1.31";
int port = 1883;

bool memSettingsSaved = false;
bool memSettingsRestored = false;

void TFTPrintText(int wid, int hei, char *text) {
  TFTscreen.setCursor(wid, hei);
  TFTscreen.print(text);
}

void TFTPrintDoubleArrow(int wid, int hei) {
  TFTscreen.setCursor(wid, hei);
  TFTscreen.print(char(17));
  TFTscreen.print(char(16));
}

char dummyText[4];

float tempPV =  0.0;
float tempSP =  0.0;
float tempErr = 0.0;
float tempAvg = 0.0;
float tempMin = 0.0;
float tempMax = 0.0;
float tempExt = 0.0;

float tempOldPV =  0.0;
float tempOldSP =  0.0;
float tempOldErr = 0.0;
float tempOldAvg = 0.0;
float tempOldMin = 0.0;
float tempOldMax = 0.0;
float tempOldExt = 0.0;

float relhumPV = 0.0;
float relhumSP = 0.0;
float relhumErr = 0.0;
float relhumAvg = 0.0;
float relhumMin = 0.0;
float relhumMax = 0.0;
float relhumExt = 0.0;

float relhumOldPV = 0.0;
float relhumOldSP = 0.0;
float relhumOldErr = 0.0;
float relhumOldAvg = 0.0;
float relhumOldMin = 0.0;
float relhumOldMax = 0.0;
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
  if(SD.exists("settings.csv")) {
    File settingsFile = SD.open("settings.csv", FILE_READ);

    if(!settingsFile) {
      if (debugMode) Serial.println(F("SD - File for restoring settings not opened!"));
      sdAlarm = true;
    }
    else {
      int i = 0;
      char dummyChar;
      String dummyString[4] = {"","","",""};
      
      while (settingsFile.available()) {
        dummyChar = settingsFile.read();
        if(dummyChar != ';') {
          dummyString[i] += dummyChar;
        }
        else if(dummyChar == ';') {
          i++;
        }
      }
      settingsFile.close();
      
      tempSP = dummyString[0].toFloat();
      tempErr = dummyString[1].toFloat();
      relhumSP = dummyString[2].toFloat();
      relhumErr = dummyString[3].toFloat();

      if (debugMode) Serial.println(F("SD - Settings restored..."));
      memSettingsRestored = true;
    }
  }
  else {
    if (debugMode) Serial.println(F("SD - File for restoring settings not found!"));
    sdAlarm = true;
  }
}

void SaveSettings() {
  if(SD.exists("settings.csv")) {
    SD.remove("settings.csv");
  }
  
  File settingsFile = SD.open("settings.csv", FILE_WRITE);

  if(!settingsFile) {
    if (debugMode) Serial.println(F("SD - File for saving settings not opened!"));
    sdAlarm = true;
  }
  else {
    String settingsString = "";

    dtostrf(tempSP,4, 1, dummyText);
    settingsString += dummyText;
    settingsString += ";";
    dtostrf(tempErr,4, 1, dummyText);
    settingsString += dummyText;
    settingsString += ";";    
    dtostrf(relhumSP,4, 1, dummyText);
    settingsString += dummyText;
    settingsString += ";";
    dtostrf(relhumErr,4, 1, dummyText);
    settingsString += dummyText;

    settingsFile.println(settingsString);
    settingsFile.close();

    if (debugMode) Serial.println(F("SD - Settings saved...")); 
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

    if (debugMode) Serial.println(F("MQTT - Data published..."));
}

void setup() {
  if (debugMode) {
    Serial.begin(9600);
    while (!Serial) {
    }
  }
  
  if (debugMode) Serial.println(F("System - Set-up started..."));
  
  pinMode (pinDHT1,INPUT);
  pinMode (pinDHT2,INPUT);
  DHTInt.begin();
  DHTExt.begin();

  pinMode(pinRed, OUTPUT);
  digitalWrite(pinRed, LOW);
  pinMode(pinGreen, OUTPUT);
  digitalWrite(pinGreen, LOW);

  pinMode (pinPushButton,INPUT_PULLUP);
  pinMode (pinRotarySwitchChannelA,INPUT);
  pinMode (pinRotarySwitchChannelB,INPUT);
  aLastState = digitalRead(pinRotarySwitchChannelA);

  pinMode(pinCooler, OUTPUT);
  digitalWrite(pinCooler, HIGH);
  pinMode(pinHumidifier, OUTPUT);
  digitalWrite(pinHumidifier, HIGH);
  pinMode(pinVentilator, OUTPUT);
  digitalWrite(pinVentilator, HIGH);

  if(!SD.begin(SD_CS)) {
    if (debugMode) Serial.println(F("SD - Card initialization failed!"));
    sdAlarm = true;
  }
  else {
    if (debugMode) Serial.println(F("SD - Card initialized..."));
  }

  RestoreSettings();
  memSettingsRestored = false;

  TFTscreen.initR(INITR_BLACKTAB);
  TFTscreen.setRotation(1);
  TFTscreen.fillScreen(ST7735_BLACK);

  if (mqttMode) { 
    while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
      delay(5000);
    }
    if (debugMode) Serial.println(F("MQTT - WiFi connected..."));

    if (!mqttClient.connect(broker, port)) {
      if (debugMode) Serial.println(F("MQTT - MQTT broker connection failed..."));
      mqttAlarm = true;
    }
    else {
      if (debugMode) Serial.println(F("MQTT - MQTT broker connected..."));
    }
  }

  if (debugMode) Serial.println(F("System - Set-up completed..."));
}

void loop() {
  tempPV = DHTInt.readTemperature();
  relhumPV = DHTInt.readHumidity();

  tempExt = DHTExt.readTemperature();
  relhumExt = DHTExt.readHumidity();

  currentSeconds = millis() / 1000;
  if (currentSeconds >= (indexLog * intervalLog)) {
    PublishMQTT();
    indexLog++;
  }

// Control cooler, humidifier and ventilator
// + Cooler
  if(tempPV >= (tempSP + tempErr)) {
    runCooler = true;
    digitalWrite(pinCooler, LOW);
  }
  else if(tempPV <= (tempSP - tempErr)) {
    runCooler = false;
    digitalWrite(pinCooler, HIGH);
  }
// + Humidifier
  if(relhumPV <= (relhumSP - relhumErr)) {
    runHumidifier = true;
    digitalWrite(pinHumidifier, LOW);
  }
  else if(relhumPV >= (relhumSP + relhumErr)) {
    runHumidifier = false;
    digitalWrite(pinHumidifier, HIGH);
  }
// + Ventilator
  runVentilator = false;
  digitalWrite(pinVentilator, HIGH);

  aState = digitalRead(pinRotarySwitchChannelA);
  if (aState != aLastState){   
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
        else if (screenSubIndex == 2) tempErr += 0.5;
       }
       else if (screenIndex == 2) {
         if (screenSubIndex == 0) {
          screenIndex = 3;
          screenSubIndex = 0;
          TFTscreen.fillScreen(ST7735_BLACK);
         }
        else if (screenSubIndex == 1) relhumSP += 0.5;
        else if (screenSubIndex == 2) relhumErr += 0.5;
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
        else if (screenSubIndex == 2) tempErr -= 0.5;
       }
       else if (screenIndex == 2) {
        if (screenSubIndex == 0) {
          screenIndex = 1;
          screenSubIndex = 0;
          TFTscreen.fillScreen(ST7735_BLACK);
        }
        if (screenSubIndex == 1) relhumSP -= 0.5;
        else if (screenSubIndex == 2) relhumErr -= 0.5;
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

  screenSubIndexChanged = false;
  if (!digitalRead(pinPushButton) && !memButton) {
    memButton = true;
    if (debugMode) Serial.println(F("Debug - Button pressed..."));
    
    if (screenIndex == 1  || screenIndex == 2) {
      screenSubIndex++;
      if(screenSubIndex > 2) screenSubIndex = 0;
    }

    if (screenIndex == 3) {
      screenSubIndex++;
      if(screenSubIndex > 2) screenSubIndex = 0;
    }

    screenSubIndexChanged = true;
  }
    
  if (digitalRead(pinPushButton) && memButton) {
    memButton = false;
  }

// Display data
// + Overview screen
  if(screenIndex == 0) {
    TFTscreen.setTextSize(2);
    TFTscreen.setTextColor(ST7735_BLACK);
    
    if(tempPV != tempOldPV) {
      dtostrf(tempOldPV,4, 1, dummyText);
      TFTPrintText(45, 20, dummyText); 
      tempOldPV = tempPV;
    }
    if(relhumPV != relhumOldPV) {
      dtostrf(relhumOldPV,4, 1, dummyText);
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
    
    dtostrf(tempPV,4, 1, dummyText);
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
  if(screenIndex == 1) {
    TFTscreen.setTextSize(1);
    TFTscreen.setTextColor(ST7735_BLACK);

    if(tempSP != tempOldSP  || screenSubIndexChanged) {
      dtostrf(tempOldSP,4, 1, dummyText);
      TFTPrintText(35, 25, dummyText);
      tempOldSP = tempSP;
      if (screenSubIndexChanged) {
        TFTscreen.print(" ");
        TFTscreen.print(char(17));       
      }
    }
    if(tempErr != tempOldErr || screenSubIndexChanged) {
      dtostrf(tempOldErr,4, 1, dummyText);
      TFTPrintText(115, 25, dummyText);
      tempOldErr = tempErr;
      if (screenSubIndexChanged) {
        TFTscreen.print(" ");
        TFTscreen.print(char(17));        
      }
    }
    if(tempPV != tempOldPV) {
      dtostrf(tempOldPV,4, 1, dummyText);
      TFTPrintText(35, 45, dummyText);
      tempOldPV = tempPV;
    }
    if(tempAvg != tempOldAvg) {
      dtostrf(tempOldAvg,4, 1, dummyText);
      TFTPrintText(35, 65, dummyText);
      tempOldAvg = tempAvg;
    }
    if(tempMin != tempOldMin) { 
      dtostrf(tempOldMin,4, 1, dummyText);
      TFTPrintText(35, 85, dummyText);
      tempOldMin = tempMin;
    }
    if(tempMax != tempOldMax) {
      dtostrf(tempOldMax,4, 1, dummyText);
      TFTPrintText(115, 85, dummyText);
      tempOldMax = tempMax;
    }
    if(tempExt != tempOldExt) {
      dtostrf(tempOldExt,4, 1, dummyText);
      TFTPrintText(35, 105, dummyText);
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
    TFTPrintText(85, 25, "Err:");
    TFTPrintText(5, 45, "PV:");
    TFTPrintText(5, 65, "Avg:");
    TFTPrintText(5, 85, "Min:");
    TFTPrintText(85, 85, "Max:");
    TFTPrintText(5, 105, "Ext:");
  
    dtostrf(tempSP,4, 1, dummyText);
    TFTPrintText(35, 25, dummyText);
    if (screenSubIndex == 1) {
      TFTscreen.print(" ");
      TFTscreen.print(char(17));      
    }
    dtostrf(tempErr,4, 1, dummyText);
    TFTPrintText(115, 25, dummyText);
    if (screenSubIndex == 2) {
      TFTscreen.print(" ");
      TFTscreen.print(char(17));      
    }
    dtostrf(tempPV,4, 1, dummyText);
    TFTPrintText(35, 45, dummyText);
    dtostrf(tempAvg,4, 1, dummyText); 
    TFTPrintText(35, 65, dummyText);
    dtostrf(tempMin,4, 1, dummyText);
    TFTPrintText(35, 85, dummyText);
    dtostrf(tempMax,4, 1, dummyText);
    TFTPrintText(115, 85, dummyText);
    dtostrf(tempExt,4, 1, dummyText);
    TFTPrintText(35, 105, dummyText);
    if (screenSubIndex == 0) {
      TFTscreen.setCursor(145, 5);
      TFTscreen.print(char(17));
      TFTscreen.print(char(16));      
    }   
  }
// + Relative Humidity screen
  if(screenIndex == 2) {
    TFTscreen.setTextSize(1);
    TFTscreen.setTextColor(ST7735_BLACK);

    if(relhumSP != relhumOldSP  || screenSubIndexChanged) {
      dtostrf(relhumOldSP,4, 1, dummyText);
      TFTPrintText(35, 25, dummyText);
      relhumOldSP = relhumSP;
      if (screenSubIndexChanged) {
        TFTscreen.print(" ");
        TFTscreen.print(char(17));       
      }
    }
    if(relhumErr != relhumOldErr || screenSubIndexChanged) {
      dtostrf(relhumOldErr,4, 1, dummyText);
      TFTPrintText(115, 25, dummyText);
      relhumOldErr = relhumErr;
      if (screenSubIndexChanged) {
        TFTscreen.print(" ");
        TFTscreen.print(char(17));        
      }
    }
    if(relhumPV != relhumOldPV) {
      dtostrf(relhumOldPV,4, 1, dummyText);
      TFTPrintText(35, 45, dummyText);
      relhumOldPV = relhumPV;
    }
    if(relhumAvg != relhumOldAvg) {
      dtostrf(relhumOldAvg,4, 1, dummyText);
      TFTPrintText(35, 65, dummyText);
      relhumOldAvg = relhumAvg;
    }
    if(relhumMin != relhumOldMin) { 
      dtostrf(relhumOldMin,4, 1, dummyText);
      TFTPrintText(35, 85, dummyText);
      relhumOldMin = relhumMin;
    }
    if(relhumMax != relhumOldMax) {
      dtostrf(relhumOldMax,4, 1, dummyText);
      TFTPrintText(115, 85, dummyText);
      relhumOldMax = relhumMax;
    }
    if(relhumExt != relhumOldExt) {
      dtostrf(relhumOldExt,4, 1, dummyText);
      TFTPrintText(35, 105, dummyText);
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
    TFTPrintText(85, 25, "Err:");
    TFTPrintText(5, 45, "PV:");
    TFTPrintText(5, 65, "Avg:");
    TFTPrintText(5, 85, "Min:");
    TFTPrintText(85, 85, "Max:");
    TFTPrintText(5, 105, "Ext:");
  
    dtostrf(relhumSP,4, 1, dummyText);
    TFTPrintText(35, 25, dummyText);
    if (screenSubIndex == 1) {
      TFTscreen.print(" ");
      TFTscreen.print(char(17));      
    }
    dtostrf(relhumErr,4, 1, dummyText);
    TFTPrintText(115, 25, dummyText);
    if (screenSubIndex == 2) {
      TFTscreen.print(" ");
      TFTscreen.print(char(17));      
    }
    dtostrf(relhumPV,4, 1, dummyText);
    TFTPrintText(35, 45, dummyText);
    dtostrf(relhumAvg,4, 1, dummyText); 
    TFTPrintText(35, 65, dummyText);
    dtostrf(relhumMin,4, 1, dummyText);
    TFTPrintText(35, 85, dummyText);
    dtostrf(relhumMax,4, 1, dummyText);
    TFTPrintText(115, 85, dummyText);
    dtostrf(relhumExt,4, 1, dummyText);
    TFTPrintText(35, 105, dummyText );
    if (screenSubIndex == 0) {
      TFTscreen.setCursor(145, 5);
      TFTscreen.print(char(17));
      TFTscreen.print(char(16));      
    }   
  }
// + Control systems screen
  if(screenIndex == 3) {
    TFTscreen.setTextSize(1);
    TFTscreen.setTextColor(ST7735_BLACK);

    if(runCooler != runCoolerOld) {
      if(runCooler) TFTPrintText(85, 25, "Running");
      else TFTPrintText(85, 25, "Off");
      runCoolerOld = runCooler;
    }
    if(runHumidifier != runHumidifierOld) {
      if(runCooler) TFTPrintText(85, 45, "Running");
      else TFTPrintText(85, 45, "Off");
      runHumidifierOld = runHumidifier;
    }
    if(runVentilator != runVentilatorOld) {
      if(runVentilator) TFTPrintText(85, 65, "Running");
      else TFTPrintText(85, 65, "Off");
      runVentilatorOld = runVentilator;
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
    TFTPrintText(5, 65, "Ventilator:");

    if(runCooler) TFTPrintText(85, 25, "Running");
    else TFTPrintText(85, 25, "Off");
    if(runHumidifier) TFTPrintText(85, 45, "Running");
    else TFTPrintText(85, 45, "Off");
    if(runVentilator) TFTPrintText(85, 65, "Running");
    else TFTPrintText(85, 65, "Off");
    if (screenSubIndex == 0) {
      TFTscreen.setCursor(145, 5);
      TFTscreen.print(char(17));
      TFTscreen.print(char(16));      
    }
    if (screenSubIndex == 1) {
      if(screenSubIndexChanged) {
        TFTPrintText(5, 90, "Save Settings?");
        TFTscreen.print(" ");
        TFTscreen.print(char(17));
      }
      else if(memSettingsSaved) {
        TFTPrintText(5, 90, "Settings Saved!");
        TFTscreen.print(" ");
        TFTscreen.print(char(17));
        memSettingsSaved = false;
      }
    }
    if (screenSubIndex == 2) {
      if(screenSubIndexChanged) {
        TFTPrintText(5, 100, "Restore Settings?");
        TFTscreen.print(" ");
        TFTscreen.print(char(17));
      }
      else if(memSettingsRestored) {
        TFTPrintText(5, 100, "Settings Restored!");
        TFTscreen.print(" ");
        TFTscreen.print(char(17));
        memSettingsRestored = false;
      }
    }     
  }

  if(systemAlarm || sdAlarm || controlAlarm) {
    digitalWrite(pinRed, HIGH);
    digitalWrite(pinGreen, LOW);
  }
  else {
    digitalWrite(pinRed, LOW);
    digitalWrite(pinGreen, HIGH);
  }
}

//void LogData() {
//  File dataFile = SD.open("data.csv", FILE_WRITE);
//
//  if(!dataFile) {
//    if (debugMode) Serial.println(F("SD - File for logging data not opened!"));
//    sdAlarm = true;
//  }
//  else {
//    String dataString = "";
//
//    dataString += String(indexLogFile);
//    dataString +=";";   
//    dtostrf(tempSP,4, 1, dummyText);
//    dataString += dummyText;
//    dataString += ";";
//    dtostrf(tempErr,4, 1, dummyText);
//    dataString += dummyText;
//    dataString += ";";
//    dtostrf(tempPV,4, 1, dummyText);
//    dataString += dummyText;
//    dataString += ";";
//    dtostrf(tempExt,4, 1, dummyText);
//    dataString += dummyText;
//    dataString += ";";     
//    dtostrf(relhumSP,4, 1, dummyText);
//    dataString += dummyText;
//    dataString += ";";
//    dtostrf(relhumErr,4, 1, dummyText);
//    dataString += dummyText;
//    dataString += ";";
//    dtostrf(relhumPV,4, 1, dummyText);
//    dataString += dummyText;
//    dataString += ";";
//    dtostrf(relhumExt,4, 1, dummyText);
//    dataString += dummyText;
//
//    dataFile.println(dataString);
//    dataFile.close(); 
//
//    if (debugMode) Serial.println(F("SD - Data logged..."));
//  }
//}
