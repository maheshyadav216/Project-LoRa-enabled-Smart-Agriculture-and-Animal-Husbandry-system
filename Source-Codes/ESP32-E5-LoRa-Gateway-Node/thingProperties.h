//==============================================================================================//
// Project - LoRa Enabled Smart Agriculture and Animal Husbadry System for remote rural areas.
// Author - (Hackster ID - maheshyadav216 ) : https://www.hackster.io/maheshyadav2162
// Contest - IoT into the Wild contest for sustainable Planet 2022
// Organizers - hackster.io and SEEED Studio
// GitHub Repo of Projet - https://github.com/maheshyadav216/Project-LoRa-enabled-Smart-Agriculture-and-Animal-Husbandry-system
// Code last Modified on - 27/09/2022
//===============================================================================================//
#include "arduino_secrets.h"
#include <ArduinoIoTCloud.h>
#include <Arduino_ConnectionHandler.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

static char recv_buf[512];
static bool is_exist = false;

#define RXD2 16
#define TXD2 17

const unsigned long updateInterval = 5000;
unsigned long previousUpdateTime = 0;

const char DEVICE_LOGIN_NAME[]  = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";

const char SSID[]               = SECRET_SSID;    // Network SSID (name)
const char PASS[]               = SECRET_PASS;    // Network password (use for WPA, or use as key for WEP)
const char DEVICE_KEY[]  = SECRET_DEVICE_KEY;    // Secret device password


float cattleshed_Humidity;
float cattleshed_Temperature;
float greenhouse_Humidity;
float greenhouse_Temperature;
int cattleshed_Light;
int greenhouse_CO2eq;
int greenhouse_Light;
int greenhouse_SoilMoisture;
int greenhouse_TVOC;
bool cattleshed_Bulbs;
bool cattleshed_Cooler;
bool cattleshed_ExFan;
bool cattleshed_Heater;
bool greenhouse_ExFan;
bool greenhouse_Pump;


// Function for sending AT commands and Checking Response
static int at_send_check_response(char *p_ack, int timeout_ms, char *p_cmd, ...)
{
    int ch = 0;
    int index = 0;
    int startMillis = 0;
    va_list args;
    memset(recv_buf, 0, sizeof(recv_buf));
    va_start(args, p_cmd);
    Serial2.printf(p_cmd, args);
    Serial.printf(p_cmd, args);
    va_end(args);
    delay(200);
    startMillis = millis();
 
    if (p_ack == NULL)
    {
        return 0;
    }
 
    do
    {
        while (Serial2.available() > 0)
        {
            ch = Serial2.read();
            recv_buf[index++] = ch;
            Serial.print((char)ch);
            delay(2);
        }
 
        if (strstr(recv_buf, p_ack) != NULL)
        {
            return 1;
        }
 
    } while (millis() - startMillis < timeout_ms);
    return 0;
}

// Function for Initilising OLED Display
void init_display() {
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  delay(2000);
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(10, 30);
  // Display static text
  display.println("IoT into the Wild!");
  display.display();
  delay(2000); 
}

// Function for Initilising Wio E5 LoRa Module
void setup_LoRa_receiver() {
  // Setting LoRa communication
  Serial.print("Init E5 LoRa Module!\r\n");
  if (at_send_check_response("+AT: OK", 100, "AT\r\n"))
  {
    is_exist = true;
    at_send_check_response("+MODE: TEST", 1500, "AT+MODE=TEST\r\n");
    at_send_check_response("+TEST: RFCFG", 1500, "AT+TEST=RFCFG,866,SF12,125,12,15,14,ON,OFF,OFF\r\n");
    delay(200);
    
    display.clearDisplay();
    display.setCursor(15, 10);
    display.println("Found E5 Module !");
    display.setCursor(15, 30);
    display.println("LoRa Setup : OK");
    display.setCursor(10, 50);
    display.println("Connecting to Cloud");
    display.display();
  }
  else
  {
    is_exist = false;
    Serial.print("No E5 module found.\r\n");
    
    display.clearDisplay();
    display.setCursor(10, 20);
    display.println("UnFound E5 Module !");
    display.setCursor(10, 45);
    display.println("LoRa Setup : NA");
    display.display();
  }
}

// Function for parsing the incomming data String
String getValue(String data, char separator, int index)
{
    int found = 0;
    int strIndex[] = { 0, -1 };
    int maxIndex = data.length() - 1;
 
    for (int i = 0; i <= maxIndex && found <= index; i++) {
        if (data.charAt(i) == separator || i == maxIndex) {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i+1 : i;
        }
    }
    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

// Function for Hex to ASCII conversion
byte aNibble(char in) {
  if (in >= '0' && in <= '9') {
    return in - '0';
  } else if (in >= 'a' && in <= 'f') {
    return in - 'a' + 10;
  } else if (in >= 'A' && in <= 'F') {
    return in - 'A' + 10;
  }
  return 0;
}

// Function for Hex to ASCII conversion
char * unHex(const char* input, char* target, size_t len) {
  if (target != nullptr && len) {
    size_t inLen = strlen(input);
    if (inLen & 1) {
      Serial.println(F("unhex: malformed input"));
    }
    size_t chars = inLen / 2;
    if (chars >= len) {
      Serial.println(F("unhex: target buffer too small"));
      chars = len - 1;
    }
    for (size_t i = 0; i < chars; i++) {
      target[i] = aNibble(*input++);
      target[i] <<= 4;
      target[i] |= aNibble(*input++);
    }
    target[chars] = 0;
  } else {
    Serial.println(F("unhex: no target buffer"));
  }
  return target;
}

void initProperties(){

  ArduinoCloud.setBoardId(DEVICE_LOGIN_NAME);
  ArduinoCloud.setSecretDeviceKey(DEVICE_KEY);
  ArduinoCloud.addProperty(cattleshed_Humidity, READ, 10 * SECONDS, NULL);
  ArduinoCloud.addProperty(cattleshed_Temperature, READ, 10 * SECONDS, NULL);
  ArduinoCloud.addProperty(greenhouse_Humidity, READ, 10 * SECONDS, NULL);
  ArduinoCloud.addProperty(greenhouse_Temperature, READ, 10 * SECONDS, NULL);
  ArduinoCloud.addProperty(cattleshed_Light, READ, ON_CHANGE, NULL);
  ArduinoCloud.addProperty(greenhouse_CO2eq, READ, ON_CHANGE, NULL);
  ArduinoCloud.addProperty(greenhouse_Light, READ, ON_CHANGE, NULL);
  ArduinoCloud.addProperty(greenhouse_SoilMoisture, READ, 2 * SECONDS, NULL);
  ArduinoCloud.addProperty(greenhouse_TVOC, READ, ON_CHANGE, NULL);
  ArduinoCloud.addProperty(cattleshed_Bulbs, READ, ON_CHANGE, NULL, 2);
  ArduinoCloud.addProperty(cattleshed_Cooler, READ, ON_CHANGE, NULL);
  ArduinoCloud.addProperty(cattleshed_ExFan, READ, ON_CHANGE, NULL, 2);
  ArduinoCloud.addProperty(cattleshed_Heater, READ, ON_CHANGE, NULL);
  ArduinoCloud.addProperty(greenhouse_ExFan, READ, ON_CHANGE, NULL, 2);
  ArduinoCloud.addProperty(greenhouse_Pump, READ, 2 * SECONDS, NULL);

}

WiFiConnectionHandler ArduinoIoTPreferredConnection(SSID, PASS);
