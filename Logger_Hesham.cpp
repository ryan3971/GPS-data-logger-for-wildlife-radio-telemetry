/****************************************************************************************************************************
   NOT ORIG
   NOT ORIG
   NOT ORIG
   NOT ORIG
   NOT ORIG
   NOT ORIG
   NOT ORIG
   NOT ORIG
   NOT ORIG
 ***************************************************************************************************************************/

// Libraries and links to download.
//For info on installing Arduino libraries, see https://www.arduino.cc/en/Guide/Libraries
#include <SoftwareSerial.h>
#include <TinyGPS++.h>      // http://arduiniana.org/libraries/tinygpsplus/
#include <LowPower.h>       // https://github.com/rocketscream/Low-Power
#include <Wire.h>
#include <eepromi2c.h>
#include <EEPROM.h>


#define transistorGPS 2
#define transistorEEPROM 5
#define LOCATION 16
#define Batpin A0



struct Rconfig {
  byte fix_attempt;
  double lat;
  double lon;
  byte day;
  byte month;
  byte hour;
  byte minute;
  byte satellites;
  byte second;
} Rconfig;

/*----------------------LOGGING-----------------------*/

int stay_on =  1; /*minutes*/
float logger_interval =18 ;  /*hours*/
/*
int desiredTime = 4;  
float logger_interval2 = 5 ;    //time off inbetween readings

int loop_time = (stay_on + (60 * logger_interval2));
*/

uint32_t feedDuration = stay_on * 60000;               //time GPS is awake and feeding data to variable containers
uint32_t sleepInterval = (logger_interval * 60 * 60) / 8;   //raw number of cycles of watchdog interrupt
uint32_t sleepInterval2 = 8;//(logger_interval2 * 60 * 60) / 8;
int day_interval = 50;//desiredTime/loop_time ;


static const int RXPin = 4, TXPin = 3;
static const uint32_t GPSBaud = 9600;
uint32_t GPS_run = millis(), timer = millis();

unsigned int address;     //number range is 0 - 65,535
uint32_t beg = millis();
byte EEadd = 1;

TinyGPSPlus gps;

SoftwareSerial ss(RXPin, TXPin);



/*----------------------------------------------------*/


struct config {
  byte fix_attempt;
  double lat;
  double lon;
  byte day;
  byte month;
  byte hour;
  byte minute;
  byte satellites;
  byte second;
} config;



unsigned short address_temp;
char iniate;
char action;
int count;


int numberLines = 25;   // used in Z case. Force prints x number of lines.

