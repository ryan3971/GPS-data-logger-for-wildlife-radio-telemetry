/*NOTE THAT IN SLEEP MODE THE ARDUINO USES A WATCH DOG TIMER WHICH CAN BE WILDLY INACCURATE. THIS CODE WAS DESIGNED
 * TO RUN A 6 HOUR , 18 HOURS OFF INTERVAL.
 *
 * Memory chip works in pages, where each page is made up of 64 bytes
 * Page 1: 0 - 63
 * Page 2: 64 - 127, etc
 *
 * If config is saved half on one page, half on the other, it doesn't save properly and beomes corrupt
 * Therefore, have address increment by 16 so that config is not saved on 2 seperate pages
 * Also, when a write goes bad, it tends to corrupt address 0 on the memory chip. This can corrupt the entire first config
 * Therefore, start at address 16 to prevent this
 *
 * Check if lng & lat are found. If so, exit loop, rather than attempting to get a new/better fix
 *
 * Only awake from 10 till 4
 * Will have to turn it on at 10 for it to work
 *
 * 2 minutes to get GPS
 * 2.2 seconds of delay
 * Get GPS every hour/half hour
 * Keep on from 10 till 4 --- 6 hours
 * Keep off for 18 hours
 *
 * Predetermined variables contating 6 hours and 18 hours in millis
 * Start at 10, save curretn millis()
 * Have the short sleeps account for the GPS fix tiem adn delays so it always starts on the hour/half-hour
 * Check if 6 hours have elasped
 * Sleep for 18 hours
 *
 * Rollover for millis() occurs after approx. 49 days
 *
 *
 * The combined time of getting GPS fix and short sleep is approx. 30min (2min fix, 28min sleep) (other stuff in loop takes milliseconds to complete)
 * Start at 10, have loop run 12 times, than go into long sleep (will be approx 4
 *
 * Hour issue from midnit to 3 wont be an issue since logger will never be on at that time
 *
 * Let user know if data is a repeat from previous data due to not getting a fix
 *
 * Set all pins to input (or output) and LOW
 *
 * Having Serial enaables consumes a lot of power (Only when connected to computer though)
 *
 *
 *TODO:
 * Clean currently does all of the chip
 * Add code that sets al pins to LOW to save on power
 * When GPS is on, unit draws approx. 100mA. Need to bring that down
 *
 */

// Libraries and links to download.
//For info on installing Arduino libraries, see https://www.arduino.cc/en/Guide/Libraries
#include <SoftwareSerial.h>
#include <TinyGPS++.h>      // http://arduiniana.org/libraries/tinygpsplus/
#include <LowPower.h>       // https://github.com/rocketscream/Low-Power
#include <Wire.h>
#include <eepromi2c.h>      //https://github.com/solexious/ACNodeEmbedCode/blob/master/eepromi2c.h
#include <EEPROM.h>

#define transistorGPS 8
#define transistorEEPROM 5

static const int stay_on = 1; /*amount of time gps is active in minutes*/
static const int short_sleep = 60; /*amount of time logger sleeps between reading in minutes*/

uint32_t feedDuration = stay_on * 60000;                 //time spent getting GPS fix
uint32_t shortSleep =(short_sleep * 60) / 8;   //time sleeping (short sleep)

uint32_t StartUpSleep =(24 * 60 * 60) / 8;   //time sleeping before running (used for deploying)

static const int RXPin = 4, TXPin = 3;
static const uint32_t GPSBaud = 9600;
unsigned short address;     //number range is 0 - 65,535


TinyGPSPlus gps;
SoftwareSerial ss(RXPin, TXPin);

static const int DATA_START_LOCATION = 16;
static const int ADDRESS_INCREMENT = 16;

static const int numberLines = 100;   // used in Z case. Force prints x number of lines.
unsigned long startup_timer;


int mode = 0;
static const int LOGGER = 0;
static const int MENU = 1;

uint32_t GPS_run;

struct config {
  byte fix_attempt;
  double lat;
  double lon;
  byte day;
  byte month;
  byte hour;
  byte minute;
  byte second;
  byte satellites;
} config;

//------------------------------------------( Setup )------------------------------------------//
void setup(){

  digitalWrite(LED_BUILTIN, HIGH);
  delay(250);
  digitalWrite(LED_BUILTIN, LOW);
  delay(250);


  pinMode(transistorGPS, OUTPUT);
  pinMode(transistorEEPROM, OUTPUT);

  pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin(9600);
  Wire.begin();
  ss.begin(GPSBaud);

  digitalWrite(transistorEEPROM, LOW);
  digitalWrite(transistorGPS, LOW);
  EEPROM.get(0, address);

  mode = LOGGER;

  Serial.println("Press spce bar (than click enter) to go menu. In 10 seconds, the GPS Logger will automaically start");
  startup_timer = millis();

  while (millis() - startup_timer < 10000)  {
    if (Serial.read() == ' '){
      mode = MENU;
      break;
    }
  }

}


//------------------------------------------( END Setup )------------------------------------------//


