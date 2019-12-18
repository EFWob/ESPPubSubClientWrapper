#ifndef ESPPubSubClient_h
#define ESPPubSubClient_h
#include <BasicStatemachine.h>
#include <PubSubClient.h>
#include <deque>
#if defined(ESP8266)
#include <ESP8266WiFi.h>
//#include <ESP8266WebServer.h>
#endif
#define QUEUE_CALLBACKS
#define PUBLISH_WAITCONNECTED
#if defined(ESP32)
#include <vector>
#include <WiFi.h>
//#include <WebServer.h>
#endif
class onEventItem;
class PendingCallbackItem;
class WaitingPublishItem;


class ESPPubSubClient : public BasicStatemachine, public PubSubClient {
  public:
	ESPPubSubClient(char *domain, uint16_t port = 1883);
	ESPPubSubClient(uint8_t *ipaddr, uint16_t port = 1883);
//	PubSubClient* getClient();
//	bool connect();
//	bool connected();
	void setConnect(const char* id);
	void setConnect(const char* id, const char* user, const char* pass);
	void setConnect(const char* id, const char* willTopic, uint8_t willQos, boolean willRetain, const char* willMessage);
	void setConnect(const char* id, const char* user, const char* pass, const char* willTopic, uint8_t willQos, boolean willRetain, const char* willMessage);
	void setConnect(const char* id, const char* user, const char* pass, const char* willTopic, uint8_t willQos, boolean willRetain, const char* willMessage, boolean cleanSession);
	void onEnter(int16_t currentStateNb, int16_t oldStateNb); 
	void on(char* topic, MQTT_CALLBACK_SIGNATURE, uint8_t qos = 0);
	boolean publish_waitConnected(const char* topic, const char* payload);
	boolean publish_waitConnected(const char* topic, const char* payload, boolean retained);
	boolean publish_waitConnected(const char* topic, const uint8_t * payload, unsigned int plength);
	boolean publish_waitConnected(const char* topic, const uint8_t * payload, unsigned int plength, boolean retained);
	void loop();
   protected:
    void receivedCallback(char* topic, byte* payload, unsigned int length); 
    void runState(int16_t stateNb);
	char* _clientID = NULL;
//	PubSubClient *_client = NULL;
	WiFiClient _wiFiClient;
	bool _firstRetry = false;
	int _subscribed = 0;
	char* _connect_id = NULL;
	char* _connect_user = NULL;
	char* _connect_pass = NULL;
	char* _connect_willTopic = NULL;
	uint8_t _connect_willQos = 0;
	bool _connect_willRetain = false;
	char* _connect_willMessage = NULL;
	bool _connect_cleanSession = true;
	std::vector<onEventItem *> _onEvents ;              
#if defined(QUEUE_CALLBACKS)	
	std::deque<PendingCallbackItem *>  _pendingCallbacks;
#endif	
#if defined(PUBLISH_WAITCONNECTED)
	std::deque<WaitingPublishItem *>  _waitingPublishs;
	
#endif
};


class onEventItem {
public:
	onEventItem(char *t, MQTT_CALLBACK_SIGNATURE, uint8_t qos);
	boolean topicMatch(char *value);
	char * topic;
	uint16_t hasHash;
	bool hasLevelWildcard;
	uint8_t qos;
	bool subscribed;
	MQTT_CALLBACK_SIGNATURE;
};

#if defined(QUEUE_CALLBACKS)
class PendingCallbackItem {
public:
	PendingCallbackItem(char *topic, uint8_t *payload, unsigned int payloadLen, MQTT_CALLBACK_SIGNATURE) {
		_topic = strdup(topic);
		if (payload && payloadLen) {
			_payload = (uint8_t *)malloc(payloadLen);
			if (_payload)
				memcpy(_payload, payload, payloadLen);
			_payloadLen = payloadLen;
		}
		else {
			_payload = NULL;
			_payloadLen = 0;
		}
		this->callback = callback;	
	};		
	
