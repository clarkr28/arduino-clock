#include "WiFi.h"
#include "AsyncUDP.h"
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif


/* NeoPixel init */
#define PIN 21
Adafruit_NeoPixel strip = Adafruit_NeoPixel(72, PIN, NEO_GRBW + NEO_KHZ800);


/* wifi init */
const char * ssid = "ClarkWifi";
const char * password = "Is it basketball season yet?";
int     HTTP_PORT     = 80;
String  HTTP_METHOD   = "GET";
char    HOST_NAME[]   = "worldclockapi.com";
String  PATH_NAME     = "/api/json/cst/now";
int     VERBOSE_WIFI  = 0;
int     VERBOSE_TIME  = 1;
WiFiClient client;
AsyncUDP udp;



// _______________ Struct Definitions_______________


/** TIME STRUCT */
struct TimeKeeper {
  long long msFileTime;
  unsigned long msAtUpdate;
  unsigned long msCheckpoint; // ms today when the time was last synced
};


struct ClockState {
  int hours;
  int minutes;
  int seconds;
  float fracSeconds;
  uint32_t hoursColor;
  uint32_t minutesColor;
  uint32_t secondsColor;
};


struct DriftCorrector {
  
  // the time millis gives when the device is powered on
  unsigned long startTime;
  
  // correction factor: ms to add per minute
  double cf; 
};



// _______________ DriftCorrector Section_______________

/**
 * Initialize the drift corrector with the current time and the
 * correction factor. Call this right after boot
 * dc: DriftCorrector reference to initialize
 */
void initDriftCorrector(DriftCorrector* dc) {
  dc.startTime = millis();
  dc.cf = 0;
  return &dc;
}



// _______________ TimeKeeper Section _______________

/**
 * Update TimeKeeper after sync with internet
 * tk: TimeKeeper ref to save persistent values
 * fileTime: time starting at Jan 1, 1601
 * millisTime: milliseconds arduino has been running (millis function)
 */
void setTime(TimeKeeper* tk, long long fileTime, unsigned long millisTime) {
  tk->msFileTime = fileTime;
  tk->msAtUpdate = millisTime;
  tk->msCheckpoint = (unsigned long) (fileTime % (long long) (1000 * 60 * 60 * 24));
}

unsigned long getMsToday(TimeKeeper* tk, unsigned long msCurr) {
  return tk->msCheckpoint + msCurr - tk->msAtUpdate; 
}

int getHours(TimeKeeper* tk, unsigned long msCurr) {
  unsigned long msToday = getMsToday(tk, msCurr);
  return (int) (msToday / (3600 * 1000));
}

int getMinutes(TimeKeeper* tk, unsigned long msCurr) {
  unsigned long msToday = getMsToday(tk, msCurr);
  return (int) (msToday % (3600 * 1000) / (60 * 1000));
}

int getSecondsInt(TimeKeeper* tk, unsigned long msCurr) {
  unsigned long msToday = getMsToday(tk, msCurr);
  unsigned long msThisMinute = msToday % (60 * 1000);
  return (int) msThisMinute / 1000;
}

/* If the time is 4:15:03.123, this returns 03.123 */
float getFracSecond(TimeKeeper* tk, unsigned long msCurr) {
  unsigned long msToday = getMsToday(tk, msCurr);
  unsigned long msThisMinute = msToday % (60 * 1000);
  return (float) msThisMinute / 1000.0; 
}


void updateTime(TimeKeeper* tk) {
  if (client.connect(HOST_NAME, HTTP_PORT)) {
    if (VERBOSE_WIFI) {
      Serial.println("Connected");  
    }
    // make and send HTTP request
    client.println(HTTP_METHOD + " " + PATH_NAME + " HTTP/1.1");
    client.println("Host: " + String(HOST_NAME));
    client.println("Connection: close");
    client.println(); // end HTTP header

    char buff[1000];
    while(client.connected()) {
      unsigned long arduinoTime = millis();
      if(client.available()){
        // read an incoming byte from the server and print it to serial monitor:
        //char c = client.read();
        int size = client.read((uint8_t*) buff, 999);
        buff[size] = '\0';
        if (VERBOSE_WIFI) {
          Serial.print(buff);
          Serial.print("\n\n");  
        }        

        char timeKey[] = "currentFileTime\":";
        char* timePosition = strstr(buff, timeKey) + strlen(timeKey);
        if (timePosition != NULL) {
          char* timeEndPosition = strchr(timePosition, ',');
          int stringLength = timeEndPosition - timePosition + 1 - 4; // leaves sec.000
          char timeString[stringLength];
          strncpy(timeString, timePosition, stringLength - 1);
          timeString[stringLength - 1] = '\0';

          setTime(tk, atoll(timeString), arduinoTime);

          if (VERBOSE_WIFI) {
            Serial.println("found the time!");
            Serial.println(timePosition);
            Serial.println(timeString);
            Serial.println(tk->msCheckpoint);
            Serial.println();  
          }
        }
      }
    }

    // the server's disconnected, stop the client:
    client.stop();
    if (VERBOSE_WIFI) {
      Serial.println();
      Serial.println("disconnected");  
    }    
  } else {// if not connected:
    if (VERBOSE_WIFI) {
      Serial.println("connection failed");  
    }
  }
}


void setup()
{
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("WiFi Failed");    
    while(1) {
        delay(1000);
    }
  }
}


void displayTime(ClockState* cs) {
  
  // clear the display
  uint32_t off = strip.Color(0, 0, 0, 0);
  for (int i=0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, off); 
  }

  // set the overlays
  uint32_t lightWhite = strip.Color(0, 0, 0, 5);
  strip.setPixelColor(0, lightWhite);
  // set minutes overlays
  for (int i=0; i < 60; i+=15) {
    strip.setPixelColor(i + 12, lightWhite);    
  }

  // set the time
  // remember the hours is set in 24 hour format
  strip.setPixelColor(cs->hours % 12, cs->hoursColor);
  // remember the minutes loop comes after the 12 hours pixels
  strip.setPixelColor(cs->minutes + 12, cs->minutesColor);
  strip.setPixelColor(cs->seconds + 12, cs->secondsColor);
  strip.show();
}


void loop()
{
  TimeKeeper tk;
  updateTime(&tk);
  ClockState cs;

  // set colors to use for clock
  cs.hoursColor = strip.Color(255, 0, 0, 0); // red
  cs.minutesColor = strip.Color(0, 0, 255, 0); // blue
  cs.secondsColor = strip.Color(0, 255, 0, 0); // green 
  
  while(1) {
    updateTime(&tk);

    for (int i=0; i < 300; i++) {
      // get the time
      char serialBuff[50];
      unsigned long ms = millis();
      int hours = getHours(&tk, ms);
      int minutes = getMinutes(&tk, ms);
      int seconds = getSecondsInt(&tk, ms);
      float fracSeconds = getFracSecond(&tk, ms);
      if (VERBOSE_TIME) {
        sprintf(serialBuff, "%02i:%02i:%02i %f", hours, minutes, seconds, fracSeconds);
        Serial.println(serialBuff);
      }

      // update the clock
      cs.hours = hours;
      cs.minutes = minutes;
      cs.seconds = seconds;
      cs.fracSeconds = fracSeconds;
      displayTime(&cs);
      
      delay(100);
    }
  }  
}
