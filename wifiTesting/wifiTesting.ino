#include "WiFi.h"
#include "AsyncUDP.h"
#include "ClockConstants.h"

int     HTTP_PORT     = 80;
String  HTTP_METHOD   = "GET";
char    HOST_NAME[]   = "worldclockapi.com";
String  PATH_NAME     = "/api/json/est/now";

WiFiClient client;

AsyncUDP udp;

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
  long long currentTime = 0, previousTime = 0;
  while(1) {
  
    if (client.connect(HOST_NAME, HTTP_PORT)) {
      Serial.println("Connected");
      // make and send HTTP request
      client.println(HTTP_METHOD + " " + PATH_NAME + " HTTP/1.1");
      client.println("Host: " + String(HOST_NAME));
      client.println("Connection: close");
      client.println(); // end HTTP header
  
      char buff[1000];
      while(client.connected()) {
        if(client.available()){
          // read an incoming byte from the server and print it to serial monitor:
          //char c = client.read();
          int size = client.read((uint8_t*) buff, 1000);
          buff[size] = '\0';
          Serial.print(buff);
          Serial.print("\n\n");
  
          char timeKey[] = "currentFileTime\":";
          char* timePosition = strstr(buff, timeKey) + strlen(timeKey);
          if (timePosition != NULL) {
            char* timeEndPosition = strchr(timePosition, ',');
            int stringLength = timeEndPosition - timePosition + 1 - 5; // leaves sec.00
            char timeString[stringLength];
            strncpy(timeString, timePosition, stringLength - 1);
            timeString[stringLength - 1] = '\0';

            previousTime = currentTime;
            currentTime = atoll(timeString);
            
            Serial.println("found the time!");
            Serial.println(timePosition);
            Serial.println(timeString);
            Serial.println(currentTime);
            Serial.println(currentTime - previousTime);
            Serial.println();
          }
        }
      }
  
      // the server's disconnected, stop the client:
      client.stop();
      Serial.println();
      Serial.println("disconnected");
    } else {// if not connected:
      Serial.println("connection failed");
    }
  
    delay(10000);
  }  
}