	~PendingCallbackItem() {
		if (_topic)
			free(_topic);
		if (_payload)
			free(_payload);
	};
	
	char * _topic;
	uint8_t* _payload;
	unsigned int _payloadLen;
	MQTT_CALLBACK_SIGNATURE;
};
#endif

#define STATE_MQTT_RECONNECT 	1
#define STATE_MQTT_RESUBSCRIBE  2
#define STATE_MQTT_LOOP		 	3
ESPPubSubClient::ESPPubSubClient (char* domain, uint16_t port) :PubSubClient(domain, port, _wiFiClient) {
//	_client = new PubSubClient(domain, port);
	StatemachineLooper.add(this);
};

ESPPubSubClient::ESPPubSubClient(uint8_t* ipaddr, uint16_t port) :PubSubClient(ipaddr, port, _wiFiClient) {
//	_client = new PubSubClient(ipaddr, port, *(new WiFiClient));
	StatemachineLooper.add(this);
};

/*
PubSubClient* ESPPubSubClient::getClient() {
	return _client;
}

bool ESPPubSubClient::connected() {
	return _client->connected();
}
*/

#if defined(PUBLISH_WAITCONNECTED)
class WaitingPublishItem {
	public:
		WaitingPublishItem(const char* topic, const uint8_t * payload, unsigned int plength, boolean retained) {
			_topic = NULL;
			_payload = NULL;
			_retained = retained;
			_plength = plength;
			if (topic)
				if (_topic = (char *)malloc(strlen(topic) + 1))
					strcpy(_topic, topic);
			if (payload)
				if (_payload = (uint8_t *)malloc(plength))
					memcpy(_payload, payload, plength);
		};
		~WaitingPublishItem() {
			if (_topic)
				free(_topic);
			if (_payload)
				free(_payload);
		};
		char *_topic;
		uint8_t* _payload;
		unsigned int _plength;
		boolean _retained;
};
#endif


void ESPPubSubClient::onEnter(int16_t currentStateNb, int16_t oldStateNb) {
	if (STATE_MQTT_RECONNECT == currentStateNb)
		_firstRetry = true;
	else if (STATE_MQTT_RESUBSCRIBE == currentStateNb)
		_subscribed = 0;
}
void ESPPubSubClient::runState(int16_t stateNb) {
int wifiStatus = WiFi.status();	
#if defined(QUEUE_CALLBACKS)
	while (_pendingCallbacks.size() > 0) {
		PendingCallbackItem *callBackItem = _pendingCallbacks[0];
		_pendingCallbacks.pop_front();
//		publish("Garage/delayedCallback", callBackItem->_topic);
		if (callBackItem->callback)
			callBackItem->callback(callBackItem->_topic, callBackItem->_payload, callBackItem->_payloadLen);
		delete callBackItem;
		yield();
	}
#endif
	if ((STATE_INIT_NONE != stateNb) && (WL_CONNECTED != wifiStatus)) {
		setState(STATE_INIT_NONE);
		Serial.println("Reset MQTT: WiFi lost!?");
		return;
	}

	switch (stateNb) {
		case STATE_INIT_NONE:
			if (WL_CONNECTED == wifiStatus) {
				setCallback([this](char* topic, byte* payload, unsigned int length) 
						{this->receivedCallback(topic, payload, length);});
				setState(STATE_MQTT_RECONNECT);
			}
			break;
		case STATE_MQTT_RECONNECT:
//			if (connected())
//				setState(STATE_MQTT_LOOP);
			if (_firstRetry || (getStateTime() > 5000)) {
				const char *s = _clientID;
				String clientIdStr;
				if (NULL == s) {
					clientIdStr = "ESPClient-";
					clientIdStr += String(random(0xffff), HEX)+String(random(0xffff), HEX);
				// Attempt to connect
					s = clientIdStr.c_str();
				}
				_firstRetry = false;
				Serial.print("Attempting MQTT connection...");
				Serial.print(s);
				Serial.print("...");
				// Create a random client ID
				// Attempt to connect
				if (connect(s, _connect_user, _connect_pass, _connect_willTopic, _connect_willQos, 
							_connect_willRetain, _connect_willMessage, _connect_cleanSession)) {
					setState(STATE_MQTT_RESUBSCRIBE);
					Serial.println("connected");
					// Once connected, publish an announcement...
					// ... and resubscribe
				} else {
					Serial.print("failed, rc=");
					Serial.print(PubSubClient::state());
					Serial.println(" try again in 5 seconds");
					resetStateTime();
				}
			}
			break;
		case STATE_MQTT_RESUBSCRIBE:
				if (_subscribed >= _onEvents.size())
					setState(STATE_MQTT_LOOP);
				else {
					PubSubClient::loop();
					_onEvents[_subscribed]->subscribed =
						PubSubClient::subscribe(_onEvents[_subscribed]->topic, _onEvents[_subscribed]->qos);
					_subscribed++;
				}
			break;
		case STATE_MQTT_LOOP:
			if (!PubSubClient::loop()) {
				setState(STATE_INIT_NONE);
				Serial.println("Leaving MQTT-Loop!");
			} else {
#if defined(PUBLISH_WAITCONNECTED)				
				if (_waitingPublishs.size() > 0) {
					WaitingPublishItem *publishItem = _waitingPublishs[0];
					_waitingPublishs.pop_front();
					PubSubClient::publish(publishItem->_topic, publishItem->_payload, 
							publishItem->_plength, publishItem->_retained);
					delete publishItem;
				}
#endif
			}
			break;
		}
};		


