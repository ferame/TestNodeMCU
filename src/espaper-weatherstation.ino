/**The MIT License (MIT)
Copyright (c) 2017 by Daniel Eichhorn
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
See more at https://blog.squix.org
  Copyright (c) 2017 by Daniel Eichhorn
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
*/
/*****************************
   Important: see settings.h to configure your settings!!!
 * ***************************/
#include "settings.h"

#include <Arduino.h>
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include "WeatherStationFonts.h"

/***
   Install the following libraries through Arduino Library Manager
   - Mini Grafx by Daniel Eichhorn
   - ESP8266 WeatherStation by Daniel Eichhorn
   - Json Streaming Parser by Daniel Eichhorn
   - simpleDSTadjust by neptune2
 ***/ //nodemcu Bluetooth detect beacon, qr codes scanning with rpi, documenting api requirements, summary of device logic., 13:00 lt laiku
// NodeMCU does not have an internal bluetooth module. As a result, an external module, such as HC-05 would be required (7 pounds).
// QR codes scanning with Raspberry PI - https://slackhacker.com/2016/09/12/effective-qr-scanning-with-the-raspberry-pi-and-camera-module/
//  Any camera module is suitable, as long as the focus distance is optimal.
//  Processing of the picture captured is done by using Z-Bar libraries and tools. Possible issue - not as quick response as the industry offered alternatives.
// Android library generating QR code - https://github.com/kenglxn/QRGen - Generating some secret key every time for Authentication, in combination with usual credentials.
// iOS library generating QR code - https://github.com/aschuch/QRCode
//
#include <JsonListener.h>
#include <OpenWeatherMapCurrent.h>
#include <OpenWeatherMapForecast.h>
#include <Astronomy.h>
#include <MiniGrafx.h>
#include <EPD_WaveShare.h>

#include "ArialRounded.h"
#include <MiniGrafxFonts.h>
#include "moonphases.h"
#include "weathericons.h"
#include "twitter.h"
#include "configportal.h"

#include <MFRC522.h>

#define RST_PIN         D1          // Configurable, see typical pin layout above
#define SS_PIN          D8         // Configurable, see typical pin layout above

MFRC522 rfid(SS_PIN, RST_PIN);  // Create MFRC522 instance
byte nuidPICC[4];

#define MINI_BLACK 0
#define MINI_WHITE 1


#define MAX_FORECASTS 20

// defines the colors usable in the paletted 16 color frame buffer
uint16_t palette[] = {ILI9341_BLACK, // 0
                      ILI9341_WHITE, // 1
                     };

#define SCREEN_HEIGHT 128
#define SCREEN_WIDTH 296
#define BITS_PER_PIXEL 1

//Interface time variables
unsigned long previousMillis = 0;
unsigned long currentMillis;
const unsigned long interval = 50000;
//Button variables
int currently_selected_button = 1;
int currently_selected_layer = 0;
int buttonState1 = 0;
int buttonState2 = 0;

// 0 - Base layer (Notify me, Contact info, Refresh)
// 1 - Notify me layer (Anonymous, Cancel, With Uniid)
// 2 - Motify me 2 Layer (Notification that alert was sent - Ok (timeout if not pressed, if pressed go back to Base Layer))
// 3 - Nofify me 3 Layer (Retry, Cancel)
// 4 - Notify me 4 Layer (Notification that alert was sent, with name and surname of sender mentioned - Ok (timeout if not pressed, if pressed go back to Base Layer))
// 5 - Contact info Layer (Notify Me / Show QR code / Cancel) (Show contact details of owner, along with QR code maybe as another layer)
// 6 - Contact info 2 Layer (Done (brings to 5), Cancel (brings to 0))

//New layer
// 0 - Base Layer 0 (Notify Me, Contact Info, Refresh)
// 1 - Notify me Layer 1 (Anonymously, With UniID, Cancel)
// 2 - Contact Info Layer 1 (QR Code, Done, - )
// 3 - Anonymously "Success" Layer 2 ( - , Main menu, - )
// 4 - Anonymously "Failure" Layer 2 (Retry, Main menu, - )
// 5 - With UniID Layer 2 ( - , Main menu, - )
// 6 - QR Code Layer 2 (Back, Main menu, - )
// 7 - Card Tapped "Success" Layer 3 ( - , Main menu, - )
// 8 - Card Tapped "Failure" Layer 3 (Retry, Main menu, - )

