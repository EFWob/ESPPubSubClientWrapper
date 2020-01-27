#ifndef ESPPubSubClientWrapper_h
#define ESPPubSubClientWrapper_h
#include <BasicStatemachine.h>
#include <PubSubClient.h>
//#include <deque>
//#include <vector>

#define QUEUE_CALLBACKS
#define PUBLISH_WAITCONNECTED
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

class onEventItem;
class PendingCallbackItem;
class WaitingPublishItem;


class ESPPubSubClientWrapper : public PubSubClient {
  public:
	ESPPubSubClientWrapper(char *domain, uint16_t port = 1883);
	ESPPubSubClientWrapper(uint8_t *ipaddr, uint16_t port = 1883);
//	PubSubClient* getClient();
//	bool connect();
//	bool connected();
	void setState(int16_t stateNb);
	void setConnect(const char* id);
	void setConnect(const char* id, const char* user, const char* pass);
	void setConnect(const char* id, const char* willTopic, uint8_t willQos, boolean willRetain, const char* willMessage);
	void setConnect(const char* id, const char* user, const char* pass, const char* willTopic, uint8_t willQos, boolean willRetain, const char* willMessage);
	void setConnect(const char* id, const char* user, const char* pass, const char* willTopic, uint8_t willQos, boolean willRetain, const char* willMessage, boolean cleanSession);
//	void onEnter(int16_t currentStateNb, int16_t oldStateNb); 
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
	int16_t _stateNb = 0;
	uint32_t _stateStartTime = 0;
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
//	std::vector<onEventItem *> _onEvents ;              
	onEventItem* _firstOnEvent = NULL;
	onEventItem* _lastOnEvent = NULL;
	onEventItem* _subsciptionPending = NULL;
#if defined(QUEUE_CALLBACKS)	
//	std::deque<PendingCallbackItem *>  _pendingCallbacks;
//	std::vector<PendingCallbackItem *> _pendingCallbacks;
	PendingCallbackItem* _firstPendingCallback = NULL;
	PendingCallbackItem* _lastPendingCallback = NULL;

#endif	
#if defined(PUBLISH_WAITCONNECTED)
//	std::deque<WaitingPublishItem *>  _waitingPublishs;
//	std::vector<WaitingPublishItem *>  _waitingPublishs;
	WaitingPublishItem* _firstWaitingPublish = NULL;
	WaitingPublishItem* _lastWaitingPublish = NULL;
	
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
	onEventItem* _next = NULL;
	friend class ESPPubSubClientWrapper;
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
	PendingCallbackItem* _next = NULL;
	friend class ESPPubSubClientWrapper;
};
#endif

#define STATE_MQTT_NONE         0
#define STATE_MQTT_RECONNECT 	1
#define STATE_MQTT_RESUBSCRIBE  2
#define STATE_MQTT_LOOP		 	3
ESPPubSubClientWrapper::ESPPubSubClientWrapper (char* domain, uint16_t port) :PubSubClient(domain, port, _wiFiClient) {
};

ESPPubSubClientWrapper::ESPPubSubClientWrapper(uint8_t* ipaddr, uint16_t port) :PubSubClient(ipaddr, port, _wiFiClient) {
};


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
		WaitingPublishItem* _next = NULL;
};
#endif


//void ESPPubSubClientWrapper::onEnter(int16_t currentStateNb, int16_t oldStateNb) {
void ESPPubSubClientWrapper::setState(int16_t newState) {
	if (STATE_MQTT_NONE == newState) {
//		for(int i = 0;i < _onEvents.size();i++)
//			_onEvents[i]->subscribed = false;
		onEventItem* onEvent = _firstOnEvent;
		while(onEvent) {
			onEvent->subscribed = false;
			onEvent = onEvent->_next;
		}
	} else if (STATE_MQTT_RECONNECT == newState)
		_firstRetry = true;
	else if (STATE_MQTT_RESUBSCRIBE == newState) {
		_subscribed = 0;
		_subsciptionPending = _firstOnEvent;
	}
	_stateNb = newState;
	_stateStartTime = millis();
}


