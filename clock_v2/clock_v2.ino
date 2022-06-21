#include "WiFi.h"
#include "AsyncUDP.h"
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif
#include "ClockConstants.h"


/* NeoPixel init */
#define PIN 21
Adafruit_NeoPixel strip = Adafruit_NeoPixel(72, PIN, NEO_GRBW + NEO_KHZ800);
int     LOG_BLUR = 0;


/* wifi init */
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
  dc->startTime = millis();
  dc->cf = 1;
}

/**
 * a drift correction wrapper for millis
 */
unsigned long millisCorrected(DriftCorrector* dc) {
  long millisTime = (long) millis();
  
  // make sure drift corrector is initialized
  if (dc->startTime == 0 || dc->cf == 0 || dc->startTime > millisTime) {
    return (unsigned long) millisTime;
  }

  unsigned long correction = floor((( (double) millisTime - dc->startTime) / 60000) * dc->cf);
  return (unsigned long) millisTime + correction;
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


void updateTime(TimeKeeper* tk, DriftCorrector* dc) {
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
      unsigned long arduinoTime = millisCorrected(dc);
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


// _______________ Clock Display Section _______________

/**
 * Display the time on the two rings of LEDs
 * cs: The clock state to display
 * blurSecond: true if the second and minute transitions should be blurred
 */
void displayTime(ClockState* cs, bool blur) {
  
  // clear the display
  strip.clear();

  // set the overlays
  uint32_t lightWhite = strip.Color(0, 0, 0, 5);
  mergePixelColor(0, lightWhite);
  // set minutes overlays
  for (int i=0; i < 60; i+=15) {
    mergePixelColor(i + 12, lightWhite);    
  }

  // set the hours
  // remember the hours is set in 24 hour format
  mergePixelColor(cs->hours % 12, cs->hoursColor);
  

  if (blur) {
    if (LOG_BLUR) {
      Serial.println();
    }

    /* blur the second over a couple pixels */
    int currSecond = cs->seconds;
    
    for (int i = currSecond - 3; i < currSecond + 5; i++) {
      int ledIndex = i + 12;
      if (ledIndex > 71) {
        ledIndex = ledIndex % 72 + 12;
      } 
      else if (ledIndex < 12) {
        ledIndex += 60;
      }

      // adding 0.00001 avoids the edge case where fracSeconds
      // and i are exactly equal (resulting with a distance of zero)
      float distance = abs(cs->fracSeconds - (float) i) + 0.00001;
      if (distance > 2.5) {
        distance = 0;
      }

      int pixelValue;
      if (distance > 0) {
        distance = 255 * distance / 2.5;
        pixelValue = 255 - (int) distance;
        if (pixelValue < 0) {
          pixelValue = 0;
        }
      } 
      else {
        pixelValue = 0;
      }

      if (LOG_BLUR) {
        char buff[50];
        sprintf(buff, "%02i %02i %i", currSecond, i, pixelValue);
        Serial.println(buff);
      }
      
      mergePixelColor(ledIndex, strip.Color(0, 0, pixelValue, 0));
    }

    /* blur the minute in and out if necessary */
    if (cs->seconds == 59 || cs->seconds == 58) {
      // blur the minte out for the last second
      double fracPart, intPart;
      fracPart = modf(cs->fracSeconds, &intPart);
      if (cs->seconds == 59) {
        fracPart += 1.0;
      }
      int pixelValue = 255 - ceil(fracPart / 2 * 255);
      mergePixelColor(cs->minutes + 12, strip.Color(0, pixelValue, 0, 0));
    } 
    else if (cs->seconds == 0 || cs->seconds == 1) {
      // blur the minute in for the first second
      double fracPart, intPart;
      fracPart = modf(cs->fracSeconds, &intPart);
      if (cs->seconds == 1) {
        fracPart += 1.0;
      }
      int pixelValue = ceil(fracPart / 2 * 255);
      mergePixelColor(cs->minutes + 12, strip.Color(0, pixelValue, 0, 0));
    } 
    else {
      mergePixelColor(cs->minutes + 12, cs->minutesColor);
    }
  } 
  else {
    
    // display the seconds/minutes in a binary method on a single pixel
    // add 12 because the mintutes/seconds circle comes after the hours circle
    mergePixelColor(cs->seconds + 12, cs->secondsColor);
    mergePixelColor(cs->minutes + 12, cs->minutesColor);
    
  }
  strip.show();
}


/**
 * Merge the passed color with the color already at that index
 */
void mergePixelColor(int index, uint32_t newColor) {
  uint32_t existingColor = strip.getPixelColor(index);
  existingColor = existingColor | newColor;
  strip.setPixelColor(index, existingColor);
}


void setup()
{
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(gWifiName, gWifiPassword);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("WiFi Failed");    
    while(1) {
        delay(1000);
    }
  }
}



void loop()
{
  DriftCorrector dc;
  initDriftCorrector(&dc);
  TimeKeeper tk;
  updateTime(&tk, &dc);
  ClockState cs;

  // set colors to use for clock
  cs.hoursColor = strip.Color(255, 0, 0, 0); // red
  cs.minutesColor = strip.Color(0, 255, 0, 0); // green
  cs.secondsColor = strip.Color(0, 0, 255, 0); // blue
  
  while(1) {
    updateTime(&tk, &dc);

    for (int i=0; i < 20000; i++) {
      // get the time
      char serialBuff[50];
      unsigned long ms = millisCorrected(&dc);
      int hours = getHours(&tk, ms);
      int minutes = getMinutes(&tk, ms);
      int seconds = getSecondsInt(&tk, ms);
      float fracSeconds = getFracSecond(&tk, ms);
      if (VERBOSE_TIME && i % 25 == 0) {
        sprintf(serialBuff, "%02i:%02i:%02i %f", hours, minutes, seconds, fracSeconds);
        Serial.println(serialBuff);
      }

      // update the clock
      cs.hours = hours;
      cs.minutes = minutes;
      cs.seconds = seconds;
      cs.fracSeconds = fracSeconds;
      displayTime(&cs, true);
      
      delay(20);
    }
  }  
}