void ESPPubSubClient::setConnect(const char *id) {
    setConnect(id,NULL,NULL,0,0,0,0,1);
}

void ESPPubSubClient::setConnect(const char *id, const char *user, const char *pass) {
    setConnect(id,user,pass,0,0,0,0,1);
}

void ESPPubSubClient::setConnect(const char *id, const char* willTopic, uint8_t willQos, boolean willRetain, const char* willMessage) {
    setConnect(id,NULL,NULL,willTopic,willQos,willRetain,willMessage,1);
}

void ESPPubSubClient::setConnect(const char *id, const char *user, const char *pass, const char* willTopic, uint8_t willQos, boolean willRetain, const char* willMessage) {
    setConnect(id,user,pass,willTopic,willQos,willRetain,willMessage,1);
}

void ESPPubSubClient::setConnect(const char *id, const char *user, const char *pass, const char* willTopic, 
		uint8_t willQos, boolean willRetain, const char* willMessage, boolean cleanSession) {
	if (_connect_id) 
		free(_connect_id);
	if (id)
		_connect_id = strdup(id);
	else
		_connect_id = NULL;
	if (_connect_user) 
		free(_connect_user);
	if (user)
		_connect_user = strdup(user);
	else
		_connect_user = NULL;
	if (_connect_pass) 
		free(_connect_pass);
	if (pass)
		_connect_pass = strdup(pass);
	else
		_connect_pass = NULL;
	if (_connect_willTopic) 
		free(_connect_willTopic);
	if (willTopic)
		_connect_willTopic = strdup(willTopic);
	else
		_connect_willTopic = NULL;
	if (_connect_willMessage) 
		free(_connect_willMessage);
	if (willMessage)
		_connect_willMessage = strdup(willMessage);
	else
	_connect_willMessage = NULL;
	_connect_willQos = willQos;
	_connect_willRetain = willRetain;
	_connect_cleanSession = cleanSession;
	}		
	