//void ESPPubSubClientWrapper::runState(int16_t stateNb) {
void ESPPubSubClientWrapper::loop() {
int wifiStatus = WiFi.status();	
PendingCallbackItem *callBackItem;
#if defined(QUEUE_CALLBACKS)
//	while (_pendingCallbacks.size() > 0) {
//		callBackItem = _pendingCallbacks[0];
//		_pendingCallbacks.pop_front();
//		_pendingCallbacks.erase(_pendingCallbacks.begin());
//		publish("Garage/delayedCallback", callBackItem->_topic);
	while (callBackItem = _firstPendingCallback) {
		if (callBackItem->callback)
			callBackItem->callback(callBackItem->_topic, callBackItem->_payload, callBackItem->_payloadLen);
		if (NULL == (_firstPendingCallback = callBackItem->_next))
			_lastPendingCallback = NULL;
		delete callBackItem;
		yield();
	}
#endif
	if ((STATE_INIT_NONE != _stateNb) && (WL_CONNECTED != wifiStatus)) {
		setState(STATE_INIT_NONE);
		Serial.println("Reset MQTT: WiFi lost!?");
		return;
	}

	switch (_stateNb) {
		case STATE_INIT_NONE:
//			Serial.println("State None!");
			if (WL_CONNECTED == wifiStatus) {
				setCallback([this](char* topic, byte* payload, unsigned int length) 
						{this->receivedCallback(topic, payload, length);});
				setState(STATE_MQTT_RECONNECT);
			}
			break;
		case STATE_MQTT_RECONNECT:
//			if (connected())
//				setState(STATE_MQTT_LOOP);
			if (_firstRetry || (millis() - _stateStartTime > 5000)) {
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
				if (PubSubClient::connect(s, _connect_user, _connect_pass, _connect_willTopic, _connect_willQos, 
							_connect_willRetain, _connect_willMessage, _connect_cleanSession)) {
					setState(STATE_MQTT_RESUBSCRIBE);
					Serial.println("connected");
					// Once connected, publish an announcement...
					// ... and resubscribe
				} else {
					Serial.print("failed, rc=");
					Serial.print(PubSubClient::state());
					Serial.println(" try again in 5 seconds");
					_stateStartTime = millis();
				}
			}
			break;
		case STATE_MQTT_RESUBSCRIBE:
#ifndef NEWSUBSCRIBE
				if (NULL == _subsciptionPending)
					setState(STATE_MQTT_LOOP);
				else {
					PubSubClient::loop();
					_subsciptionPending->subscribed =
						PubSubClient::subscribe(_subsciptionPending->topic, _subsciptionPending->qos);
					_subsciptionPending = _subsciptionPending->_next;						
				}
#else
				if (_subscribed >= _onEvents.size())
					setState(STATE_MQTT_LOOP);
				else {
					PubSubClient::loop();
					_onEvents[_subscribed]->subscribed =
						PubSubClient::subscribe(_onEvents[_subscribed]->topic, _onEvents[_subscribed]->qos);
					_subscribed++;
				}
#endif
			break;
		case STATE_MQTT_LOOP:
			if (!PubSubClient::loop()) {
				setState(STATE_INIT_NONE);
				Serial.println("Leaving MQTT-Loop!");
			} else {
#if defined(PUBLISH_WAITCONNECTED)		
				WaitingPublishItem* publishItem;
//				if (_waitingPublishs.size() > 0) {
//					WaitingPublishItem *publishItem = _waitingPublishs[0];
//					_waitingPublishs.pop_front();
//					_waitingPublishs.erase(_waitingPublishs.begin());
				if (publishItem = _firstWaitingPublish) {
					PubSubClient::publish(publishItem->_topic, publishItem->_payload, 
							publishItem->_plength, publishItem->_retained);
					if (NULL == (_firstWaitingPublish = publishItem->_next))
						_lastWaitingPublish = NULL;
					delete publishItem;
				}
#endif
			}
			break;
		}
};		


void ESPPubSubClientWrapper::setConnect(const char *id) {
    setConnect(id,NULL,NULL,0,0,0,0,1);
}

void ESPPubSubClientWrapper::setConnect(const char *id, const char *user, const char *pass) {
    setConnect(id,user,pass,0,0,0,0,1);
}

void ESPPubSubClientWrapper::setConnect(const char *id, const char* willTopic, uint8_t willQos, boolean willRetain, const char* willMessage) {
    setConnect(id,NULL,NULL,willTopic,willQos,willRetain,willMessage,1);
}

void ESPPubSubClientWrapper::setConnect(const char *id, const char *user, const char *pass, const char* willTopic, uint8_t willQos, boolean willRetain, const char* willMessage) {
    setConnect(id,user,pass,willTopic,willQos,willRetain,willMessage,1);
}