void setup() {

  Serial.begin(9600);
  Wire.begin();
  ss.begin(GPSBaud);

  pinMode(transistorGPS, OUTPUT);
  pinMode(transistorEEPROM, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(Batpin, INPUT);

  pinMode(transistorEEPROM, OUTPUT);
  digitalWrite(transistorEEPROM, HIGH);
  digitalWrite(transistorGPS, LOW);

  address = EEPROM.read(1);
  digitalWrite(LED_BUILTIN, LOW);
  menu2();

}

/*--------------------------------------START----------------------------------*/
void loop() {
  if (millis() < 5000) {
    iniate = toUpperCase(Serial.read());
  } else iniate = 'L';
  switch (iniate) {

    /*--------------------------------LOGGINH -------------------------------- */

    case 'L': {
        Serial.print("--------------------------STARTING TO LOG---------------");
        while (true) {
          while (count <= day_interval) {
            
            digitalWrite(transistorGPS, HIGH);
            delay(1000);                       // wait for a second

            GPS_run = millis();
            while (millis() - GPS_run < feedDuration) {
              feedGPS();
              // if ((gps.location.lat() != config.lat) && (gps.location.lng() != config.lon) && (gps.time.minute() != config.minute))WriteEE2 ();
            }

            WriteEE ();

            printlocation2();
            delay(1000);


            for (int i = 0; i < (sleepInterval2); i++) {
              LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
            }
            LowPower.powerDown(SLEEP_4S, ADC_OFF, BOD_OFF);

            count++;

          }
          count = 0;
          
          for (int i = 0; i < (sleepInterval); i++) {
            LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
          }
        }


      }

    /*--------------------------------READ AND WRITE -------------------------------- */

    case'F': {
        Serial.println("-------------STARTING TO READ AND WRITE---------------");
        Serial.println("______________________________________________________");
        menu();

        while (true) {
          action = toUpperCase(Serial.read());
          switch (action) {

            /************************* read EEPROM *********************/



            case 'R': {
                digitalWrite(LED_BUILTIN, HIGH);

                // Reset flag and start address
                int x = 0; address = 16;

                Serial.println();
                Serial.println("************* COPY Begin *****************");
                Serial.println("Lat,Lon,Day/Month,UTC_Time");

                while (x == 0) {
                  eeRead(address, Rconfig);    //Read the first location into structure 'config'
                  delay(100);

                  /*A value of 0 for month (a non-possibility means the unit
                    did not record, so break out of while loop*/
                  if (Rconfig.fix_attempt == 0) {
                    Serial.println();
                    Serial.println("No more locations were recorded");
                    Serial.println();
                    x = 1;
                  }

                  else {
                    printlocation2();
                  }

                  address += LOCATION;
                  delay(5);
                }

                digitalWrite(LED_BUILTIN, LOW);
                Serial.print("Last address: "); Serial.println(EEPROM.read(1));
                Serial.println("************* Finished *****************"); Serial.println();
                menu();
              }
              break;

            /****** erases EEPROM by writing a '0' to each address *******/
            case 'C': {
                digitalWrite(LED_BUILTIN, HIGH);

                EEPROM.write(1, LOCATION);

                Serial.println("");
                Serial.println("--------Clearing I2C EEPROM--------");
                Serial.print("Pleast wait until light turns off...");

                for (int i = 0; i < 4000; i++) {
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
            case 'Z': {
                digitalWrite(LED_BUILTIN, HIGH);

                //  eeRead(14, address_temp);

                address = 0;

                for (int i = 0 ; i < 50; i++) {
                  eeRead(address, Rconfig); delay(100);
                  printlocation2();
                  address += LOCATION;
                }

                Serial.print("Last address: "); Serial.println(EEPROM.read(1));
                digitalWrite(LED_BUILTIN, LOW);
                Serial.println();
                menu();
              }
              break;
            /*---------------------------------------------------------------*/

            default:

              break;
          }

        }



      } // read case





  } // for switch

  // for if
}
// for loop
/*-------------------------------------- FUNCTIONS -----------------------*/



void menu() {
  Serial.println("Please type:");
  Serial.println("C - clear memory");
  Serial.println("R - Read locations (Reads each location, but stops when Month == 0)");
  Serial.print("Z - force print the first "); Serial.print(numberLines); Serial.println(" lines");
}

void menu2() {
  Serial.println("___________MENU___________");
  Serial.println("Please type:");
  Serial.println("F - For Read and Writing Data");
  Serial.println("Wait 5 secounds to iniate logger");
}


bool feedGPS() {
  while (ss.available()) {
    if (gps.encode(ss.read())) {
      return true;

    }
    return false;
  }
}

void printlocation2() {

  Serial.print(Rconfig.lat, 6); Serial.print("\t");
  Serial.print(Rconfig.lon, 6); Serial.print("\t");
  Serial.print(Rconfig.month); Serial.print("/");
  Serial.print(Rconfig.day); Serial.print("\t");
  if (Rconfig.hour < 10) Serial.print("0");
  Serial.print(Rconfig.hour); Serial.print(":");
  if (Rconfig.minute < 10) Serial.print("0");
  Serial.print(Rconfig.minute); Serial.print(":");
  Serial.print(Rconfig.second); Serial.print("\t");
  Serial.println(Rconfig.satellites, DEC);
  delay(10);
}

void WriteEE () {

  config.fix_attempt = 1;
  config.lat = gps.location.lat();
  config.lon = gps.location.lng();
  config.day = gps.date.day();
  config.month = gps.date.month();
  config.hour = gps.time.hour();
  config.minute = gps.time.minute();
  config.second = gps.time.second();
  config.satellites = gps.satellites.value();


  digitalWrite(transistorGPS, LOW);
  delay(100);
  digitalWrite(transistorEEPROM, HIGH);

  eeWrite(address, config);
  address += LOCATION;
  EEPROM.write(1, address);
  delay(100);

  digitalWrite(transistorEEPROM, LOW);
}



