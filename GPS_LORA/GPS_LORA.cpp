
// Libraries and links to download.
//For info on installing Arduino libraries, see https://www.arduino.cc/en/Guide/Libraries
#include <SoftwareSerial.h>
#include <TinyGPS++.h>      // http://arduiniana.org/libraries/tinygpsplus/
#include <LowPower.h>       // https://github.com/rocketscream/Low-Power
#include <Wire.h>
//#include <EEPROM.h>
#include <CayenneLPP.h>


#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>

#define transistorGPS 10

CayenneLPP lpp(51);

// LoRaWAN NwkSKey, network session key (msb format)
static const PROGMEM u1_t NWKSKEY[16] = { 0x92, 0x4C, 0xBC, 0xD5, 0x6B, 0x22, 0xB0, 0x4F, 0x6E, 0xB0, 0xFA, 0x65, 0x9C, 0xA6, 0xA5, 0xCE };

// LoRaWAN AppSKey, application session key (msb format)
static const u1_t PROGMEM APPSKEY[16] = { 0x71, 0xB3, 0x1E, 0x24, 0x3B, 0xB2, 0xBF, 0xC4, 0x7D, 0x82, 0xDE, 0x85, 0x8A, 0xB1, 0x03, 0x17 };
// LoRaWAN end-device address (DevAddr)
static const u4_t DEVADDR = 0x26021966; // <-- Change this address for every node!

// These callbacks are only used in over-the-air activation, so they are
// left empty here (we cannot leave them out completely unless
// DISABLE_JOIN is set in config.h, otherwise the linker will complain).
void os_getArtEui (u1_t* buf) { }
void os_getDevEui (u1_t* buf) { }
void os_getDevKey (u1_t* buf) { }

static osjob_t sendjob;

// Pin mapping
const lmic_pinmap lmic_pins = {
    .nss = 6,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 5,
    .dio = {8, 7, LMIC_UNUSED_PIN},//LMIC_UNUSED_PIN for 7
};

static const int stay_on = 1; /*amount of time gps is active in minutes*/
static const int short_sleep = 7; /*amount of time logger sleeps between reading in minutes*/


uint32_t feedDuration = stay_on * 60000;                 //time spent getting GPS fix
uint32_t shortSleep =(short_sleep * 60) / 8;   //time sleeping (short sleep)
//Will always round down if a deciaml


static const int RXPin = 9, TXPin = 3;
static const uint32_t GPSBaud = 9600;

uint32_t GPS_run;

TinyGPSPlus gps;
SoftwareSerial ss(RXPin, TXPin);

void format_and_send_data();


//------------------------------------------( Setup )------------------------------------------//
void setup(){

  Serial.begin(9600);
  Wire.begin();
  ss.begin(GPSBaud);
  Serial.println(F("Starting"));

  pinMode(transistorGPS, OUTPUT);

  // LMIC init
  os_init();
  // Reset the MAC state. Session and pending data transfers will be discarded.
  LMIC_reset();

  // Set static session parameters. Instead of dynamically establishing a session
  // by joining the network, precomputed session parameters are be provided.
  #ifdef PROGMEM
  // On AVR, these values are stored in flash and only copied to RAM
  // once. Copy them to a temporary buffer here, LMIC_setSession will
  // copy them into a buffer of its own again.
  uint8_t appskey[sizeof(APPSKEY)];
  uint8_t nwkskey[sizeof(NWKSKEY)];
  memcpy_P(appskey, APPSKEY, sizeof(APPSKEY));
  memcpy_P(nwkskey, NWKSKEY, sizeof(NWKSKEY));
  LMIC_setSession (0x1, DEVADDR, nwkskey, appskey);
  #else
  // If not running an AVR with PROGMEM, just use the arrays directly
  LMIC_setSession (0x1, DEVADDR, NWKSKEY, APPSKEY);
  #endif

  #if defined(CFG_us915)
  // NA-US channels 0-71 are configured automatically
  // but only one group of 8 should (a subband) should be active
  // TTN recommends the second sub band, 1 in a zero based count.
  // https://github.com/TheThingsNetwork/gateway-conf/blob/master/US-global_conf.json
  LMIC_selectSubBand(1);
  #endif

  // Disable link check validation
  LMIC_setLinkCheckMode(0);

  // TTN uses SF9 for its RX2 window.
  LMIC.dn2Dr = DR_SF9;
//***************************************************************************************************************************
  // Set data rate and transmit power for uplink (note: txpow seems to be ignored by the library)
  LMIC_setDrTxpow(DR_SF7,14);

  pinMode(LED_BUILTIN, OUTPUT);

  digitalWrite(LED_BUILTIN, HIGH);
  delay(250);
  digitalWrite(LED_BUILTIN, LOW);
  delay(250);
  //digitalWrite(transistorGPS, HIGH);

}

