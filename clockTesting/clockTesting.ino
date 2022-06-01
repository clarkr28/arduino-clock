#include "WiFi.h"
#include "AsyncUDP.h"

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


/** TIME STRUCT */
struct TimeKeeper {
  long long msFileTime;
  unsigned long msSinceBoot;
  unsigned long msCheckpoint; // ms today when the time was last retrieved
};

void setTime(TimeKeeper* tk, long long fileTime, unsigned long bootTime) {
  tk->msFileTime = fileTime;
  tk->msSinceBoot = bootTime;
  tk->msCheckpoint = (unsigned long) (fileTime % (long long) (1000 * 60 * 60 * 24));
}

unsigned long getMsToday(TimeKeeper* tk, unsigned long msCurr) {
  return tk->msCheckpoint + msCurr - tk->msSinceBoot; 
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

          if (VERBOSE_TIME || VERBOSE_WIFI) {
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

void loop()
{
  TimeKeeper tk;
  updateTime(&tk);
  
  while(1) {
    updateTime(&tk);

    for (int i=0; i < 500; i++) {
      char serialBuff[50];
      unsigned long ms = millis();
      int hours = getHours(&tk, ms);
      int minutes = getMinutes(&tk, ms);
      int seconds = getSecondsInt(&tk, ms);
      sprintf(serialBuff, "%02i:%02i:%02i", hours, minutes, seconds);
      Serial.println(serialBuff);
      delay(500);
    }
  }  
}
