#include <Wire.h>
#include "SparkFun_VL53L1X.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <ESP32Time.h>


#define LEFT 0
#define RIGHT 1
#define XSHUT1 32
#define INTERRUPT1 33
#define XSHUT2 16
#define INTERRUPT2 4
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
#define SENSOR1_ADDR 0x9
#define SENSOR2_ADDR 0x14
#define BUZZER_PIN 17


//date//
#define YEAR 2023
#define MONTH 1
#define DAY 12
#define HOUR 11
#define MINUTE 0
#define SECOND 0


SFEVL53L1X Sensor1(Wire, XSHUT1, INTERRUPT1);
SFEVL53L1X Sensor2(Wire, XSHUT2, INTERRUPT2);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
ESP32Time rtc(0);


int THRESHOLD_DIS = 0;
int sensorHeight = 0;
uint8_t zone = 0;
int distance1 = 0;
int distance2 = 0;
uint8_t error = 0;
int last_tick = 0;
uint8_t center[2] = {167, 231};
uint8_t pathTrack[4] = {0,0,0,0};
bool event = false;
uint8_t stage = 0;
uint8_t last_stage = 0;
uint8_t trackFillingSize = 1;
int ppl = 0;
int duration = 0; 
int inNout = 0;
int BuzzTick = 0;
String Location = "Heung Che Street Public Toilet";

  
//Time message generator//
String TimeMsg()
{
  String year, month, day, hour, minute, second;
  year = String(rtc.getYear());
  month = String(rtc.getMonth()+1);
  day = String(rtc.getDay());
  hour = String(rtc.getHour(true));
  minute = String(rtc.getMinute());
  second = String(rtc.getSecond());

  if(rtc.getMonth() < 10)
    month = "0" + month;
  if(rtc.getDay() < 10)
    day = "0" + day;
  if(rtc.getHour() < 10)
    hour = "0"+hour;
  if(rtc.getMinute() < 10)
    minute = "0"+minute;
  if(rtc.getSecond() < 10)
    second = "0"+second;
  String msg = year + '-'
               + month + '-'
               + day + 'T'
               + hour + ':'
               + minute + ':'
               + second + 'Z';
   return msg;
}

//data log function//
void dataLog(fs::FS &fs, const char * path, String msg)
{
  File file = fs.open(path, FILE_APPEND);
  if(file.println(msg)){
        Serial.println("File written");
    } else {
        Serial.println("Write failed");
  file.close();
  }
}

//buzzer//
void beep()
{
  BuzzTick = millis();
  digitalWrite(BUZZER_PIN, HIGH);
}

//people counting function//
int peopleCounting(int distance1, int distance2,  uint8_t zone)
{
  static int pathTrack[] = {0,0,0,0};
  static int pathTrackFillingSize = 1;
  static int leftPreviousStatus = 0;
  static int rightPreviousStatus = 0;
  static int start = 0;
  static int peopleCount = 0;
  static int prevPT2 = 0;
  static int prevPPL = 0;
  
  int currentZoneStatus = 0;
  int allZonesCurrentStatus = 0;
  int anEventHasHappened = 0;

  if((distance1 < THRESHOLD_DIS) || (distance2 < THRESHOLD_DIS))
  {
    currentZoneStatus = 1;
  }

  if(zone == LEFT)
  {
    if(currentZoneStatus != leftPreviousStatus)
    {
      anEventHasHappened = 1;

      if(currentZoneStatus== 1)
      {
        allZonesCurrentStatus += 1;
      }
      if(rightPreviousStatus == 1)
      {
        allZonesCurrentStatus += 2;
      }

      leftPreviousStatus = currentZoneStatus;
    }
  }

  else
  {
    if(currentZoneStatus != rightPreviousStatus)
    {
      anEventHasHappened = 1;
      if(currentZoneStatus == 1)
      {
        allZonesCurrentStatus += 2;
      }
      if(leftPreviousStatus == 1)
      {
        allZonesCurrentStatus += 1;
      }
      rightPreviousStatus = currentZoneStatus;
    }
  }

  if(anEventHasHappened)
  {
    if(pathTrackFillingSize < 4)
    {
      pathTrackFillingSize++;
    }
    if((leftPreviousStatus == 0) && (rightPreviousStatus ==0))
    {
      if(pathTrackFillingSize == 4)
      {
        if((pathTrack[1] == 1) && (pathTrack[2] == 3) && (pathTrack[3] == 2))
        {
          peopleCount++;
        }
        else if((pathTrack[1] == 2) && (pathTrack[2] == 3) && (pathTrack[3] == 1))
        {
          peopleCount--;
        }
      }
      for (int i=0; i<4; i++){
        pathTrack[i] = 0;}
      pathTrackFillingSize = 1;
    }
    else
    {
      pathTrack[pathTrackFillingSize - 1] = allZonesCurrentStatus;
    }
  }
  if(prevPT2 == 0)
  {
    if(pathTrack[1] == 1 || pathTrack[1] == 2)
    {
      start = millis();
    }
  }

  if(prevPPL - peopleCount == 1)
  {
    beep();
    Serial.print("someone out   ");
    Serial.print("in room: ");
    Serial.println(peopleCount);
    String msg = TimeMsg() + ',' + String(1) + ',' + String(-1) + ',' + String(peopleCount) + ',' + Location;
    dataLog(SD, "/dataLog.csv", msg);
    //Serial.print("duration");
    //Serial.println(millis() - start);
  }
  else if(prevPPL - peopleCount == -1)
  {
    beep();
    Serial.print("someone in   ");
    Serial.print("in room: ");
    Serial.println(peopleCount);
    String msg = TimeMsg() + ',' + String(1) + ',' + String(1) + ',' + String(peopleCount) + ',' + Location;
    dataLog(SD, "/dataLog.csv", msg);
    //Serial.print("duration");
    //Serial.println(millis() - start);
  }
  prevPT2 = pathTrack[1];
  prevPPL = peopleCount;
  Serial.print("[ ");
  for(int i = 0; i < 3; i++)
  {
    Serial.print(pathTrack[i]);
    Serial.print(", ");
  }
  Serial.print(pathTrack[3]);
  Serial.println(" ]");
  return peopleCount;
}

