// You have to hold down RST on LoRa+GPS Shields when uploading or yo uwill get init failed


/*-------------------------INCLUDES-----------------------------*/
#include <SPI.h>
#include <SoftwareSerial.h>
#include <RHReliableDatagram.h>
#include <RH_RF95.h>

#include <TinyGPS++.h>
#include <Wire.h>


/*----------------------DEFINITIONS-----------------------------*/

#define CLIENT_ADDRESS 1
#define SERVER_ADDRESS 2

#define transistorGPS 10
#define RXPin = 9; 
#define TXPin = 3;

/*-------------------------GLOBALS------------------------------*/

char dataArr[10]; 
String sensorString = "";
boolean sensorStringComplete = false;

static const int stay_on = 1;
uint32_t feedDuration = stay_on * 60000;  

/*---------------------SENSOR CONSTRUCTORS------------------------*/

SoftwareSerial ss(RXPin, TXpin);
TinyGPSPlus gps; 

/*---------------------LORA CONSTRUCTORS------------------------*/

RH_RF95 driver;

RHReliableDatagram LORAmanager(driver, CLIENT_ADDRESS);

/*---------------------------SETUP------------------------------*/

void setup() {

    Wire.begin();
    ss.begin(9600);
    Serial.begin(9600);

    pinMode(transistorGPS, OUTPUT);
    
    if (!LORAmanager.init()){
        Serial.println("init failed");
    } else {
        Serial.println("init succeeded");
    }
}

void loop() {

    digitalWrite(transistorGPS,HIGH);
    delay(400); 
    GPS_run = millis();

    while (millis() - GPS_run < feedDuration) {
    feedGPS();
    }

    delay(100);
    digitalWrite(transistorGPS, LOW);
    delay(100);
  
    
    format data 
    send data 
    





    sensorSerial.print("R\r");

    while (sensorSerial.available() > 0 && !sensorStringComplete) {
        char inchar = (char)sensorSerial.read();
        sensorString += inchar;
        Serial.println(inchar);
        if (inchar == '\r') {
            sensorStringComplete = true;
        }
    }

    if(sensorStringComplete && isdigit(sensorString[0])) {
        sensorString.toCharArray(dataArr, sizeof(dataArr));
        if (LORAmanager.sendtoWait(dataArr, sizeof(dataArr), SERVER_ADDRESS)) {
            Serial.print("Sending: ");
            Serial.println(dataArr);
            sensorStringComplete = false;
            sensorString = "";
        } 
    }
    
    delay(20000);
}




bool feedGPS() {
    while (ss.available()) {      //Checks if data is available on those pins
      if (gps.encode(ss.read()))    //Repeatedly feed it characters from your GPS device:
        return true;
        }
    return false;
}






