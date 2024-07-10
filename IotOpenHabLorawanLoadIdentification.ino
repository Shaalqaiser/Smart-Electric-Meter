//UUID = df9b8543-fb7f-4a7d-ae46-98c344aa95bc
//Secret = rS2ZbqGQxSxsjMgisQft

#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <HTTPClient.h>
#include <FS.h>
#include <PubSubClient.h>
#include <AutoConnect.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <PZEM004Tv30.h>
#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>

#define LORA_SS_PIN 5
#define LORA_RST_PIN 14
#define LORA_DI0_PIN 2

// LoRa parameters
#define BAND 433E6  // Change this to your desired frequency band


#define PZEM_RX_PIN 16
#define PZEM_TX_PIN 17
#define PZEM_SERIAL Serial2


#define PARAM_FILE "/param.json"
#define AUX_MQTTSETTING "/mqtt_setting"
#define AUX_MQTTSAVE "/mqtt_save"
#define AUX_MQTTCLEAR "/mqtt_clear"

// Adjusting WebServer class with between ESP8266 and ESP32.
typedef WebServer WiFiWebServer;

#define MQTT_USER_ID "no_one"


float voltage = 0.00;
float current = 0.00;
float power = 0.00;
float energy = 0.00;
float frequency = 0.00;
float pf = 0.00;

String LoadState = "NO Load Connected";

//Timing Intervals
int show = 0;
int displayUpdateInterval = 1;  // In seconds
unsigned long lastDisplayUpdateTime;

unsigned long lastDataSendingTime;
AutoConnect portal;
AutoConnectConfig config;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
unsigned long lastDataPublishTime;
unsigned long lastPowerUpdateTime;
// JSON Document variables for Energy Data
StaticJsonDocument<100> energyDataJsonObject;

// MQTT Configuration parameters
String clientId = "ARandom_client_id_with_123";

// Parameters red from the saved JSON file
String mqttBrokerIP;
String mqttBrokerPort;
String mqttUsername;
String mqttPassword;
String mqttDataPublishTopic;
int mqttDataPublishInterval;




PZEM004Tv30 pzem(PZEM_SERIAL, PZEM_RX_PIN, PZEM_TX_PIN);



LiquidCrystal_I2C lcd(0x27, 16, 2);  // Change the I2C address to match your LCD



// Previous power consumption variable
float previousPower = 0;

int Load1Status = 0;
int Load2Status = 0;
int Load3Status = 0;
int Load4Status = 0;

// Connect to the MQTT broker
bool mqttConnect() {
  uint8_t retry = 3;
  while (!mqttClient.connected()) {

    if (mqttBrokerIP.length() <= 0)
      break;

    mqttClient.setServer(mqttBrokerIP.c_str(), mqttBrokerPort.toInt());
    Serial.println(String("Attempting MQTT broker connection:") + mqttBrokerIP);

    if (mqttClient.connect(clientId.c_str(), mqttUsername.c_str(), mqttPassword.c_str())) {
      Serial.println("Connection to MQTT broker established:" + String(clientId));
      return true;
    } else {
      Serial.println("Connection to MQTT broker failed:" + String(mqttClient.state()));
      if (!--retry)
        break;
      delay(3000);
    }
  }
  return false;
}

// Publish the Energy Data
void mqttPublish() {
  String energyDataString;
  serializeJson(energyDataJsonObject, energyDataString);
  mqttClient.publish(mqttDataPublishTopic.c_str(), energyDataString.c_str());
}