char *notify_me = "NOTIFY ME";
char *contact_info = "CONTACT INFO";
char *refresh = "REFRESH";
char *anonymous = "WITHOUT UNIid";
char *unanonymous = "WITH UNIid";
char *cancel = "CANCEL";
char *done = "Done";
char *nothing = "";
char *retry = "RETRY";
char *show_qr_code = "SHOW QR CODE";

const char *buttons_strs [7][3] = {
  {notify_me,contact_info,refresh},
  {anonymous,cancel,unanonymous},
  {nothing,done,nothing},
  {retry,done,nothing},
  {nothing,done,nothing},
  {notify_me,show_qr_code,nothing},
  {nothing,done,cancel}
};

EPD_WaveShare epd(EPD2_9, CS, RST, DC, BUSY);
MiniGrafx gfx = MiniGrafx(&epd, BITS_PER_PIXEL, palette);

OpenWeatherMapCurrentData conditions;
Astronomy::MoonData moonData;
OpenWeatherMapForecastData forecasts[MAX_FORECASTS];

// Setup simpleDSTadjust Library rules
simpleDSTadjust dstAdjusted(StartRule, EndRule);
uint32_t dstOffset = 0;
uint8_t foundForecasts = 0;

void updateData();
void drawProgress(uint8_t percentage, String text);
void drawTime();
void drawButtons(int layer, int option);
void drawCurrentWeather();
void drawForecast();
void drawTempChart();
void drawForecastDetail(uint16_t x, uint16_t y, uint8_t dayIndex);
void drawTwitter();


void drawBattery();
String getMeteoconIcon(String iconText);
const char* getMeteoconIconFromProgmem(String iconText);
const char* getMiniMeteoconIconFromProgmem(String iconText);
void drawForecast();


long lastDownloadUpdate = millis();

String moonAgeImage = "";
uint16_t screen = 0;
long timerPress;
bool canBtnPress;

