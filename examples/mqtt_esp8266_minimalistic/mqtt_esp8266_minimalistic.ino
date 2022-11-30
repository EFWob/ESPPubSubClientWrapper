#include <ESP8266WiFi.h>
#include <ESPPubSubClientWrapper.h>

const char* ssid = "..........";				// Put your WiFi credentials here
const char* password = "..........";
const char* mqtt_server = "broker.mqtt-dashboard.com";

ESPPubSubClientWrapper client(mqtt_server);

  
  
void callbackHello(char* topic, char * payload) {
  Serial.println("\r\nMessage ""hello"" received");
  if (payload)
  {
    Serial.printf("Payload-len=%d, Payload=\"%s\"\r\n", strlen(payload), payload);
  }
  else
    Serial.println("Payload is NULL.");
}

void callbackWorld(char* topic, uint8_t* payload, unsigned int payloadLen) {
  Serial.println("\r\nMessage ""world"" received");
  if (payload)
  {
    char s[payloadLen + 1];
    memcpy(s, payload, payloadLen);
    s[payloadLen] = '\0';
    Serial.printf("Payload-len=%d, Payload=\"%s\"\r\n", payloadLen, s);
  }
  else
    Serial.println("Payload is NULL.");

}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  client.on("hello", callbackHello);
  client.on("world", callbackWorld);  
}

void loop() {
  client.loop();
}

