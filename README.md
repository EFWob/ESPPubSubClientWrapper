# Arduino ESP8266/ESP32 Client for MQTT using PubSubClient-Library

This library provides a client for doing simple publish/subscribe messaging with
a server that supports MQTT. It wraps the functionality of the PubSubClient-Library
by deriving a wrapper class from PubSubClient class.

The main additions to the base class are:
 * the `on()` method that allows a link of a specific topic to a specific callback (i. e. you
    no longer have to parse the incoming topics to link them to specific actions in your sketch).
 * The connection/reconnection is handled whithin the `loop()` method. That means you no longer need
    to wait for WiFi-layer to be established before connecting to the MQTT-Server. 

## Basic usage and deviations to PubSubClient

 * Instead of "PubSubClient.h" include "ESPPubSubClientWrapper.h"
 * Instead of class `PubSubClient` instantiate a client Object from class `ESPPubSubClientWrapper`
 * There are only two ways of constructing an `ESPPubSubClientWrapper` object:
	* `ESPPubSubClientWrapper clientName(const char *domain, uint16_t port)` to domain name of
	    host as in i. e. "broker.mqtt-dashboard.com"
	*  `ESPPubSubClientWrapper(uint8_t *ipaddr, uint16_t port)` to specify the IP4-address of 
		the host as in i. e. {192, 168, 4, 1}
	* in both cases the parameter port can be omitted and defaults to 1883.
 * The methods setServer(), setClient() and setStream() must not be used.
 * The methods setCallback() and subscribe() can be used as usual, but using the method on() is
   often the better choice as it allows a direct link of a specific topic to a specific callback.
 * The API for the method on() is similar to the `subscribe()` method. The parameters are the same:
	`ESPPubSubClientWrapper& on(const char* topic, MQTT_CALLBACK_SIGNATURE, uint8_t qos = 0);` however the return value is 
	a reference to the calling object thus allowing to chain multiple calls to `on()`.
 * The topics used for calling `on()` can include the MQTT-Wildcards `#` and `+`.
 * It is possible that an incoming topic matches to more than one callback. Consider as example `client.on("test/#", 
	callback1);client.on("#", callback2);`. The second topic subscription matches also topics that match the first call 
	to `on()`. In that case not all matching callbacks fire but the order of subscription is relevant. In the given example, 
    each incoming message that matches `"test/#"` will fire `callback1()` while all other topics are matched to `callback2()`.
   	
	

## Minimalistic Example

The following is a minimalistic example (for ESP8266)


`#include <ESP8266WiFi.h>
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
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  client.on("hello", callbackHello);
  client.on("world", callbackWorld);  
}

void loop() {
  client.loop();
}`


## Limitations

 * Usual limitations of PubSubClient library apply.
 * Do not use the set-methods established by base class except `setCallback()`. I. e. do not use
	* `PubSubClient& setServer(uint8_t * ip, uint16_t port);`
	* `PubSubClient& setServer(const char * domain, uint16_t port);`
	* `PubSubClient& setClient(Client& client);`
	* `PubSubClient& setStream(Stream& stream);`


## Compatible Hardware

 - ESP8266
 - ESP32

## License

This code is released under the MIT License.