boolean connectWifi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  //Manual Wifi
  Serial.print("[");
  Serial.print(WIFI_SSID.c_str());
  Serial.print("]");
  Serial.print("[");
  Serial.print(WIFI_PASS.c_str());
  Serial.print("]");
  WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());
  int i = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    i++;
    if (i > 20) {
      Serial.println("Could not connect to WiFi");
      return false;
    }
    Serial.print(".");
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  Serial.println("Woke up");
  pinMode(USR_BTN, INPUT_PULLUP);
  int btnState = digitalRead(USR_BTN);

  gfx.init();
  gfx.setRotation(1);
  gfx.setFastRefresh(false);

  // load config if it exists. Otherwise use defaults.
  boolean mounted = SPIFFS.begin();
  if (!mounted) {
    Serial.println("FS not formatted. Doing that now");
    SPIFFS.format();
    Serial.println("FS formatted...");
    SPIFFS.begin();
  }
  loadConfig();

  Serial.println("State: " + String(btnState));
  if (btnState == HIGH) {
    boolean connected = connectWifi();
    Serial.println("Button LOW...");
    startConfigPortal(&gfx);
  } else {
    boolean connected = connectWifi();
    Serial.println("Button HIGH...");
    if (connected) {
      updateData();
      gfx.fillBuffer(MINI_WHITE);

      drawTime();
      //drawBattery();
      drawWifiQuality();
      drawCurrentWeather();
      drawForecast();
      drawTwitter();
      drawButtons(currently_selected_layer,currently_selected_button);
      gfx.commit();
    } else {
      gfx.fillBuffer(MINI_WHITE);
      gfx.setColor(MINI_BLACK);
      gfx.setTextAlignment(TEXT_ALIGN_CENTER);
      gfx.drawString(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 30, "Could not connect to WiFi\nPress LEFT + RIGHT button\nto enter config mode");
      gfx.commit();
      //}
      //Serial.println("Going to sleep");
      //ESP.deepSleep(UPDATE_INTERVAL_SECS * 1000000);
    }
    pinMode(D0, INPUT);
    while (!Serial);		// Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)
  	 SPI.begin();			// Init SPI bus
  	rfid.PCD_Init();		// Init MFRC522
    rfid.PCD_DumpVersionToSerial();
    //pinMode(A0, INPUT);
    //pinMode(2, OUTPUT);
  }
}
/*
void loop(){
  readRFID();
  //delay(500);
}*/
void loop() {
  // read the state of the pushbutton value:
  buttonState1 = digitalRead(D0);
  buttonState2 = analogRead(A0);
  //digitalWrite(2, LOW);
  // check if the pushbutton is pressed. If it is, the buttonState is HIGH:
  if (buttonState1 == HIGH) {
    // turn LED on:
    Serial.println("One clicked");
    if (currently_selected_button < 2) {
      currently_selected_button++;
    }else{
      currently_selected_button = 0;
    }
    drawButtons(currently_selected_layer,currently_selected_button);
    Serial.println("Layer: " + String(currently_selected_layer) + "Button: " + String(currently_selected_button));
    gfx.commit();
    //digitalWrite(2, LOW);
    delay(500);
  }
  if (buttonState2 > 1023) {
    //Layer 0
    if (currently_selected_layer == 0) {
      Serial.println("LAYER 0");
      if (currently_selected_button == 0) {
        currently_selected_layer = 1;
      }else if (currently_selected_button == 1){
        currently_selected_layer = 5;
      }else{
        setup();
      }
    }
    //Layer 1 {anonymous,cancel,unanonymous}
    else if (currently_selected_layer == 1) {
      Serial.println("LAYER 1");
      if (currently_selected_button == 0) {
        Serial.println("Twilio would be sent and Confirmation shown");
      }else if (currently_selected_button == 1){
        currently_selected_layer = 0;
      }else{
        currently_selected_layer = 3;
        previousMillis = millis();
        Serial.println("Millis start to count");
      }
    }
    //Layer 2 {nothing,done,nothing}
    else if (currently_selected_layer == 2) {
      Serial.println("LAYER 2");
      if (currently_selected_button == 0) {
        Serial.println("Nothing");
      }else if (currently_selected_button == 1){
        Serial.println("RFID scan would initiate");
      }else{
        Serial.println("Nothing");
      }
    }
    //Layer 3 {retry,done,nothing}
    else if (currently_selected_layer == 3) {
      Serial.println("LAYER 3");
      Serial.println("RFID scan would initiate, then afterwards Twilio would be sent and Confirmation shown");
      if (currently_selected_button == 0) {
        Serial.println("Retry");
      }else if (currently_selected_button == 1){
        currently_selected_layer = 0;
      }else{
        Serial.println("Nothing");
      }
    }
    //Layer 4 {nothing,done,nothing}
    else if (currently_selected_layer == 4) {
      Serial.println("LAYER 4");
      if (currently_selected_button == 0) {
        Serial.println("Nothing");
      }else if (currently_selected_button == 1){
        currently_selected_layer = 0;
      }else{
        Serial.println("Nothing");
      }
    }
    //Layer 5 {notify_me,show_qr_code,nothing}
    else if (currently_selected_layer == 5) {
      Serial.println("LAYER 5");
      if (currently_selected_button == 0) {
        currently_selected_layer = 1;
      }else if (currently_selected_button == 1){
        currently_selected_layer = 6;
      }else{
        Serial.println("Nothing");
      }
    }
    //Layer 6 {nothing,done,cancel}
    else if (currently_selected_layer == 6) {
      Serial.println("LAYER 6");
      if (currently_selected_button == 0) {
        Serial.println("Nothing");
      }else if (currently_selected_button == 1){
        currently_selected_layer = 0;
      }else{
        Serial.println("Nothing");
      }
    }
    currently_selected_button = 1;
    drawButtons(currently_selected_layer,currently_selected_button);
    Serial.println("Another clicked");
    gfx.commit();
    /*if (currently_selected_layer) {
      currently_selected_button++;
    }else{
      currently_selected_button = 0;
    }*/
    //digitalWrite(2, HIGH);
    delay(500);
  }

  currentMillis = millis();
  if(currentMillis - previousMillis >= interval && previousMillis != 0){
    Serial.println("This is correct");
    currently_selected_button = 1;
    currently_selected_layer = 0;
    drawButtons(currently_selected_layer,currently_selected_button);
    gfx.commit();
    previousMillis = 0;
  }
  //if (currently_selected_layer == 3) {
    //readRFID();
    //delay(200);
  //}

  /*
  for (int8_t i = 0; i < 7; i++) {
    Serial.println("Drawing buttons: " + i);
    drawButtons(i);
    gfx.commit();
    Serial.println("Screen updated, going to sleep");
    delay(5000);
    //ESP.deepSleep(UPDATE_INTERVAL_SECS * 1000000);
  }*/
}

