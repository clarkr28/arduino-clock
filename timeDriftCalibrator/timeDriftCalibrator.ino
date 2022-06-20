#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif


/* NeoPixel init */
#define PIN 21
Adafruit_NeoPixel strip = Adafruit_NeoPixel(72, PIN, NEO_GRBW + NEO_KHZ800);



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
  dc->cf = 0;
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



void setup()
{
  // 10 second countdown
  
  ClockState cs;
  // set colors to use for clock
  cs.hoursColor = strip.Color(255, 0, 0, 0); // red
  cs.minutesColor = strip.Color(0, 0, 255, 0); // blue
  cs.secondsColor = strip.Color(0, 255, 0, 0); // green 

  cs.hours = 11;
  cs.minutes = 59;
  for (int i = 50; i < 60; i++) {
    cs.seconds = i;
    displayTime(&cs);
    delay(1000);
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
  DriftCorrector dc;
  initDriftCorrector(&dc);
  TimeKeeper tk;
  ClockState cs;

  unsigned long startMillis = millis();
  setTime(&tk, (long long) startMillis, startMillis);

  // set colors to use for clock
  cs.hoursColor = strip.Color(255, 0, 0, 0); // red
  cs.minutesColor = strip.Color(0, 0, 255, 0); // blue
  cs.secondsColor = strip.Color(0, 255, 0, 0); // green 
  
  while(1) {

    // get the time
    char serialBuff[50];
    unsigned long ms = millisCorrected(&dc) - startMillis;
    int hours = getHours(&tk, ms);
    int minutes = getMinutes(&tk, ms);
    int seconds = getSecondsInt(&tk, ms);
    float fracSeconds = getFracSecond(&tk, ms);

    // update the clock
    cs.hours = hours;
    cs.minutes = minutes;
    cs.seconds = seconds;
    cs.fracSeconds = fracSeconds;
    displayTime(&cs);
    
    delay(5);
  }  
}
