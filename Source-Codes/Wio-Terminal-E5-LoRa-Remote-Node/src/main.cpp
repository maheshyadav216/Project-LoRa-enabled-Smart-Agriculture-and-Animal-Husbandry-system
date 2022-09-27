//==============================================================================================//
// Project - LoRa Enabled Smart Agriculture and Animal Husbadry System for remote rural areas.
// Author - (Hackster ID - maheshyadav216 ) : https://www.hackster.io/maheshyadav2162
// Contest - IoT into the Wild contest for sustainable Planet 2022
// Organizers - hackster.io and SEEED Studio
// GitHub Repo of Projet - https://github.com/maheshyadav216/Project-LoRa-enabled-Smart-Agriculture-and-Animal-Husbandry-system
// Code last Modified on - 27/09/2022
//===============================================================================================//

// This is LoRa Sender/Remote Node code, which sends LoRa data from remote Agriculture Field to nearest LoRa Gateway 
// which will be well connected to Internet and cloud services.

#include <Arduino.h>
#include <SoftwareSerial.h>
#include "TFT_eSPI.h" //include TFT LCD library 
#include "Free_Fonts.h" //include free fonts library 
#include "Seeed_FS.h" //Including SD card library
#include "RawImage.h"  //Including image processing library
#include <Wire.h>
#include "sensirion_common.h" //include sensirion SGP30 library 
#include "sgp30.h"
#include <SensirionI2CSht4x.h> // include sensirion SHT30 Temp sensor lib

SoftwareSerial e5(0, 1); 

u8 soilSensor = A7;
SensirionI2CSht4x sht4x;

float greenhouse_Temperature;
float greenhouse_Humidity;
u16 greenhouse_CO2eq;
u16 greenhouse_TVOC;
u8 greenhouse_Light;
u8 greenhouse_SoilMoisture;
bool greenhouse_ExFan;
bool greenhouse_Pump;

float cattleshed_Temperature;
float cattleshed_Humidity;
u8 cattleshed_Light;
bool cattleshed_Bulbs;
bool cattleshed_ExFan;
bool cattleshed_Cooler;
bool cattleshed_Heater;

String GH_ex_fan_stat;
String CS_ex_fan_stat;
String pump_stat;
String heater_stat;
String cooler_stat;
String bulbs_stat;

u8 pump = D2;
u8 ex_fan = D3;
u8 heater = D4;
u8 cooler = D5;
u8 bulbs = D6;


const unsigned long sendInterval = 20000;
const unsigned long updateInterval = 5000;

unsigned long previousTime = 0;
unsigned long previousUpdateTime = 0;

static char recv_buf[512];
static bool is_exist = false;

TFT_eSPI tft; //initialize TFT LCD