void ESPPubSubClient::receivedCallback(char* topic, uint8_t* payload, unsigned int payloadLen) {
bool found = false;
int i;
//	Serial.print("Checking callbacks! Topic: ");
//	Serial.println(topic);
	for (i = 0; (i < _onEvents.size()) && !found;) {
		if (_onEvents[i]->subscribed)
			{
//			Serial.print("...match with: \"");Serial.print(_onEvents[i]->topic);Serial.print("\"??: ");
			if (_onEvents[i]->hasLevelWildcard)
				found = _onEvents[i]->topicMatch(topic);
			else if (_onEvents[i]->hasHash)
				found = (0 == strncmp(topic, _onEvents[i]->topic, _onEvents[i]->hasHash - 1));
			else
				found = (0 == strcmp(topic, _onEvents[i]->topic));
//			Serial.println(found);
			if (!found)
				i++;
			}
	}
	if (found) {
#if defined(QUEUE_CALLBACKS)
	_pendingCallbacks.push_back(new PendingCallbackItem(topic, payload, payloadLen, _onEvents[i]->callback));
#else
	if (_onEvents[i]->callback) {
		_onEvents[i]->callback(topic, payload, payloadLen);
	}	
#endif
	}
}

void ESPPubSubClient::on(char* topic, MQTT_CALLBACK_SIGNATURE, uint8_t qos) {
onEventItem *onEvent = new onEventItem(topic, callback, qos);
	_onEvents.push_back(onEvent);
	if (STATE_MQTT_LOOP == getState())
		PubSubClient::subscribe(onEvent->topic, onEvent->qos);
}


onEventItem::onEventItem(char *t, MQTT_CALLBACK_SIGNATURE, uint8_t qos) {
	if (topic = strdup(t)) {
		char *hashPos = strchr(topic, '#');
		if (hashPos != NULL) {
			hasHash = 1 + hashPos - topic; 
			hashPos[1] = 0;
		}
		else
			hasHash = 0;
		hasLevelWildcard = strchr(topic, '+') != NULL;		
		this->callback = callback;
		this->qos = qos;
	};
}

boolean onEventItem::topicMatch(char *value) {
char *pattern = topic;
  while (*value) {
    if (*value != *pattern) {
       if ('#' == *pattern)
        return true;
       if ('+' == *pattern) {
        pattern++;
        while ((0 != *value) && ('/' != *value))
          value++;
       }
        else return false;
    }
    else {
      value++;
      pattern++;
    } 
  }
  return (0 == *pattern) || (0 == strcmp(pattern, "#")) || (0 == strcmp(pattern, "+"));
}

boolean ESPPubSubClient::publish_waitConnected(const char* topic, const char* payload) {
	return(publish_waitConnected(topic, (const uint8_t*) payload, strlen(payload), false));
}

boolean ESPPubSubClient::publish_waitConnected(const char* topic, const char* payload, boolean retained) {
	return(publish_waitConnected(topic, (const uint8_t*) payload, strlen(payload), retained));
};
boolean ESPPubSubClient::publish_waitConnected(const char* topic, const uint8_t * payload, unsigned int plength) {
	return(publish_waitConnected(topic,payload, plength, false));
}

boolean ESPPubSubClient::publish_waitConnected(const char* topic, const uint8_t * payload, unsigned int plength, boolean retained) {
#if defined(PUBLISH_WAITCONNECTED)
	if (connected()) {
		return PubSubClient::publish(topic, payload, plength, retained);
	} else {
		_waitingPublishs.push_back(new WaitingPublishItem(topic, payload, plength, retained));
		return true;
	}
#else
	return PubSubClient::publish(topic, payload, plength, retained);
#endif	
}

void ESPPubSubClient::loop() {
	run();
}


/*
onEventStruct *onEventStruct::getOnEvent(const char *topic) {
onEventStruct *ret = this;
		while (ret) {
			Serial.print("Try match: ");Serial.print(topic);Serial.print(" with: ");Serial.println(ret->topic);
			if (ret->hasLevelWildcard) {
				if (ret->topicMatch((char *)topic))
					return ret;
			}
			else if (ret->hasHash){
				if (strstr(topic, ret->topic) == topic)
					return ret;
			}	
			else if (0 == strcmp(topic, ret->topic))
				return ret;
			Serial.println("Match not found. May be next!?");
			ret = ret->next;
		}
		return ret;		
}
*/
	



#endif