static uint8_t AppData[11];
bool lora_loop = false;


void loop(){
 
  digitalWrite(transistorGPS, HIGH);
  delay(500);
  GPS_run = millis();

  while (millis() - GPS_run < feedDuration) {
    feedGPS();
  }
  delay(100);
 digitalWrite(transistorGPS, LOW);
  delay(100);

  digitalWrite(LED_BUILTIN, HIGH);
  delay(250);
  digitalWrite(LED_BUILTIN, LOW);
  delay(250);

  Serial.print(gps.location.lat(), 6); Serial.print("\t");
  Serial.print(gps.location.lng(),6); Serial.print("\t  ");
  Serial.print(gps.time.hour()); Serial.print("/");
  Serial.print(gps.time.minute());Serial.println(" \t\t");
  delay(100);
  
  lpp.reset();
  lpp.addGPS(1, gps.location.lat(), gps.location.lng(), gps.altitude.meters());

  delay(100);

  os_setCallback(&sendjob, do_send);
  delay(500);

  lora_loop = true;
  while (lora_loop)  {
    os_runloop_once();
  }
  delay(500);

  for (int i = 0; i < shortSleep; i++) {
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
 }
  delay(500);
}


bool feedGPS() {
    while (ss.available()) {      //Checks if data is available on those pins
      if (gps.encode(ss.read()))    //Repeatedly feed it characters from your GPS device:
        return true;
        }
    return false;
}

void do_send(osjob_t* j) {
 //  Check if there is not a current TX/RX job running
  if (LMIC.opmode & OP_TXRXPEND) {
    Serial.println(F("OP_TXRXPEND, not sending"));
  } else {
    // Prepare upstream data transmission at the next possible time.
    LMIC_setTxData2(1, lpp.getBuffer(), lpp.getSize(), 0);
    Serial.println(F("Packet queued"));
  } 
}


void onEvent (ev_t ev) {
    Serial.print(os_getTime());
    Serial.print(": ");
    switch(ev) {
        case EV_SCAN_TIMEOUT:
            Serial.println(F("EV_SCAN_TIMEOUT"));
            break;
        case EV_BEACON_FOUND:
            Serial.println(F("EV_BEACON_FOUND"));
            break;
        case EV_BEACON_MISSED:
            Serial.println(F("EV_BEACON_MISSED"));
            break;
        case EV_BEACON_TRACKED:
            Serial.println(F("EV_BEACON_TRACKED"));
            break;
        case EV_JOINING:
            Serial.println(F("EV_JOINING"));
            break;
        case EV_JOINED:
            Serial.println(F("EV_JOINED"));
            break;
        case EV_RFU1:
            Serial.println(F("EV_RFU1"));
            break;
        case EV_JOIN_FAILED:
            Serial.println(F("EV_JOIN_FAILED"));
            break;
        case EV_REJOIN_FAILED:
            Serial.println(F("EV_REJOIN_FAILED"));
            break;
        case EV_TXCOMPLETE:
            Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));

            lora_loop = false;
            
            if (LMIC.txrxFlags & TXRX_ACK)
              Serial.println(F("Received ack"));
            if (LMIC.dataLen) {
              Serial.println(F("Received "));
              Serial.println(LMIC.dataLen);
              Serial.println(F(" bytes of payload"));
            }
            // Schedule next transmission
           // os_setTimedCallback(&sendjob, os_getTime()+sec2osticks(TX_INTERVAL), do_send);
            
            break;
        case EV_LOST_TSYNC:
            Serial.println(F("EV_LOST_TSYNC"));
            break;
        case EV_RESET:
            Serial.println(F("EV_RESET"));
            break;
        case EV_RXCOMPLETE:
            // data received in ping slot
            Serial.println(F("EV_RXCOMPLETE"));
            break;
        case EV_LINK_DEAD:
            Serial.println(F("EV_LINK_DEAD"));
            break;
        case EV_LINK_ALIVE:
            Serial.println(F("EV_LINK_ALIVE"));
            break;
         default:
            Serial.println(F("Unknown event"));
            break;
    }
}
