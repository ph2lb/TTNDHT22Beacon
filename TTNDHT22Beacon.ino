/*  
 * ------------------------------------------------------------------------
 * "PH2LB LICENSE" (Revision 1) : (based on "THE BEER-WARE LICENSE" Rev 42) 
 * <lex@ph2lb.nl> wrote this file. As long as you retain this notice
 * you can do modify it as you please. It's Free for non commercial usage 
 * and education and if we meet some day, and you think this stuff is 
 * worth it, you can buy me a beer in return
 * Lex Bolkesteijn 
 * ------------------------------------------------------------------------ 
 * Filename : TTNDHT22Beacon.ino  
 * Version  : 1.0 (BETA)
 * ------------------------------------------------------------------------
 * Description : A low power DHT22 based datalogger for the ThingsNetwork.
 *  with deepsleep support and variable interval
 * ------------------------------------------------------------------------
 * Revision : 
 *  - 2016-nov-11 1.0 first "beta"
 * ------------------------------------------------------------------------
 * Hardware used : 
 *  - Arduino Pro-Mini 3.3V
 *  - RN2483
 *  - DHT22
 * ------------------------------------------------------------------------
 * Software used : 
 *  - Modified TheThingsNetwork library (for deepsleep support) check
 *    my github
 *  - DHT library
 *  - LowPower library
 * ------------------------------------------------------------------------ 
 * TODO LIST : 
 *  - add more sourcode comment
 * ------------------------------------------------------------------------ 
 * TheThingsNetwork Payload functions : 
 * 
 * DECODER : 
 * 
 * function (bytes) {
 * var batt = bytes[0] / 10.0;
 *
 * if (bytes.length >= 2)
 * {
 *   var humidity = bytes[1];
 * } 
 * if (bytes.length >= 3)
 * {
 *   var temperature = (((bytes[2] << 8) | bytes[3]) / 10.0) - 40.0;
 * } 
 * if (bytes.length >= 5)
 * { 
 *   var pkcount = bytes[4];
 *   var txresult = (bytes[5] & 0x0f);
 *   var txretrycount = (bytes[5] & 0xf0) >> 4;
 * }
 *
 */ 

#include "TheThingsNetwork.h"
#include <SoftwareSerial.h>
#include <DHT.h>
#include <LowPower.h>

// Set your AppEUI and AppKey