void ESPPubSubClientWrapper::setConnect(const char *id, const char *user, const char *pass, const char* willTopic, 
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
	

void ESPPubSubClientWrapper::receivedCallback(char* topic, uint8_t* payload, unsigned int payloadLen) {
onEventItem* found = NULL;
onEventItem* onEvent;
int i;
//	Serial.print("Checking callbacks! Topic: ");
//	Serial.println(topic);
#if !defined(NEWSUBSCRIBE)
	onEvent = _firstOnEvent;
	for(onEvent = _firstOnEvent;(NULL != onEvent) && (NULL == found);onEvent = onEvent->_next) 
		if (onEvent->subscribed) {
			if (onEvent->hasLevelWildcard)
				found = onEvent->topicMatch(topic)?onEvent:NULL;
			else if (onEvent->hasHash)
				found = (0 == strncmp(topic, onEvent->topic, onEvent->hasHash - 1))?onEvent:NULL;
			else
				found = (0 == strcmp(topic, onEvent->topic))?onEvent:NULL;
		}
#else
	for (i = 0; (i < _onEvents.size()) && !found;) {
		if (_onEvents[i]->subscribed)
			{
//			Serial.print("...match with: \"");Serial.print(_onEvents[i]->topic);Serial.print("\"??: ");
			if (_onEvents[i]->hasLevelWildcard)
				found = _onEvents[i]->topicMatch(topic)?_onEvents[i]:NULL;
			else if (_onEvents[i]->hasHash)
				found = (0 == strncmp(topic, _onEvents[i]->topic, _onEvents[i]->hasHash - 1))?_onEvents[i]:NULL;
			else
				found = (0 == strcmp(topic, _onEvents[i]->topic))?_onEvents[i]:NULL;
//			Serial.println(found);
			if (!found)
				i++;
			}
	}
#endif
	if (found) {
#if defined(QUEUE_CALLBACKS)
//	_pendingCallbacks.push_back(new PendingCallbackItem(topic, payload, payloadLen, _onEvents[i]->callback));
	PendingCallbackItem* pendingCallback = new PendingCallbackItem(topic, payload, payloadLen, found->callback);
	if (_lastPendingCallback)
		_lastPendingCallback->_next = pendingCallback;
	else
		_firstPendingCallback = pendingCallback;
	_lastPendingCallback = pendingCallback;
#else
	if (found->callback) {
		found->callback(topic, payload, payloadLen);
	}	
#endif
	}
}

void ESPPubSubClientWrapper::on(char* topic, MQTT_CALLBACK_SIGNATURE, uint8_t qos) {
onEventItem *onEvent = new onEventItem(topic, callback, qos);
	if (NULL == _firstOnEvent)
		_firstOnEvent = onEvent;
	else
		_lastOnEvent->_next = onEvent;
	_lastOnEvent = onEvent;
	//Verkettung sollte passen. _onEvents wird hier nicht mehr benötigt	
//	_onEvents.push_back(onEvent);
	if (STATE_MQTT_LOOP == _stateNb)
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

boolean ESPPubSubClientWrapper::publish_waitConnected(const char* topic, const char* payload) {
	return(publish_waitConnected(topic, (const uint8_t*) payload, strlen(payload), false));
}

boolean ESPPubSubClientWrapper::publish_waitConnected(const char* topic, const char* payload, boolean retained) {
	return(publish_waitConnected(topic, (const uint8_t*) payload, strlen(payload), retained));
};
boolean ESPPubSubClientWrapper::publish_waitConnected(const char* topic, const uint8_t * payload, unsigned int plength) {
	return(publish_waitConnected(topic,payload, plength, false));
}

boolean ESPPubSubClientWrapper::publish_waitConnected(const char* topic, const uint8_t * payload, unsigned int plength, boolean retained) {
#if defined(PUBLISH_WAITCONNECTED)
	if (connected()) {
		return PubSubClient::publish(topic, payload, plength, retained);
	} else {
		WaitingPublishItem* publishItem = new WaitingPublishItem(topic, payload, plength, retained);
		if (NULL == _firstWaitingPublish)
			_firstWaitingPublish = publishItem;
		else
			_lastWaitingPublish->_next = publishItem;
		_lastWaitingPublish = publishItem;
//		_waitingPublishs.push_back(new WaitingPublishItem(topic, payload, plength, retained));
		return true;
	}
#else
	return PubSubClient::publish(topic, payload, plength, retained);
#endif	
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
	

class ESPPubSubClient : public ESPPubSubClientWrapper, public BasicStatemachine {
  public:
	ESPPubSubClient(char *domain, uint16_t port = 1883);
	ESPPubSubClient(uint8_t *ipaddr, uint16_t port = 1883);
  protected:
    void runState(int16_t stateNb);
};
	

ESPPubSubClient::ESPPubSubClient(char *domain, uint16_t port) : ESPPubSubClientWrapper(domain, port) {
	StatemachineLooper.add(this);
}

ESPPubSubClient::ESPPubSubClient(uint8_t *ipaddr, uint16_t port): ESPPubSubClientWrapper(ipaddr, port) {
	StatemachineLooper.add(this);
}	
	
void ESPPubSubClient::runState(int16_t stateNb) {
	loop();
}	


#endif