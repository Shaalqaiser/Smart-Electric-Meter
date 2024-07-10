

#define BLYNK_TEMPLATE_ID "TMPL6gIRYSySB"
#define BLYNK_TEMPLATE_NAME "Iot Smart Energy Meter"
#define BLYNK_AUTH_TOKEN "H0F_Vsamjid8jQmFp0B4C7QsNgW2PQG3"

#include <LoRa.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <BlynkSimpleEsp32.h>
#include <LiquidCrystal_I2C.h>

// Insert your network credentials
#define WIFI_SSID "ABCDEFGH"
#define WIFI_PASSWORD "12345678"

// Insert Authorized Email and Corresponding Password
#define USER_EMAIL "derwaishgroup1@gmail.com"
#define USER_PASSWORD "Saad@103"

#define LORA_SS_PIN 5
#define LORA_RST_PIN 14
#define LORA_DI0_PIN 2

// LoRa parameters
#define BAND 433E6 // Change this to your desired frequency band


#define BLYNK_PRINT Serial

StaticJsonDocument<200> receivedDataJsonObject;

LiquidCrystal_I2C lcd(0x27, 20, 4);  // Change the I2C address to match your LCD

float voltage = 0.00;
float current = 0.00;
float power = 0.00;
float energy = 0.00;
float frequency = 0.00;
float pf = 0.00;

String LoadState = "No Data"; 



char auth[] = BLYNK_AUTH_TOKEN;

char ssid[] = WIFI_SSID;
char pass[] = WIFI_PASSWORD;

BlynkTimer timer;

void setup() {
  Serial.begin(115200);



  // Setup LoRa pins
  LoRa.setPins(LORA_SS_PIN, LORA_RST_PIN, LORA_DI0_PIN);

  // Initialize LoRa
  if (!LoRa.begin(BAND)) {
    Serial.println("LoRa initialization failed. Check your connections.");
    while (1);
  }

  lcd.begin();
  lcd.backlight();
  lcd.clear();

  
  lcd.setCursor(0, 0);
  lcd.print("  Smart Energy  ");
  lcd.setCursor(0, 1);
  lcd.print("      Meter     ");

  delay(2000);

  Serial.print("Connecting to WiFi ..");
  lcd.setBacklight(255);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Wifi Connecting");
  lcd.setCursor(0, 1);
  lcd.print(".....");
  delay(1000);


  Blynk.begin(auth, ssid, pass);

  Serial.print("Wifi Connected");
  lcd.setBacklight(255);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Wifi Connected");
  delay(1000);

}

void loop() {
  receiveDataWithLoRa();
  SendDataToIot();
}

void SendDataToIot() {

  Blynk.virtualWrite(V0, voltage);
  Blynk.virtualWrite(V1, current);
  Blynk.virtualWrite(V2, power);
  Blynk.virtualWrite(V3, energy);
  Blynk.virtualWrite(V4, frequency);

}


void receiveDataWithLoRa() {
  // Check if a packet has been received
  if (LoRa.parsePacket()) {
    // Read the packet
    String receivedData = "";

    while (LoRa.available()) {
      receivedData += (char)LoRa.read();
    }

    Serial.println("Received data over LoRa: " + receivedData);

    // Parse received JSON data
    DeserializationError error = deserializeJson(receivedDataJsonObject, receivedData);
    if (error) {
      Serial.println("Failed to parse received JSON");
      return;
    }

    // Process received data as needed
    voltage = receivedDataJsonObject["VOLT"];
    current = receivedDataJsonObject["CURRENT"];
    power = receivedDataJsonObject["POWER"];
    energy = receivedDataJsonObject["ENERGY"];
    frequency = receivedDataJsonObject["FREQUENCY"];
    pf = receivedDataJsonObject["POWERFACTOR"];
    LoadState = String(receivedDataJsonObject["LOADSTATE"].as<const char*>());

    // Display received data
    displayReceivedData(voltage, current, power, energy, frequency, pf, LoadState);
  }
}

void displayReceivedData(float voltage, float current, float power, float energy, float frequency, float pf, String loadState) {
  // Display received data on LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Received Data:");
  lcd.setCursor(0, 1);
  lcd.print("Volt: " + String(voltage) + "V");
  lcd.setCursor(0, 2);
  lcd.print("Amp: " + String(current) + "A");
  lcd.setCursor(0, 3);
  lcd.print("Pwr: " + String(power) + "W");
  // Add more display lines for other parameters as needed

  // Print received data on Serial Monitor
  Serial.println("Received Data:");
  Serial.println("Voltage: " + String(voltage) + "V");
  Serial.println("Current: " + String(current) + "A");
  Serial.println("Power: " + String(power) + "W");
  Serial.println("Energy: " + String(energy) + "KWh");
  Serial.println("Frequency: " + String(frequency) + "Hz");
  Serial.println("Power factor: " + String(pf));
  Serial.println("Load State: " + LoadState);
  // Add more Serial prints for other parameters as needed
}