// Function for sending AT commands and Checking Response
static int at_send_check_response(char *p_ack, int timeout_ms, char *p_cmd, ...)
{
  int ch = 0;
  int index = 0;
  int startMillis = 0;
  va_list args;
  memset(recv_buf, 0, sizeof(recv_buf));
  va_start(args, p_cmd);
  e5.printf(p_cmd, args);
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
      while (e5.available() > 0)
      {
          ch = e5.read();
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

// Function for Initialising Wio E5 LoRa module
void init_E5_module(){
  e5.begin(9600);
  if (at_send_check_response("+AT: OK", 100, "AT\r\n"))
  {
      is_exist = true;
      at_send_check_response("+MODE: TEST", 1000, "AT+MODE=TEST\r\n");
      at_send_check_response("+TEST: RFCFG", 1000, "AT+TEST=RFCFG,866,SF12,125,12,15,14,ON,OFF,OFF\r\n");
      delay(200);
  }
  else
  {
      is_exist = false;
      Serial.print("No E5 module found.\r\n");
  }
}

// Function for Initialising GPIO's
void gpioSetup(){
  pinMode(WIO_5S_UP, INPUT_PULLUP);
  pinMode(WIO_5S_PRESS, INPUT_PULLUP);
  pinMode(WIO_LIGHT, INPUT);

  pinMode(pump, OUTPUT);
  pinMode(ex_fan, OUTPUT);
  pinMode(heater, OUTPUT);
  pinMode(cooler, OUTPUT);
  pinMode(bulbs, OUTPUT);

  digitalWrite(pump, HIGH);
  digitalWrite(ex_fan, HIGH);
  digitalWrite(heater, HIGH);
  digitalWrite(cooler, HIGH);
  digitalWrite(bulbs, HIGH);
}

// Function for Initialising Gas Sensor
void initCo2Sensor() {
  s16 err;
  //u32 ah = 0;
  u16 scaled_ethanol_signal, scaled_h2_signal;
  /*  Init module,Reset all baseline,The initialization takes up to around 15 seconds, during which
  all APIs measuring IAQ(Indoor air quality ) output will not change.Default value is 400(ppm) for co2,0(ppb) for tvoc*/
  while (sgp_probe() != STATUS_OK) {
    Serial.println("SGP failed");
    while (1);
  }

  /*Read H2 and Ethanol signal in the way of blocking*/
  err = sgp_measure_signals_blocking_read(&scaled_ethanol_signal,

                                        &scaled_h2_signal);

  if (err == STATUS_OK) {
    //Serial.println("get ram signal!");
  } else {
    Serial.println("error reading signals");
  }

  // Set absolute humidity to 13.000 g/m^3
  //It's just a test value
  sgp_set_absolute_humidity(13000);
  err = sgp_iaq_init();
}

// Function for Initialising Temp and Humidity Sensor
void initTempSensor() {
  Wire.begin();
  uint16_t error;
  char errorMessage[256];
  sht4x.begin(Wire);
  uint32_t serialNumber;
  error = sht4x.serialNumber(serialNumber);
  if (error) {
    Serial.print("Error trying to execute serialNumber(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  } else {
    //Serial.print("Serial Number: ");
    //Serial.println(serialNumber);
  }  
}

// Function for checking all parameters/sensors of Greenhouse at a time
void checkGreenHouse() {
  // lets check all the parameters of greenhouse like temperature, humidity etc...
  // Temperature and Humidity
  uint16_t error;
  char errorMessage[256];
  error = sht4x.measureHighPrecision(greenhouse_Temperature, greenhouse_Humidity);
  if (error) {
    Serial.print("Error trying to execute measureHighPrecision(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  } else {
    /*Serial.print("Greenhouse Temperature: ");
    Serial.print(greenhouse_Temperature);
    Serial.println(" °C");
    Serial.print("Greenhouse Humidity: ");
    Serial.print(greenhouse_Humidity);
    Serial.println(" %");
    Serial.println("");*/
  }

  if((greenhouse_Temperature >= 40) || (greenhouse_Humidity >= 65)){
    digitalWrite(ex_fan, LOW);
    greenhouse_ExFan = 1;
    GH_ex_fan_stat = "ON";
  }else if ((greenhouse_Temperature <= 30) || (greenhouse_Humidity <= 50)){
    digitalWrite(ex_fan, HIGH);
    greenhouse_ExFan = 0;
    GH_ex_fan_stat = "OFF";
  }

  // Air Quality - CO2eq and TVOC
  s16 err = 0;
  err = sgp_measure_iaq_blocking_read(&greenhouse_TVOC, &greenhouse_CO2eq);
  if (err == STATUS_OK) {
    /*Serial.print("Greenhouse tVOC  Concentration: ");
    Serial.print(greenhouse_TVOC);
    Serial.println(" ppb");
    Serial.print("Greenhouse CO2eq Concentration: ");
    Serial.print(greenhouse_CO2eq);
    Serial.println(" ppm");*/
    } else {
      Serial.println("error reading IAQ values\n");
  }

  // Soil Moisture percentage in greenhouse
  u16 soilAnalogValue = analogRead(soilSensor);
  greenhouse_SoilMoisture = map(soilAnalogValue, 0, 1024, 0, 100);
  /*Serial.print("Greenhouse Soil Moisture level: ");
  Serial.print(greenhouse_SoilMoisture);
  Serial.println(" %");*/

  if(greenhouse_SoilMoisture <= 25){
    digitalWrite(pump, LOW);
    greenhouse_Pump = 1;
    pump_stat = "ON";
  }
  else if(greenhouse_SoilMoisture >=60){
    digitalWrite(pump, HIGH);
    greenhouse_Pump = 0;
    pump_stat = "OFF";
  }

  // Light (Illuminance) level/percentage in greenhouse
  u16 LightAnalogValue = analogRead(WIO_LIGHT);  
  greenhouse_Light = map(LightAnalogValue, 0, 1024, 0, 100);
  /*Serial.print("Greehouse Light level: ");
  Serial.print(greenhouse_Light);
  Serial.println(" %");*/
}


// Function for checking all parameters/sensors of Cattle Shed and Husbandry system at a time
void checkCattleShed() {
  // lets check all the parameters of Cattle Shed like temperature, humidity etc...
  // Temperature and Humidity
  uint16_t error;
  char errorMessage[256];
  error = sht4x.measureHighPrecision(cattleshed_Temperature, cattleshed_Humidity);
  if (error) {
    Serial.print("Error trying to execute measureHighPrecision(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  } else {
    /*Serial.print("Cattle shed Temperature: ");
    Serial.print(cattleshed_Temperature);
    Serial.println(" °C");
    Serial.print("Cattle shed Humidity: ");
    Serial.print(cattleshed_Humidity);
    Serial.println(" %");
    Serial.println("");*/
  }

  if(cattleshed_Humidity >= 65){
    digitalWrite(ex_fan, LOW);
    cattleshed_ExFan = 1;
    CS_ex_fan_stat = "ON";
  }else if (cattleshed_Humidity <= 50){
    digitalWrite(ex_fan, HIGH);
    cattleshed_ExFan = 0;
    CS_ex_fan_stat = "OFF";
  }

  if(cattleshed_Temperature >= 40){
    digitalWrite(cooler, LOW);
    cattleshed_Cooler = 1;
    cooler_stat= "ON";
  }else if (cattleshed_Temperature <= 30){
    digitalWrite(cooler, HIGH);
    cattleshed_Cooler = 0;
    cooler_stat = "OFF";
  }

  if(cattleshed_Temperature <= 20){
    digitalWrite(heater, LOW);
    cattleshed_Heater = 1;
    heater_stat= "ON";
  }else if (cattleshed_Temperature >= 25){
    digitalWrite(heater, HIGH);
    cattleshed_Heater = 0;
    heater_stat = "OFF";
  }

  // Light (Illuminance) level/percentage in Cattle shed
  u16 LightAnalogValue = analogRead(WIO_LIGHT);  
  cattleshed_Light = map(LightAnalogValue, 0, 1024, 0, 100);
  /*Serial.print("Cattle Shed Light level: ");
  Serial.print(cattleshed_Light);
  Serial.println(" %");*/

  if(cattleshed_Light <= 30){
    digitalWrite(bulbs, LOW);
    cattleshed_Bulbs = 1;
    bulbs_stat = "ON";
  }else if (cattleshed_Light >= 35){
    digitalWrite(bulbs, HIGH);
    cattleshed_Bulbs = 0;
    bulbs_stat = "OFF";
  }

}

// Function for Displaying Home Screen on Wio Terminal
void HomeScreen(){
  drawImage<uint8_t>("WioTerminal-screen-1.bmp", 0, 0); //Display this 8-bit image in sd card from (0, 0)
  tft.setTextColor(TFT_BLACK);
  tft.setFreeFont(&FreeSerifBold9pt7b); //set font type 
  tft.drawFloat(greenhouse_Temperature,2,83,69); //draw text string 
  tft.drawFloat(greenhouse_Humidity,2,83,114); //draw text string 
  tft.drawNumber(greenhouse_SoilMoisture,83,158); //draw text string 
  tft.drawNumber(greenhouse_Light,83,207); //draw text string 
  tft.drawNumber(greenhouse_CO2eq,221,68); //draw text string 
  tft.drawNumber(greenhouse_TVOC,221,112); //draw text string 
  tft.drawString(pump_stat,265,160); //draw text string 
  tft.drawString(GH_ex_fan_stat,265,205); //draw text string 
}


// Function for Initilising Wio Terminal Display
void initDisplay() {
  //Initialise SD card
  if (!SD.begin(SDCARD_SS_PIN, SDCARD_SPI)) {
    while (1);
  }
  tft.begin(); //start TFT LCD 
  tft.setRotation(1); //set screen rotation 
}


// Function for Displaying Second Screen (Cattleshed Data) on Wio Terminal
void secondScreen(){
  drawImage<uint8_t>("WioTerminal-screen-2.bmp", 0, 0); //Display this 8-bit image in sd card from (0, 0)
  tft.setTextColor(TFT_BLACK);
  tft.setFreeFont(&FreeSerifBold9pt7b); //set font type 
  tft.drawFloat(cattleshed_Temperature,2,83,70); //draw text string 
  tft.drawFloat(cattleshed_Humidity,2,83,117); //draw text string 
  tft.drawNumber(cattleshed_Light,83,162); //draw text string 
  tft.drawString("NAN",83,208); //draw text string 
  tft.drawString(cooler_stat,268,66); //draw text string 
  tft.drawString(heater_stat,268,114); //draw text string 
  tft.drawString(CS_ex_fan_stat,268,164); //draw text string 
  tft.drawString(bulbs_stat,268,209); //draw text string 
}


// Function for LoRa packet preparation and sending
static int LoRa_send()
{
  String sensorData = "";
  char cmd[256] = "";
  char data[128] = "";
  int ret = 0;
  sensorData = sensorData + String(greenhouse_Temperature) + "," + String(greenhouse_Humidity) + "," + String(greenhouse_SoilMoisture) + "," + String(greenhouse_Light) 
                + "," + String(greenhouse_CO2eq) + "," + String(greenhouse_TVOC) + "," + String(greenhouse_Pump) + "," + String(greenhouse_ExFan)
                + ","+ String(cattleshed_Light) + "," + String(cattleshed_ExFan) + "," + String(cattleshed_Heater) + "," + String(cattleshed_Cooler) + "," + String(cattleshed_Bulbs);
  //Serial.print("Printing Sensor Data String : ");
  //Serial.println(sensorData);

  strncpy(data,sensorData.c_str(),sizeof(data));
  data[sizeof(data) -1] = 0;

  sprintf(cmd, "AT+TEST=TXLRSTR,\"my216,%s\"\r\n", data);
  //Serial.print("Printing cmd String : ");
  //Serial.print(cmd);

  ret = at_send_check_response("TX DONE", 5000, cmd);
    if (ret == 1)
    {
      Serial.println("");
      Serial.print("Sent successfully!\r\n");
    }
    else
    {
      Serial.println("");
      Serial.print("Send failed!\r\n");
    }
  return ret;
}


void setup() {
  Serial.begin(9600);
  init_E5_module();
  delay(5000);
  initDisplay();
  gpioSetup();

  initCo2Sensor();
  initTempSensor();
  delay(2500);
}

void loop() {
  unsigned long currentUpdateTime = millis();
  if (currentUpdateTime - previousUpdateTime >= updateInterval) {
    checkGreenHouse();
    checkCattleShed();
    HomeScreen();
    previousUpdateTime = currentUpdateTime;
  }

  unsigned long currentTime = millis();
  if (currentTime - previousTime >= sendInterval) {
    LoRa_send();
    previousTime = currentTime;
  }

  if (digitalRead(WIO_5S_PRESS) == LOW) {
    secondScreen();
    delay(2000);
  }
}