// Save user entered MQTT params
String saveParams(AutoConnectAux& aux, PageArgument& args) {
  mqttBrokerIP = args.arg("mqtt_broker_url");
  mqttBrokerIP.trim();
  mqttBrokerPort = args.arg("mqtt_broker_port");
  mqttBrokerPort.trim();
  mqttUsername = args.arg("mqtt_username");
  mqttUsername.trim();
  mqttPassword = args.arg("mqtt_password");
  mqttPassword.trim();
  mqttDataPublishTopic = args.arg("mqtt_topic");
  mqttDataPublishTopic.trim();
  mqttDataPublishInterval = args.arg("update_interval").toInt() * 1000;

  // The entered value is owned by AutoConnectAux of /mqtt_setting.
  // To retrieve the elements of /mqtt_setting, it is necessary to get
  // the AutoConnectAux object of /mqtt_setting.
  File param = SPIFFS.open(PARAM_FILE, "w");
  portal.aux("/mqtt_setting")->saveElement(param, { "mqtt_broker_url", "mqtt_broker_port", "mqtt_username", "mqtt_password", "mqtt_topic", "update_interval" });
  param.close();

  //   Echo back saved parameters to AutoConnectAux page.
  AutoConnectText& echo = aux["parameters"].as<AutoConnectText>();
  echo.value = "Broker IP: " + mqttBrokerIP + "<br>";
  echo.value += "Port: " + mqttBrokerPort + "<br>";
  echo.value += "Username: " + mqttUsername + "<br>";
  echo.value += "Password: " + mqttPassword + "<br>";
  echo.value += "MQTT Topic: " + mqttDataPublishTopic + "<br>";
  echo.value += "Update Interval (In Seconds): " + String(mqttDataPublishInterval / 1000) + " sec.<br>";

  return String("");
}

String loadParams(AutoConnectAux& aux, PageArgument& args) {
  (void)(args);
  File param = SPIFFS.open(PARAM_FILE, "r");
  if (param) {
    aux.loadElement(param);
    param.close();
  } else
    Serial.println(PARAM_FILE " open failed");
  return String("");
}



void handleRoot() {
  String content =
    "<html>"
    "<head>"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "</head>"
    "<body>"
    "<p style=\"padding-top:10px;text-align:center\">" AUTOCONNECT_LINK(COG_24) "</p>"
                                                                                "</body>"
                                                                                "</html>";

  WiFiWebServer& webServer = portal.host();
  webServer.send(200, "text/html", content);
}

// Load AutoConnectAux JSON from SPIFFS.
bool loadAux(const String auxName) {
  bool rc = false;
  String fn = auxName + ".json";
  File fs = SPIFFS.open(fn.c_str(), "r");
  if (fs) {
    rc = portal.load(fs);
    fs.close();
  } else
    Serial.println("SPIFFS open failed: " + fn);
  return rc;
}

// Load MQTT Settings

void loadMQTTSettings() {
  AutoConnectAux* setting = portal.aux(AUX_MQTTSETTING);
  if (setting) {
    PageArgument args;
    AutoConnectAux& mqtt_setting = *setting;
    loadParams(mqtt_setting, args);

    AutoConnectInput& brokerIpElement = mqtt_setting["mqtt_broker_url"].as<AutoConnectInput>();
    mqttBrokerIP = brokerIpElement.value;

    AutoConnectInput& brokerPortElement = mqtt_setting["mqtt_broker_port"].as<AutoConnectInput>();
    mqttBrokerPort = brokerPortElement.value;

    AutoConnectInput& brokerUsernameElement = mqtt_setting["mqtt_username"].as<AutoConnectInput>();
    mqttUsername = brokerUsernameElement.value;

    AutoConnectInput& brokerPasswordElement = mqtt_setting["mqtt_password"].as<AutoConnectInput>();
    mqttPassword = brokerPasswordElement.value;

    AutoConnectInput& mqttTopicElement = mqtt_setting["mqtt_topic"].as<AutoConnectInput>();
    mqttDataPublishTopic = mqttTopicElement.value;

    AutoConnectInput& publishIntervalElement = mqtt_setting["update_interval"].as<AutoConnectInput>();
    mqttDataPublishInterval = publishIntervalElement.value.toInt() * 1000;

    config.homeUri = "/";
    portal.config(config);

    portal.on(AUX_MQTTSETTING, loadParams);
    portal.on(AUX_MQTTSAVE, saveParams);
  } else
    Serial.println("aux. load error");
}
void setup() {
  delay(1000);
  Serial.begin(115200);
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  Serial.println();
  SPIFFS.begin();

  loadAux(AUX_MQTTSETTING);
  loadAux(AUX_MQTTSAVE);

  loadMQTTSettings();

  Serial.print("WiFi ");
  if (portal.begin()) {
    config.bootUri = AC_ONBOOTURI_HOME;
    Serial.println("connected:" + WiFi.SSID());
    Serial.println("IP:" + WiFi.localIP().toString());
  } else {
    Serial.println("connection failed:" + String(WiFi.status()));
    while (1) {
      delay(100);
      yield();
    }
  }

  WiFiWebServer& webServer = portal.host();
  webServer.on("/", handleRoot);
  //  webServer.on(AUX_MQTTCLEAR, handleClearChannel);
  //Intialize the LCD

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("  Smart Energy  ");
  lcd.setCursor(0, 1);
  lcd.print("      Meter     ");


  delay(2000);

  String ip_ = WiFi.localIP().toString();
  char c[30];
  ip_.toCharArray(c, sizeof(c));
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(c);

  delay(2000);

  // Setup LoRa pins
  LoRa.setPins(LORA_SS_PIN, LORA_RST_PIN, LORA_DI0_PIN);

  // Initialize LoRa
  if (!LoRa.begin(BAND)) {
    Serial.println("LoRa initialization failed. Check your connections.");
    while (1)
      ;
  }
}