// Update the internet based information and update screen
void updateData() {
  configTime(UTC_OFFSET * 3600, 0, NTP_SERVERS);
  OpenWeatherMapCurrent *conditionsClient = new OpenWeatherMapCurrent();
  conditionsClient->setLanguage(OPEN_WEATHER_MAP_LANGUAGE);
  conditionsClient->setMetric(IS_METRIC);
  Serial.println("\nAbout to call OpenWeatherMap to fetch station's current data...");
  conditionsClient->updateCurrent(&conditions, OPEN_WEATHER_MAP_APP_ID, OPEN_WEATHER_MAP_LOCATION);
  delete conditionsClient;
  conditionsClient = nullptr;

  OpenWeatherMapForecast *forecastsClient = new OpenWeatherMapForecast();
  forecastsClient->setMetric(IS_METRIC);
  forecastsClient->setLanguage(OPEN_WEATHER_MAP_LANGUAGE);
  foundForecasts = forecastsClient->updateForecasts(forecasts, OPEN_WEATHER_MAP_APP_ID, OPEN_WEATHER_MAP_LOCATION, MAX_FORECASTS);
  delete forecastsClient;
  forecastsClient = nullptr;

  // Wait max. 3 seconds to make sure the time has been sync'ed
  Serial.println("\nWaiting for time");
  unsigned timeout = 3000;
  unsigned start = millis();
  while (millis() - start < timeout) {
    time_t now = time(nullptr);
    if (now) {
      break;
    }
    Serial.println(".");
    delay(100);
  }

  /*dstOffset = UTC_OFFSET * 3600 + dstAdjusted.time(nullptr) - time(nullptr);

  Astronomy *astronomy = new Astronomy();
  time_t now = time(nullptr) + dstOffset;
  moonData = astronomy->calculateMoonData(now);
  // illumination is not uniquely clear to identify moon age. e.g. 70% illumination happens twice in a full cycle
  float lunarMonth = 29.53;
  uint8_t moonAge = moonData.phase <= 4 ? lunarMonth * moonData.illumination / 2 : lunarMonth - moonData.illumination * lunarMonth / 2;
  moonAgeImage = String((char) (65 + ((uint8_t) ((26 * moonAge / 30) % 26))));
  delete astronomy;
  astronomy = nullptr;*/
}

