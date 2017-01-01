// Notes: see http://www.home-automation-community.com/arduino-low-power-how-to-run-atmega328p-for-a-year-on-coin-cell-battery/
//        and https://github.com/rocketscream/Low-Power
//        and http://donalmorrissey.blogspot.com.au/2010/04/sleeping-arduino-part-5-wake-up-via.html
//        and http://forum.arduino.cc/index.php?topic=173850.0
// but mostly http://www.gammon.com.au/power

// Following written for Seeeduino Stalker v2.3 http://www.seeedstudio.com/wiki/Seeeduino_Stalker_v2.3
// with XBee Radio, solar panel and 3v3 1200mAh LiPo

//  Arduino Pro or Pro Mini (3.3V, 8MHz) w/ ATmega 328

// add SDCard logging
// add DEFINE for number of seconds sleep (currently only 8 seconds [max time on AVR] is supported
#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/wdt.h>


// CONNECTIONS:
// DS3231 SDA --> SDA
// DS3231 SCL --> SCL
// DS3231 VCC --> 3.3v or 5v
// DS3231 GND --> GND

#if defined(ESP8266)
#include <pgmspace.h>
#else
#include <avr/pgmspace.h>
#endif
#include <Wire.h>  // must be incuded here so that Arduino library object file references work
#include <RtcDS3231.h>

#include <Battery.h>

#include <SPI.h>
#include <SD.h>

#define countof(a) (sizeof(a) / sizeof(a[0]))

float voltage;
int BatteryValue, battlibchcnt = 0;
// until I work out how to properly drive XBees....
const int XBEE_SYNC_DELAY = 10000;
// stalker has CS on pin 10 (or 9, configurable)
const int chipSelect = 10;

long currentmillis = 0, days = 0, hours = 0, mins = 0, secs = 0;

RtcDS3231 Rtc;
RtcDateTime STARTTIME ;
Battery battery;

char CH_status_print[][4] =
{
  "off", "on", "ok", "err"
};

// watchdog intervals
// sleep bit patterns for WDTCSR
enum
{
  WDT_16_MS  =  0b000000,
  WDT_32_MS  =  0b000001,
  WDT_64_MS  =  0b000010,
  WDT_128_MS =  0b000011,
  WDT_256_MS =  0b000100,
  WDT_512_MS =  0b000101,
  WDT_1_SEC  =  0b000110,
  WDT_2_SEC  =  0b000111,
  WDT_4_SEC  =  0b100000,
  WDT_8_SEC  =  0b100001,
};  // end of WDT intervals enum


/***************************************************
    Name:        ISR(WDT_vect)
    Returns:     Nothing.
    Parameters:  None.
    Description: Watchdog Interrupt Service. This
                 is executed when watchdog timed out.

 ***************************************************/
ISR (WDT_vect)
{
  wdt_disable();  // disable watchdog
}  // end of WDT_vect

/***************************************************
    Name:        myWatchdogEnable
    Returns:     Nothing.
    Parameters:  None.
    Description: Enters the arduino into sleep mode.
 ***************************************************/
void myWatchdogEnable(int sleepTime)
{
  delay(XBEE_SYNC_DELAY);
  // disable ADC
  ADCSRA = 0;

  // clear various "reset" flags
  MCUSR = 0;
  // allow changes, disable reset
  WDTCSR = bit (WDCE) | bit (WDE);
  // set interrupt mode and an interval
  // need to process 'sleepTime' into values
  WDTCSR = bit (WDIE) | WDT_8_SEC;    // set WDIE, and 8 seconds delay
  wdt_reset();  // pat the dog

  // ready to sleep
  set_sleep_mode (SLEEP_MODE_PWR_DOWN);
  noInterrupts ();           // timed sequence follows
  sleep_enable();

  // turn off brown-out enable in software
  // BODS must be set to one and BODSE must be set to zero within four clock cycles
  MCUCR = bit (BODS) | bit (BODSE);

  // The BODS bit is automatically cleared after three clock cycles
  MCUCR = bit (BODS);

  // We are guaranteed that the sleep_cpu call will be done
  // as the processor executes the next instruction after
  // interrupts are turned on.
  interrupts ();  // one cycle
  sleep_cpu ();   // one cycle

  // cancel sleep as a precaution
  sleep_disable();
}