void loop() {


  if (mqttDataPublishInterval > 0) {
    if ((millis() - lastDataPublishTime) > mqttDataPublishInterval) {
      if (!mqttClient.connected()) {
        mqttConnect();
      }
      updateMeterData();

      mqttPublish();
      lastDataPublishTime = millis();
      if ((millis() - lastDataSendingTime) > 5000) {
        //updateMeterData();
        sendDataWithLoRa();
        lastDataSendingTime = millis();
      } else {
      }
    }
  } else {
    //  Serial.println("Invalid MQTT publish interval.");
  }



  if ((millis() - lastPowerUpdateTime) >= 3000) {
    lastPowerUpdateTime = millis();

  } else {
  }



  if ((millis() - lastDisplayUpdateTime) > (displayUpdateInterval * 1000)) {
    lastDisplayUpdateTime = millis();
    DisplayData();
  } else {
  }
  portal.handleClient();
  mqttClient.loop();
  // Serial.println(mqttClient.connected());
}


void DisplayData() {

  if (show == 0) {

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Voltage= ");
    lcd.print(voltage);
    lcd.print("V");
    lcd.setCursor(0, 1);
    lcd.print("Current= ");
    lcd.print(current);
    lcd.print("Amp");
  } else if (show == 1) {

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Power= ");
    lcd.print(power);
    lcd.print("W");
    lcd.setCursor(0, 1);
    lcd.print("Energy= ");
    lcd.print(energy);
    lcd.print("KWh");
  } else {
  }

  show = (show + 1) % 2;
}

// PZEM Module Communication related functions

void updateMeterData() {

  GetPzemData();
  energyDataJsonObject.clear();
  energyDataJsonObject["VOLT"] = voltage;
  energyDataJsonObject["CURRENT"] = current;
  energyDataJsonObject["POWER"] = power;
  energyDataJsonObject["ENERGY"] = energy;
  energyDataJsonObject["FREQUENCY"] = frequency;
  energyDataJsonObject["POWERFACTOR"] = pf;
  energyDataJsonObject["LOADSTATE"] = LoadState;
  serializeJsonPretty(energyDataJsonObject, Serial);
}




