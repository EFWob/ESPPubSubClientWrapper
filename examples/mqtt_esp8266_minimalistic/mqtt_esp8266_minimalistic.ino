#include <ESP8266WiFi.h>
#include <ESPPubSubClientWrapper.h>

const char* ssid = "..........";				// Put your WiFi credentials here
const char* password = "..........";
const char* mqtt_server = "broker.mqtt-dashboard.com";

ESPPubSubClientWrapper client(mqtt_server);

  
  
void callbackHello(char* topic, byte* payload, unsigned int length) {
  Serial.println("Message ""hello"" received");
}

void callbackWorld(char* topic, byte* payload, unsigned int length) {
  Serial.println("Message ""world"" received");
  if ((length == 5) && (0 == strncmp((const char *)payload,"hello", 5))) 
    client.publish("world", "is listening to your hello!"); 
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