//------------------------------------------( Start Loop )------------------------------------------//
void loop(){

  switch (mode) {
    case MENU:{
      delay(1000);
      Serial.println("-------------STARTING TO READ AND WRITE---------------");
      Serial.println("______________________________________________________");
      menu();
      while(true){
       runMenu();
      }
       break;
    }
    case LOGGER:{
      Serial.println(F("Starting GPS..."));
      delay(1000);

      digitalWrite(LED_BUILTIN, HIGH);
      delay(250);
      digitalWrite(LED_BUILTIN, LOW);
      delay(250);

      for (int i = 0; i < StartUpSleep; i++) {
        LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
      }

      while(true){
        runLogger();
      }
       break;
    }
  }
}
//------------------------------------------( END Loop )------------------------------------------//

//----------------------------( Functions )-------------------------//

void runLogger()  {

    digitalWrite(transistorGPS, HIGH);

    delay(500);

    GPS_run = millis();

    while (millis() - GPS_run < feedDuration) {
      feedGPS();
      // if ((gps.location.lat() != config.lat) && (gps.location.lng() != config.lon) && (gps.time.minute() != config.minute))WriteEE2 ();
    }
    delay(500);
    WriteEE();
    printLocation();
    delay(100);

    for (int i = 0; i < shortSleep; i++) {
      LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
    }
    delay(500);
  }


void runMenu()  {

 digitalWrite(transistorEEPROM, HIGH);


 if (Serial.available() == 1) {
    switch (toUpperCase(Serial.read())) {

      /************************* read EEPROM *********************/
      case 'R':{

        address = DATA_START_LOCATION;

        Serial.println();
        Serial.println("************* COPY Begin *****************");
        Serial.println("Lat     \t Lng     \t Day/Month      EST_Time       sat.");

        while (true) {
          eeRead(address, config);    //Read the first location into structure 'config'
          address += ADDRESS_INCREMENT;
          delay(100);

          if (config.fix_attempt == 0) {
            Serial.println();
            Serial.println("No more locations were recorded");
            Serial.println();
            break;
          }
          else {
              printLocation();
            }
        }

        Serial.println("************* Finished *****************"); Serial.println();
        menu();
      }
      break;

      /****** erases EEPROM by writing a '0' to each address *******/
      case 'C':{
        digitalWrite(LED_BUILTIN, HIGH);

        Serial.println("");
        Serial.println("--------Clearing I2C EEPROM--------");
        Serial.print("Pleast wait until light turns off...");

        EEPROM.get(0, address);
        //resets the address on the EEPROM
        EEPROM.put(0, DATA_START_LOCATION);

        for (int i = 0; i < address; i++) {
          eeWrite(i, 0); delay(6);
        }

        Serial.println("Done");
        Serial.println("---------Done Clearing---------");
        digitalWrite(LED_BUILTIN, LOW); Serial.println();
        menu();
      }
      break;
       /*---------------------------------------------------------------*/

       /****** for testing *******/
      case 'Z':{
        digitalWrite(LED_BUILTIN, HIGH);

        address = DATA_START_LOCATION;

        for (int i = 0 ; i < numberLines; i++){
          eeRead(address, config); delay(100);
          address += ADDRESS_INCREMENT;
          delay(10);
          printLocation();
        }

        digitalWrite(LED_BUILTIN, LOW);
        Serial.println();
        menu();
      }
      break;
       /*---------------------------------------------------------------*/

      default:
        menu();
      break;
    }
  }
}

bool feedGPS() {
    while (ss.available()) {      //Checks if data is available on those pins
      if (gps.encode(ss.read()))    //Repeatedly feed it characters from your GPS device:
        return true;
        }
    return false;
}


void menu() {
  Serial.println("___________MENU___________");
  Serial.println("Please type:");
  Serial.println("C - clear memory");
  Serial.println("R - Read locations (Reads each location, but stops when Month == 0)");
  Serial.print("Z - force print the first "); Serial.print(numberLines);Serial.println(" lines");
}

void printLocation() {

  Serial.print(config.lat,6); Serial.print("\t");
  Serial.print(config.lon,6); Serial.print("\t  ");
  Serial.print(config.day); Serial.print("/");
  Serial.print(config.month);Serial.print(" \t\t");
  if (config.hour < 10) Serial.print("0");
  Serial.print(config.hour); Serial.print(":");
  if (config.minute < 10) Serial.print("0");
  Serial.print(config.minute);Serial.print(":");
  if (config.second < 10) Serial.print("0");
  Serial.print(config.second);Serial.print("\t");
  Serial.println(config.satellites, DEC);
  delay(10);
}

void WriteEE () {

  config.fix_attempt = 1;

  if (gps.location.isUpdated()) {
    config.lat = gps.location.lat();
    config.lon = gps.location.lng();
 //   Serial.print("YES");
  }else {
    config.lat = 0.0;
    config.lon = 0.0;
  //  Serial.print("NO");

  }

  Serial.print(gps.location.lat(),6); Serial.print("\t");
  Serial.print(gps.location.lng(),6); Serial.print("\t  ");
  config.day = gps.date.day();
  config.month = gps.date.month();
  config.hour = gps.time.hour();
  config.minute = gps.time.minute();
  config.second = gps.time.second();
  config.satellites = gps.satellites.value();

  delay(500);
  digitalWrite(transistorGPS, LOW);
  delay(500);
  digitalWrite(transistorEEPROM, HIGH);
  delay(100);

  eeWrite(address, config);
  address += ADDRESS_INCREMENT;
  EEPROM.put(0, address);
  delay(100);

  digitalWrite(transistorEEPROM, LOW);
  delay(200);
}