// draws the clock
void drawTime() {

  char *dstAbbrev;
  char time_str[30];
  time_t now = dstAdjusted.time(&dstAbbrev);
  struct tm * timeinfo = localtime (&now);

  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  gfx.setFont(ArialMT_Plain_10);
  gfx.setColor(MINI_BLACK);
  String date = ctime(&now);
  date = date.substring(0, 11) + String(1900 + timeinfo->tm_year);

  if (IS_STYLE_12HR) {
    int hour = (timeinfo->tm_hour + 11) % 12 + 1; // take care of noon and midnight
    sprintf(time_str, "%2d:%02d:%02d", hour, timeinfo->tm_min, timeinfo->tm_sec);
    gfx.drawString(2, -2, String(FPSTR(TEXT_UPDATED)) + String(time_str));
  } else {
    sprintf(time_str, "%02d:%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    gfx.drawString(2, -2, String(FPSTR(TEXT_UPDATED)) + String(time_str));
  }
  gfx.drawLine(0, 11, SCREEN_WIDTH, 11);

}

// draws current weather information
void drawCurrentWeather() {

  gfx.setColor(MINI_BLACK);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.setFont(Meteocons_Plain_42);

  gfx.drawString(25, 20, conditions.iconMeteoCon);
  Serial.println("CONDICIONS CON");
  Serial.println(conditions.iconMeteoCon);
  Serial.println("CONDICIONS CON END");
  gfx.setColor(MINI_BLACK);
  gfx.setFont(ArialMT_Plain_10);
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  gfx.drawString(55, 15, DISPLAYED_CITY_NAME);

  gfx.setFont(ArialMT_Plain_24);
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  gfx.drawString(55, 25, String(conditions.temp,0) + (IS_METRIC ? "°C" : "°F") );

  gfx.setFont(ArialMT_Plain_10);
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  gfx.drawString(55, 50, conditions.main);
  gfx.drawLine(0, 65, SCREEN_WIDTH, 65);

}

unsigned int hourAddWrap(unsigned int hour, unsigned int add) {
  hour += add;
  if(hour > 23) hour -= 24;

  return hour;
}

void drawForecast() {
  time_t now = dstAdjusted.time(nullptr);
  struct tm * timeinfo = localtime (&now);

  unsigned int curHour = timeinfo->tm_hour;
  if(timeinfo->tm_min > 29) curHour = hourAddWrap(curHour, 1);

  drawForecastDetail(SCREEN_WIDTH / 2 - 35, 15, 0);
  drawForecastDetail(SCREEN_WIDTH / 2 - 1, 15, 1);
  drawForecastDetail(SCREEN_WIDTH / 2 + 37, 15, 2);
  drawForecastDetail(SCREEN_WIDTH / 2 + 73, 15, 3);
  drawForecastDetail(SCREEN_WIDTH / 2 + 109, 15, 4);
}

// helper for the forecast columns
void drawForecastDetail(uint16_t x, uint16_t y, uint8_t index) {
  time_t observation = forecasts[index].observationTime + dstOffset;
  struct tm* observationTm = localtime(&observation);

  gfx.setFont(ArialMT_Plain_10);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);

  gfx.drawString(x + 19, y - 2, String(observationTm->tm_hour) + ":00");

  gfx.setColor(MINI_BLACK);
  gfx.drawString(x + 19, y + 9, String(forecasts[index].temp,0) + "°");
  gfx.drawString(x + 19, y + 36, String(forecasts[index].rain,0) + (IS_METRIC ? "mm" : "in"));

  gfx.setFont(Meteocons_Plain_21);
  gfx.drawString(x + 19, y + 20, forecasts[index].iconMeteoCon);
  gfx.drawLine(x + 2, 12, x + 2, 65);

}

void drawTempChart() {
  if (foundForecasts == 0) {
    return;
  }
  float minTemp = 999;
  float maxTemp = -999;
  for (int i = 0; i < foundForecasts; i++) {
    float temp = forecasts[i].temp;
    if (temp > maxTemp) {
      maxTemp = temp;
    }
    if (temp < minTemp) {
      minTemp = temp;
    }
  }
  Serial.printf("Min temp: %f, max temp: %f\n", minTemp, maxTemp);
  float range = maxTemp - minTemp;
  uint16_t maxHeight = 35;
  uint16_t maxWidth = 120;
  uint16_t chartX = 168;
  uint16_t chartY = 70;
  float barWidth = maxWidth / foundForecasts;
  gfx.setColor(MINI_BLACK);
  gfx.drawLine(chartX, chartY + maxHeight, chartX + maxWidth, chartY + maxHeight);
  gfx.drawLine(chartX, chartY, chartX, chartY + maxHeight);
  uint16_t lastX = 0;
  uint16_t lastY = 0;
  gfx.setFont(ArialMT_Plain_10);
  gfx.setTextAlignment(TEXT_ALIGN_RIGHT);
  gfx.drawString(chartX - 5, chartY - 5, String(maxTemp, 0) + "°");
  gfx.drawString(chartX - 5, chartY + maxHeight - 5, String(minTemp, 0) + "°");
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  for (uint8_t i = 0; i < foundForecasts; i++) {
    float temp = forecasts[i].temp;
    float height = (temp - minTemp) * maxHeight / range;
    uint16_t x = chartX + i* barWidth;
    uint16_t y = chartY + maxHeight - height;
    if (i == 0) {
      lastX = x;
      lastY = y;
    }
    gfx.drawLine(x, y, lastX, lastY);

    if ((i - 3) % 8 == 0) {
      gfx.drawLine(x, chartY + maxHeight, x, chartY + maxHeight - 3);
      gfx.drawString(x, chartY + maxHeight, getTime(forecasts[i].observationTime));
    }
    lastX = x;
    lastY = y;
  }
}