int thresholdCalc()
{
  int startTime = millis();
  int d = 0, ld = 0;
  while(millis() - startTime < 5000)
  {
  Sensor1.startRanging();
  while(!Sensor1.checkForDataReady()){delay(1);} 
  distance1 = Sensor1.getDistance();
  Sensor1.clearInterrupt();
  Sensor1.stopRanging(); 
  Serial.print(distance1);
  Serial.print("        ");
  Sensor2.startRanging();
  while(!Sensor2.checkForDataReady()){delay(1);}  
  distance2 = Sensor2.getDistance();  
  Sensor2.clearInterrupt();
  Sensor2.stopRanging();
  Serial.println(distance2);
  d += distance1;
  ld++;
  d += distance2;
  ld++;
  }
  return (d/ld)*0.8;
}

void setup() {
  // put your setup code here, to run once:
  Wire.begin();
  
  Serial.begin(115200);
  Serial.println("Sensor initializing...");

  pinMode(XSHUT2, OUTPUT);
  delay(10);
  pinMode(XSHUT1, INPUT);
  delay(150);
  Sensor1.init();
  Sensor1.setI2CAddress(0x12);  
  if(Sensor1.begin())
  {
    Serial.println("Failed to initialize the sensor1...");
  }
  pinMode(XSHUT1, INPUT);
  delay(50);
  pinMode(XSHUT2, INPUT);
  delay(150);
  Sensor2.init();
  delay(100);
  Sensor2.setI2CAddress(0x28);
  if(Sensor2.begin())
  {
    Serial.println("Failed to initialize the sensor1...");
    while(1);
  }
  pinMode(XSHUT2, INPUT);

  Serial.print(Sensor1.getI2CAddress());
  Serial.print(Sensor2.getI2CAddress());
  Sensor1.setDistanceModeLong();
  Sensor1.setTimingBudgetInMs(20);
  Sensor1.setIntermeasurementPeriod(25);
  Sensor2.setDistanceModeLong();
  Sensor2.setTimingBudgetInMs(20);
  Sensor2.setIntermeasurementPeriod(25);
//  Sensor.calibrateOffset(140);
//  Serial.println(Sensor.getOffset());
//  Sensor.calibrateXTalk(140);
//  Serial.println(Sensor.getXTalk());
//  Serial.println("calibrated");
//  delay(2000);
  THRESHOLD_DIS = thresholdCalc();
  Serial.print("threshold disance: ");
  Serial.println(THRESHOLD_DIS);
  Serial.println("Sensor initialized!");

  // display initilization //
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  
  if(!SD.begin()){
        Serial.println("Card Mount Failed");
        return;
    }
  uint8_t cardType = SD.cardType();

  if(cardType == CARD_NONE){
      Serial.println("No SD card attached");
      return;
  }

  rtc.setTime(SECOND, MINUTE, HOUR, DAY, MONTH, YEAR);
  String msg = ("Time Stamp, People detection, Direction, Total, Location");
  if(!SD.exists("/dataLog.csv"))
    dataLog(SD, "/dataLog.csv", msg);
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("Initialized!");
  display.display();

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  delay(2000);
}

void loop() {
  if(Serial.read() == '0')
  {
    ESP.restart();
  }
  // put your main code here, to run repeatedly:
  if(millis() - BuzzTick > 300)
    {
      digitalWrite(BUZZER_PIN, LOW);
    }
  Sensor1.setROI(8,9, center[zone]);
  Sensor1.startRanging();
  while(!Sensor1.checkForDataReady()){delay(1);} 
  distance1 = Sensor1.getDistance();
  Sensor1.clearInterrupt();
  Sensor1.stopRanging(); 

  
  Sensor2.setROI(8,9, center[zone]);
  Sensor2.startRanging();
  while(!Sensor2.checkForDataReady()){delay(1);}  
  distance2 = Sensor2.getDistance();  
  Sensor2.clearInterrupt();
  Sensor2.stopRanging();

  ppl = peopleCounting(distance1, distance2, zone);
  display.clearDisplay();
  display.setTextSize(4);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println(ppl);
  display.display();
//  if(!zone)
//  {
//    left_distance = Sensor.getDistance();
//    //ppl = peopleCounting(left_distance, zone);
//    Serial.print("Left distance: ");
//    Serial.print(left_distance);
//  }
//  else
//  {
//    right_distance = Sensor.getDistance();
//    //ppl = peopleCounting(right_distance, zone);
//    Serial.print("Right distance: ");
//    Serial.println(right_distance);
//  }
  
  
//  Serial.println(ppl);
  zone++;
  zone = zone%2;
}