void setup ()
{
  Serial.begin(57600);
  analogReference(INTERNAL);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  Serial.println();
  Serial.print(F("compiled: "));
  Serial.print(__DATE__);
  Serial.println(__TIME__);

  //--------RTC SETUP ------------
  Rtc.Begin();

  // if you are using ESP-01 then uncomment the line below to reset the pins to
  // the available pins for SDA, SCL
  // Wire.begin(0, 2); // due to limited pins, use pin 0 and 2 for SDA, SCL

  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  String  readable = dateString(compiled);
  Serial.print(readable);
  Serial.println();

  if (!Rtc.IsDateTimeValid())
  {
    // Common Cuases:
    //    1) first time you ran and the device wasn't running yet
    //    2) the battery on the device is low or even missing

    Serial.println(F("RTC lost confidence in the DateTime!"));

    // following line sets the RTC to the date & time this sketch was compiled
    // it will also reset the valid flag internally unless the Rtc device is
    // having an issue

    Rtc.SetDateTime(compiled);
  }

  if (!Rtc.GetIsRunning())
  {
    Serial.println(F("RTC was not actively running, starting now"));
    Rtc.SetIsRunning(true);
  }

  RtcDateTime now = Rtc.GetDateTime();
  if (now < compiled)
  {
    Serial.println(F("RTC is older than compile time!  (Updating DateTime)"));
    Rtc.SetDateTime(compiled);
  }
  else if (now > compiled)
  {
    Serial.println(F("RTC is newer than compile time. (this is expected)"));
  }
  else if (now == compiled)
  {
    Serial.println("RTC is the same as compile time! (not expected but all is fine)");
  }
  STARTTIME = Rtc.GetDateTime();
  // never assume the Rtc was last configured by you, so
  // just clear them to your needed state
  Rtc.Enable32kHzPin(false);
  Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeNone);


  Serial.print(F("Init SD card..."));

  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println(F("Card failed, or not present"));
    // don't do anything more:
    return;
  }
  Serial.println(F("card init"));

  //header
  File dataFile = SD.open("datalog.txt", FILE_WRITE);

  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println("on,now,temp,raw voltage,charge status,BL charge status,BL voltage,BL percentage,BL charge count,runtime ms, runtime string");
    dataFile.close();
    // print to the serial port too:
    Serial.println("on,now,temp,raw voltage,charge status,BL charge status,BL voltage,BL percentage,BL charge count,runtime ms,runtime string");
  }
  // if the file isn't open, pop up an error:
  else {
    Serial.println(F("error opening datalog.txt"));
  }
  Serial.flush();
}

void loop ()
{
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  if (!Rtc.IsDateTimeValid())
  {
    // Common Cuases:
    //    1) the battery on the device is low or even missing and the power line was disconnected
    Serial.println("RTC lost confidence in the DateTime!");
  }

  RtcDateTime now = Rtc.GetDateTime();
  RtcTemperature temp = Rtc.GetTemperature();
  battery.update();
  unsigned char CHstatus = read_charge_status();//read the charge status
  char* battlibCS = battery.getChStatus();

  bool battlibch = battery.isCharging();

  float battlibvoltage = battery.getVoltage();
  int battlibpercentage = battery.getPercentage();

  // make a string for assembling the data to log:
  String dataString = "";

  BatteryValue = analogRead(A7);
  voltage = BatteryValue * (1.1 / 1024) * (10 + 2) / 2; //Voltage devider
  if (battlibch) battlibchcnt++;
  dataString += (dateString(STARTTIME));
  dataString += (F(","));

  dataString += (dateString(now));
  dataString += (F(","));

  dataString += (temp.AsFloat());
  dataString += (F(","));

  dataString += (voltage);
  dataString += (F(","));

  dataString += (CH_status_print[CHstatus]);
  dataString += (F(","));

  // Battery Library stuff
  dataString += (battlibCS);
  dataString += (F(","));

  dataString += (battlibvoltage);
  dataString += (F(","));

  dataString += (battlibpercentage);
  dataString += (F(","));

  dataString += (battlibchcnt);
  dataString += (F(","));

  currentmillis = millis(); // get the current milliseconds from arduino
  // report milliseconds
  dataString += (currentmillis);
  dataString += (F(","));

  secs = currentmillis / 1000; //convect milliseconds to seconds
  mins = secs / 60; //convert seconds to minutes
  hours = mins / 60; //convert minutes to hours
  days = hours / 24; //convert hours to days
  secs = secs - (mins * 60); //subtract the coverted seconds to minutes in order to display 59 secs max
  mins = mins - (hours * 60); //subtract the coverted minutes to hours in order to display 59 minutes max
  hours = hours - (days * 24); //subtract the coverted hours to days in order to display 23 hours max
  //Display results
  if (days > 0) // days will displayed only if value is greater than zero
  {
    dataString += (days);
    dataString += (F(" days, "));
  }
  dataString += (hours);
  dataString += (F(":"));
  dataString += (mins);
  dataString += (F(":"));
  dataString += (secs);


  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  File dataFile = SD.open("datalog.txt", FILE_WRITE);

  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(dataString);
    dataFile.close();
    // print to the serial port too:
    Serial.println(dataString);
  }
  // if the file isn't open, pop up an error:
  else {
    Serial.println(F("error opening datalog.txt"));
  }
 
  // sleep for a total of 10 minutes (75 x 8 seconds)
  // thought I'd rather we keep everything disabled during the loop
  // for now... this'll do
  for (int i = 0; i < 75; i++)
    myWatchdogEnable(0);
}  // end of loop

String dateString (const RtcDateTime& dt)
{
  char datestring[20];

  snprintf_P(datestring,
             countof(datestring),
             PSTR("%04u/%02u/%02u %02u:%02u:%02u"),
             dt.Year(),
             dt.Month(),
             dt.Day(),
             dt.Hour(),
             dt.Minute(),
             dt.Second() );
  return (datestring);
}

unsigned char read_charge_status(void)
{
  unsigned char CH_Status = 0;
  unsigned int ADC6 = analogRead(6);
  if (ADC6 > 900)
  {
    CH_Status = 0;//sleeping
  }
  else if (ADC6 > 550)
  {
    CH_Status = 1;//charging
  }
  else if (ADC6 > 350)
  {
    CH_Status = 2;//done
  }
  else
  {
    CH_Status = 3;//error
  }
  return CH_Status;
}