String getTime(time_t timestamp) {
  time_t time = timestamp + dstOffset;
  struct tm *timeInfo = localtime(&time);

  char buf[6];
  sprintf(buf, "%02d:%02d", timeInfo->tm_hour, timeInfo->tm_min);
  return String(buf);
}

void drawTwitter(){
  gfx.setColor(MINI_BLACK);
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  gfx.setFont(ArialMT_Plain_10);
  Serial.println("Retrieving tweet");

  if (WiFi.status() == WL_CONNECTED) { //Check WiFi connection status

    HTTPClient http;  //Declare an object of class HTTPClient

    http.begin("http://tweety.gq/APIRequests.php?tweet=true");  //Specify request destination
    int httpCode = http.GET();                                  //Send the request

    if (httpCode > 0) { //Check the returning code

      String payload = http.getString();   //Get the request response payload
      Serial.println("Twitter update:" + payload);                     //Print the response payload
      //gfx.drawString(55, 69, "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
      //gfx.drawStringMaxWidth(55, 69, 241, payload);
      gfx.drawStringMaxWidth(5, 69, 291, "Twitter update:" + payload);
    }

    http.end();   //Close connection
  }
}

void readRFID(){
    // Look for new cards
    if ( ! rfid.PICC_IsNewCardPresent())
    {
      return;
    }
    // Select one of the cards
    if ( ! rfid.PICC_ReadCardSerial())
    {
      return;
    }
    rfid.PICC_DumpToSerial(&(rfid.uid));
    //Show UID on serial monitor
    Serial.print("UID tag :");
    String content= "";
    byte letter;
    for (byte i = 0; i < rfid.uid.size; i++)
    {
       Serial.print(rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
       Serial.print(rfid.uid.uidByte[i], HEX);
       content.concat(String(rfid.uid.uidByte[i] < 0x10 ? " 0" : " "));
       content.concat(String(rfid.uid.uidByte[i], HEX));
    }
    Serial.println();
    Serial.print("Message : ");
    content.toUpperCase();
    content = content.substring(1);
    //Serial.println("AM I a happy person?START" + content.substring(1) + "END");
    confirmAttendance(content);
}

void confirmAttendance(String NUID){
   if(WiFi.status()== WL_CONNECTED){   //Check WiFi connection status
     HTTPClient http;    //Declare object of class HTTPClient

     http.begin("http://tweety.gq/APIRequests.php");      //Specify request destination
     http.addHeader("Content-Type", "text/plain");  //Specify content-type header

     String textSend = "Crd:" + NUID + "Node_1";
     //String textSend = "Crd:" + NUID + "Node_2";
     int httpCode = http.POST(textSend);   //Send the request
     //String payload = http.getString();                  //Get the response payload
     String toPrint = "Card read: " + http.getString();
     Serial.println(httpCode);   //Print HTTP return code
     Serial.println(toPrint);    //Print request response payload
     http.end();  //Close connection
     gfx.drawStringMaxWidth(5, 69, 291, toPrint);
     delay(3000);
   }else{
      Serial.println("Error in WiFi connection");
   }
}

void drawBattery() {
  uint8_t percentage = 100;
  float adcVoltage = analogRead(A0) / 1024.0;
  // This values where empirically collected
  float batteryVoltage = adcVoltage * 4.945945946 -0.3957657658;
  if (batteryVoltage > 4.2) percentage = 100;
  else if (batteryVoltage < 3.3) percentage = 0;
  else percentage = (batteryVoltage - 3.3) * 100 / (4.2 - 3.3);

  gfx.setColor(MINI_BLACK);
  gfx.setFont(ArialMT_Plain_10);
  gfx.setTextAlignment(TEXT_ALIGN_RIGHT);
  gfx.drawString(SCREEN_WIDTH - 22, -1, String(batteryVoltage, 2) + "V " + String(percentage) + "%");
  gfx.drawRect(SCREEN_WIDTH - 22, 0, 19, 10);
  gfx.fillRect(SCREEN_WIDTH - 2, 2, 2, 6);
  gfx.fillRect(SCREEN_WIDTH - 20, 2, 16 * percentage / 100, 6);
}

// converts the dBm to a range between 0 and 100%
int8_t getWifiQuality() {
  int32_t dbm = WiFi.RSSI();
  Serial.println("DBM strength: ");
  Serial.println(dbm);
  if (dbm <= -100) {
    return 0;
  } else if (dbm >= -50) {
    return 100;
  } else {
    return 2 * (dbm + 100);
  }
}

void drawWifiQuality() {
  Serial.println("drawWifiQuality called");
  int8_t quality = getWifiQuality();
  Serial.println("drawWifiQuality returned, " + quality);
  gfx.setColor(MINI_BLACK);
  gfx.setTextAlignment(TEXT_ALIGN_RIGHT);
  Serial.println(String(quality) + "%");
  gfx.drawString(290, -2, "WiFi (" + String(quality) + "%)");
  for (int8_t i = 0; i < 4; i++) {
    for (int8_t j = 0; j < 2 * (i + 1); j++) {
      if (quality > i * 25 || j == 0) {
        gfx.setPixel(230 + 2 * i, 18 - j);
      }
    }
  }
}


void drawButtons(int layer, int option) {
  uint16_t third = SCREEN_WIDTH / 3;
  //gfx.setColor(MINI_WHITE);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.setFont(ArialMT_Plain_10);
  if (option == 0) {
      gfx.setColor(MINI_BLACK);
      gfx.fillRect(0, SCREEN_HEIGHT - 12, SCREEN_WIDTH, 12);
      gfx.setColor(MINI_WHITE);
      gfx.drawString(0.5 * third, SCREEN_HEIGHT - 12, buttons_strs[layer][0]);
  }else{
      gfx.setColor(MINI_WHITE);
      gfx.fillRect(0, SCREEN_HEIGHT - 12, SCREEN_WIDTH, 12);
      gfx.setColor(MINI_BLACK);
      gfx.drawString(0.5 * third, SCREEN_HEIGHT - 12, buttons_strs[layer][0]);
  }

  if (option == 1) {
      gfx.setColor(MINI_BLACK);
      gfx.fillRect(third, SCREEN_HEIGHT - 12, SCREEN_WIDTH, 12);
      gfx.setColor(MINI_WHITE);
      gfx.drawString(1.5 * third, SCREEN_HEIGHT - 12, buttons_strs[layer][1]);
  }else{
      gfx.setColor(MINI_WHITE);
      gfx.fillRect(third, SCREEN_HEIGHT - 12, SCREEN_WIDTH, 12);
      gfx.setColor(MINI_BLACK);
      gfx.drawString(1.5 * third, SCREEN_HEIGHT - 12, buttons_strs[layer][1]);
  }

  if (option == 2) {
      gfx.setColor(MINI_BLACK);
      gfx.fillRect(2*third, SCREEN_HEIGHT - 12, SCREEN_WIDTH, 12);
      gfx.setColor(MINI_WHITE);
      gfx.drawString(2.5 * third, SCREEN_HEIGHT - 12, buttons_strs[layer][2]);
  }else{
      gfx.setColor(MINI_WHITE);
      gfx.fillRect(2*third, SCREEN_HEIGHT - 12, SCREEN_WIDTH, 12);
      gfx.setColor(MINI_BLACK);
      gfx.drawString(2.5 * third, SCREEN_HEIGHT - 12, buttons_strs[layer][2]);
  }
  gfx.setColor(MINI_BLACK);
  //gfx.fillRect(0, SCREEN_HEIGHT - 12, SCREEN_WIDTH, 12);
  gfx.drawLine(0, SCREEN_HEIGHT - 12, SCREEN_WIDTH, SCREEN_HEIGHT - 12);
  gfx.drawLine(2 * (third/2), SCREEN_HEIGHT - 12, 2 * (third/2), SCREEN_HEIGHT);//button separator 1
  gfx.drawLine(2 * third, SCREEN_HEIGHT - 12, 2 * third, SCREEN_HEIGHT);//button separator 2
}