void GetPzemData() {

  Serial.print("Custom Address:");
  Serial.println(pzem.readAddress(), HEX);

  // Read the data from the sensor
  voltage = pzem.voltage();
  current = pzem.current();
  power = pzem.power();
  energy = pzem.energy();
  frequency = pzem.frequency();
  pf = pzem.pf();

  // Check if the data is valid
  if (isnan(voltage) || isnan(current) || isnan(power) || isnan(energy) || isnan(frequency) || isnan(pf)) {

    voltage = current = power = energy = frequency = pf = 0.0;
    Serial.println("Error readings");

  } else {
  }
  if (current <= 0.05) {
    current = 0.0;
  } else {
  }
  if (power <= 5.0) {
    power = 0.0;
  } else {
  }

  if (energy <= 0.009) {
    energy = 0.0;
  } else {
  }


  if (pf <= 0.1) {
    pf = 0.0;
  } else {
  }



  // Print the values to the Serial console
  Serial.print("Voltage: ");
  Serial.print(voltage);
  Serial.println("V");
  Serial.print("Current: ");
  Serial.print(current);
  Serial.println("A");
  Serial.print("Power: ");
  Serial.print(power);
  Serial.println("W");
  Serial.print("Energy: ");
  Serial.print(energy, 3);
  Serial.println("kWh");
  Serial.print("Frequency: ");
  Serial.print(frequency, 1);
  Serial.println("Hz");
  Serial.print("PF: ");
  Serial.println(pf);

  Serial.println();

  identifyLoadState(power);
}


void sendDataWithLoRa() {
  // Serialize JSON data
  String jsonData;
  serializeJson(energyDataJsonObject, jsonData);

  // Send data packet
  LoRa.beginPacket();
  LoRa.print(jsonData);
  LoRa.endPacket();

  Serial.println("Data sent over LoRa: " + jsonData);
}


void identifyLoadState(float PowerNow) {
  // Read power consumption from PZEM004TV
  float currentPower = PowerNow;

  // Calculate power difference
  float powerDifference = currentPower - previousPower;
  Serial.print("powerDifference = ");
  Serial.println(powerDifference);

  // Identify load based on power difference
  if (powerDifference > 18 && powerDifference < 30) {
    // Bulb load detected
    Load1Status += 1;
    Serial.println("Bulb connected");
  } else if (powerDifference < -18 && powerDifference > -30) {
    // Bulb disconnected
    Load1Status -= 1;
    if (Load1Status < 0) {
      Load1Status = 0;
    } else {
    }
    Serial.println("Bulb disconnected");
  }

  if (powerDifference > 65 && powerDifference < 130) {
    // Fan load detected
    Load2Status += 1;
    Serial.println("Fan connected");
  } else if (powerDifference < -65 && powerDifference > -130) {
    // Fan disconnected
    Load2Status -= 1;
    if (Load2Status < 0) {
      Load2Status = 0;
    } else {
    }
    Serial.println("Fan disconnected");
  }

  if (powerDifference > 150 && powerDifference < 250) {
    // Motor load detected
    Load3Status += 1;
    Serial.println("Motor connected");
  } else if (powerDifference < -150 && powerDifference > -250) {
    // Motor disconnected
    Load3Status -= 1;
    if (Load3Status < 0) {
      Load3Status = 0;
    } else {
    }
    Serial.println("Motor disconnected");
  }

  // Check for high-power load connection
  if (powerDifference > 300) {
    // High-power load connected
    Load4Status += 1;
    Serial.println("High-power load connected");
  } else if (powerDifference < -300) {
    // High-power load disconnected
    Load4Status -= 1;
    if (Load4Status < 0) {
      Load4Status = 0;
    } else {
    }
    Serial.println("High-power load disconnected");
  }
  if (PowerNow < 10) {
    Load1Status = 0;
    Load2Status = 0;
    Load3Status = 0;
    Load4Status = 0;
    LoadState = "";
    LoadState = "NO Load Connected";

  } else {
    LoadState = "";
    LoadState = "L1= ";
    LoadState += String(Load1Status);
    LoadState += " | L2= ";
    LoadState += String(Load2Status);
    LoadState += " | L3= ";
    LoadState += String(Load3Status);
    LoadState += " | L4= ";
    LoadState += String(Load4Status);
  }

  // Update previous power
  previousPower = currentPower;

  Serial.println();
  Serial.print("L1 = ");
  Serial.print(Load1Status);
  Serial.print(" | L2 = ");
  Serial.print(Load2Status);
  Serial.print(" | L3 = ");
  Serial.print(Load3Status);
  Serial.print(" | L4 = ");
  Serial.print(Load4Status);
  Serial.println();
  Serial.println(LoadState);
  Serial.println();
}