const byte appEui[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const byte appKey[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; 

SoftwareSerial loraSerial(10, 11); // RX, TX

#define debugSerial Serial

#define debugPrintLn(...) { if (debugSerial) debugSerial.println(__VA_ARGS__); }
#define debugPrint(...) { if (debugSerial) debugSerial.print(__VA_ARGS__); }

// define IO pins
#define LED         13  // D13 
#define RN2483RESET 12  // D12
#define BATTADC     3   // A03
#define DHTPIN      8   // D08
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321

#define BURSTINTERVAL   15    // 15 seconds
#define FASTINTERVAL    60    // 60 seconds
#define NORMALINTERVAL  900 // 15 minutes

// global variables
DHT dht(DHTPIN, DHTTYPE);
TheThingsNetwork ttn(loraSerial, debugSerial, TTN_FP_EU868 ); 
bool burstMode = false;
bool fastMode = false;
int interval = NORMALINTERVAL;  

// these variables are used for health check of the network
// like missing packages and prev transmitting result.
byte counter = 0;
byte prevresult = 0;

bool useLowPower = true;

// general functions
void led_on()
{
  digitalWrite(LED, HIGH);
}

void led_off()
{
  digitalWrite(LED, LOW);
} 

// the arduino setup
void setup()
{ 
  // allow the ttn client use autobaud on reset
  ttn.baudRate= 9600;
  // analogReference(EXTERNAL);
  //setup outputs
  pinMode(LED, OUTPUT); 

  led_on();
  
  debugSerial.begin(57600);
  loraSerial.begin(ttn.baudRate); 
  
  debugPrintLn(F("Startup"));

  //reset rn2483
  pinMode(RN2483RESET, OUTPUT);
  digitalWrite(RN2483RESET, LOW);
  delay(500);
  digitalWrite(RN2483RESET, HIGH);
  
  delay(1000); 

 // Set callback for incoming messages
  ttn.onMessage(onMessage);
 
  //the device will attempt a join every 5 second till the join is successfull
  while (!ttn.join(appEui, appKey)) {
    delay(5000);
  }

  led_off(); //turn on LED to confirm join
  // show status on debug.
  delay(5000);
  ttn.showStatus();
  debugPrintLn(F("Setup for The Things Network complete"));
  // give it a little time.
  delay(1000);
}

void SendPing()
{
  for (int i = 0; i < 4; i++)
  {
    // blinck 4 times to indicate that we will transmit.
    led_on();
    delay(250);
    led_off();
    delay(250);
  }
  // indicate that we are busy
  led_on(); 
  
  int batt = analogRead(BATTADC); // max 1023 = 6.6V/2 because ref = 3.3V resize to 0...66
  debugPrint(F("batt = "));
  debugPrintLn(batt);
  unsigned int batvaluetmp = batt * 66;
  batvaluetmp = batvaluetmp / 1023;
  byte batvalue = (byte)batvaluetmp; // no problem putting it into a int.
  debugPrint(F("batvalue = "));
  debugPrintLn(batvalue);  
 
  /*
    RH% bereik: 0-100% humidity
    Nauwkeurigheid: 2-5%
    Temperatuur bereik: -40 tot 80°C
    Nauwkeurigheid: ±0.5°C
  */

  float h_float = dht.readHumidity();
  float t_float = dht.readTemperature(); 
  
  int h = (int)h_float;
  int t = (int)((t_float + 40.0) * 10.0);
  // t = t + 40; // t [-40..+80] => [0..120]
  // t = t * 10; // t [0..120] => [0..1200]
  debugPrint(F("H = "));
  debugPrintLn(h);
  debugPrint(F("T = "));
  debugPrintLn(t);
  byte data[6];
  data[0] = batvalue;  
  data[1] = h & 0xFF;
  data[2] = t >> 8;
  data[3] = t & 0xFF; 
  data[4] = counter;
  data[5] = prevresult;

  // we try it 3 times
  for(int i=0; i<3; i++)
  {
    prevresult = (byte)(i<<4);  // store retry counter in high part of byte
    int result = ttn.sendBytes(data, sizeof(data));
    debugPrint(F("R = "));
    debugPrintLn(result);
    debugPrint(F("I = "));
    debugPrintLn(i);
    if (result < 0)
    {
       switch(result)
       {
          case -1 : prevresult = prevresult + 8; // send failed
            break;       
          case -2 : prevresult = prevresult + 9; // timeout
            break;       
          default : prevresult = prevresult + 10; // unkown error
            break;
       }
    }
    else 
    {
        prevresult = prevresult + (byte)result; // tx ok and / or rx
        break; // the for loop
    }
  }
  counter++;
  led_off();
}



void onMessage(const byte* payload, int length, int port) 
{
  if (length >= 1)
  {
    burstMode = false;
    fastMode = false;
    if (payload[0] == 0x01)
    {
        // start burst mode (every 60 seconds)
        debugSerial.println(F("Fastmode"));
        burstMode = false;
        fastMode = true;
    }
    else if (payload[0] == 0x02)
    {
       // start burst mode (every 15 seconds)
        debugSerial.println(F("Burstmode"));
        burstMode = true;
        fastMode = false;
    }
    else
    {
        // start normal mode (every 15 minutes) 
        debugSerial.println(F("Normalmode")); 
        burstMode = false;
        fastMode = false;
    }
  } 
}


// the loop routine runs over and over again forever:
void loop()
{ 
  // send ping (batt, temp, humd)
  SendPing(); 

  // calculate interval based on if burstmode or not.
  if (burstMode)
    interval = BURSTINTERVAL;
  else if (fastMode)
    interval = FASTINTERVAL;
  else 
    interval = NORMALINTERVAL;

  if (useLowPower)
  {  
    long sleepTime = ((long)interval + 10) * 1000;
    ttn.deepSleep(sleepTime);
  }

  
  
  for (int i = 0; i < interval; i++)
  { 
    if (useLowPower)
    { 
      i +=7 ; // no normal 1 second run but 8 second loops m.      
      // Enter power down state for 8 s with ADC and BOD module disabled
      LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);   
    }
    else
    {
      led_on();
      delay(200);
      led_off();
      delay(800); 
    } 
  } 
  if (useLowPower)
  { 
    ttn.wakeUp();
  }
}

