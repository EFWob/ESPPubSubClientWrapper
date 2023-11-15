#include <ESPPubSubClientWrapper.h>


class onEventItem {
public:
/*
	onEventItem(const char *t, MQTT_CALLBACK_SIGNATURE, uint8_t qos);
	onEventItem(const char *t, MQTT_CALLBACK_SIGNATURE2, uint8_t qos);
	onEventItem(const char *t, MQTT_CALLBACK_SIGNATURE3, void *data, uint8_t qos);
*/
	onEventItem(char *t, MQTT_CALLBACK_SIGNATURE1 , MQTT_CALLBACK_SIGNATURE2, 
					MQTT_CALLBACK_SIGNATURE3, void *d, uint8_t q);
	~onEventItem();
	boolean topicMatch(char *value);
	char * topic;
	uint16_t hasHash;
	bool hasLevelWildcard;
	uint8_t qos;
	bool subscribed = false;
	MQTT_CALLBACK_SIGNATURE1 callback1;
	MQTT_CALLBACK_SIGNATURE2 callback2;
	MQTT_CALLBACK_SIGNATURE3 callback3;
	void *data;
	onEventItem* _next = NULL;
	friend class ESPPubSubClientWrapper;
protected:
	bool setValues(const char *t, uint8_t qos);

};

#if defined(QUEUE_CALLBACKS)
class PendingCallbackItem {
public:
	PendingCallbackItem(char *topic, uint8_t *payload, unsigned int payloadLen, MQTT_CALLBACK_SIGNATURE1 cb1) {	
		setValues(topic, payload, payloadLen);
		this->callback1 = cb1;	
	};		

	PendingCallbackItem(char *topic, uint8_t *payload, unsigned int payloadLen, MQTT_CALLBACK_SIGNATURE2 cb2) {	
		setValues(topic, payload, payloadLen);
		this->callback2 = cb2;	
	};		

	PendingCallbackItem(char *topic, uint8_t *payload, unsigned int payloadLen, MQTT_CALLBACK_SIGNATURE3 cb3, void *data) {	
		setValues(topic, payload, payloadLen);
		this->callback3 = cb3;	
		this->data = data;
	};		


	~PendingCallbackItem() {
		if (_topic)
			free(_topic);
		if (_payload)
			free(_payload);
	};

protected:
	void setValues(char *topic, uint8_t *payload, unsigned int payloadLen) {	
		callback1 = nullptr;
		callback2 = nullptr;
		callback3 = nullptr;
		data = NULL;
		_topic = strdup(topic);
		if (payload && payloadLen) {
			_payload = (uint8_t *)malloc(payloadLen + 1);
			if (_payload)
			{
				memcpy(_payload, payload, payloadLen);
				_payload[payloadLen] = 0;
			}
			_payloadLen = payloadLen;
		}
		else {
			_payload = NULL;
			_payloadLen = 0;
		}

	};

	char * _topic;
	uint8_t* _payload;
	unsigned int _payloadLen;
	MQTT_CALLBACK_SIGNATURE1 callback1;
	MQTT_CALLBACK_SIGNATURE2 callback2;
	MQTT_CALLBACK_SIGNATURE3 callback3;
	void *data;
	PendingCallbackItem* _next = NULL;
	friend class ESPPubSubClientWrapper;
};
#endif

#define STATE_MQTT_NONE         0
#define STATE_MQTT_RECONNECT 	1
#define STATE_MQTT_RESUBSCRIBE  2
#define STATE_MQTT_LOOP		 	3
#define STATE_MQTT_STOP			4
ESPPubSubClientWrapper::ESPPubSubClientWrapper (const char* domain, uint16_t port) :PubSubClient(domain, port, _wiFiClient) {
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


void ESPPubSubClientWrapper::setState(int16_t newState) {
	if (STATE_MQTT_NONE == newState) {
		onEventItem* onEvent = _firstOnEvent;
		while(onEvent) {
			onEvent->subscribed = false;
			onEvent = onEvent->_next;
		}
	} else if (STATE_MQTT_RECONNECT == newState)
		_firstRetry = true;
	else if (STATE_MQTT_RESUBSCRIBE == newState) {
//		_subscribed = 0;
		_subsciptionPending = _firstOnEvent;
	}
	_stateNb = newState;
	_stateStartTime = millis();
}


const char* ESPPubSubClientWrapper::getSubscription(int i, void **data) {
const char* ret = NULL;	
onEventItem *p = _firstOnEvent;
	while (p && (ret == NULL)) {
		if (i) {
			i--;
			p = p->_next;
		}
		else {
			ret = p->topic;
			if (ret) {
				if (*data)
					if (p->callback3)
						*data = p->data;
			} else	
				p = NULL;
		}
	}	
	return ret;
}

boolean ESPPubSubClientWrapper::loop() {
int wifiStatus = WiFi.status();	
PendingCallbackItem *callBackItem;
#if defined(QUEUE_CALLBACKS)
	while (callBackItem = _firstPendingCallback) {
		if (callBackItem->callback1)
			callBackItem->callback1(callBackItem->_topic, callBackItem->_payload, callBackItem->_payloadLen);
		else if (callBackItem->callback2)
			callBackItem->callback2(callBackItem->_topic, (char *)callBackItem->_payload);
		else if (callBackItem->callback3)
			callBackItem->callback3(callBackItem->_topic, (char *)callBackItem->_payload, callBackItem->data);
		if (NULL == (_firstPendingCallback = callBackItem->_next))
			_lastPendingCallback = NULL;
		delete callBackItem;
		yield();
	}
#endif
	if ((STATE_MQTT_NONE != _stateNb) && (STATE_MQTT_STOP != _stateNb) && (WL_CONNECTED != wifiStatus)) {
		setState(STATE_MQTT_NONE);
//		Serial.println("Reset MQTT: WiFi lost!?");
		return true;
	}

	switch (_stateNb) {
		case STATE_MQTT_NONE:
//			Serial.println("State None!");
			if (WL_CONNECTED == wifiStatus) {
				PubSubClient::setCallback([this](char* topic, byte* payload, unsigned int length) 
						{this->receivedCallback(topic, payload, length);});
				setState(STATE_MQTT_RECONNECT);
				_connectFailCount = 0;
			}
			break;
		case STATE_MQTT_RECONNECT:
			if (_firstRetry || (millis() - _stateStartTime > 5000)) {
				const char *s = _connect_id;
				String clientIdStr;
				if (NULL == s) {
					clientIdStr = "ESPClient-";
					clientIdStr += String(random(0xffff), HEX)+String(random(0xffff), HEX);
				// Attempt to connect
					s = clientIdStr.c_str();
				}
				_firstRetry = false;
//				Serial.print("Attempting MQTT connection...");
//				Serial.print(s);
//				Serial.print("...");
				// Create a random client ID
				// Attempt to connect
				if (PubSubClient::connect(s, _connect_user, _connect_pass, _connect_willTopic, _connect_willQos, 
							_connect_willRetain, _connect_willMessage, _connect_cleanSession)) {
					setState(STATE_MQTT_RESUBSCRIBE);
//					Serial.println("connected");
					if (_cbConnect) 
						_cbConnect(_discCount);
					_discCount++;
				} else {
					bool retry = true;
					if (_cbConnectFail)
						retry = _cbConnectFail(++_connectFailCount);
//					Serial.print("failed, rc=");
//					Serial.print(PubSubClient::state());
					if (retry) {
//						Serial.println(" try again in 5 seconds");
						_stateStartTime = millis();
					} else {
//						Serial.println(" giving up finally!");
						setState(STATE_MQTT_STOP);
					}	
				}
			}
			break;
		case STATE_MQTT_RESUBSCRIBE:
				if (NULL == _subsciptionPending)
					setState(STATE_MQTT_LOOP);
				else {
					PubSubClient::loop();
					_subsciptionPending->subscribed =
						PubSubClient::subscribe(_subsciptionPending->topic, _subsciptionPending->qos);
					_subsciptionPending = _subsciptionPending->_next;						
				}
			break;
		case STATE_MQTT_LOOP:
			if (!PubSubClient::loop()) {
				setState(STATE_MQTT_NONE);
				if (_cbDisc)
					_cbDisc(_discCount);
//				Serial.println("Leaving MQTT-Loop!");
			} else {
#if defined(PUBLISH_WAITCONNECTED)		
				WaitingPublishItem* publishItem = _firstWaitingPublish;
//				if (_waitingPublishs.size() > 0) {
//					WaitingPublishItem *publishItem = _waitingPublishs[0];
//					_waitingPublishs.pop_front();
//					_waitingPublishs.erase(_waitingPublishs.begin());
				if (NULL != _firstWaitingPublish) {
					PubSubClient::publish(publishItem->_topic, publishItem->_payload, 
							publishItem->_plength, publishItem->_retained);
					if (NULL == (_firstWaitingPublish = publishItem->_next))
						_lastWaitingPublish = NULL;
					delete publishItem;
				}
#endif
			}
			break;
		case STATE_MQTT_STOP:
			break;
		}
	return true;
};		



ESPPubSubClientWrapper& ESPPubSubClientWrapper::onConnect(CALLBACK_SIGNATURE_VOID_UINT16 callback) {
	_cbConnect = callback;
	return *this;
};

ESPPubSubClientWrapper& ESPPubSubClientWrapper::onDisconnect(CALLBACK_SIGNATURE_VOID_UINT16 callback) {
	_cbDisc = callback;
	return *this;
};

ESPPubSubClientWrapper& ESPPubSubClientWrapper::onConnectFail(CALLBACK_SIGNATURE_BOOL_UINT16 callback) {
	_cbConnectFail = callback;
	return *this;
}

ESPPubSubClientWrapper& ESPPubSubClientWrapper::setCallback(MQTT_CALLBACK_SIGNATURE) {
    this->callback = callback;
    return *this;
}
	


boolean ESPPubSubClientWrapper::connect(const char *id) {
    return doSetConnect(id,NULL,NULL,0,0,0,0,1);
}

boolean ESPPubSubClientWrapper::connect(const char *id, const char *user, const char *pass) {
    return doSetConnect(id,user,pass,0,0,0,0,1);
}

boolean ESPPubSubClientWrapper::connect(const char *id, const char* willTopic, uint8_t willQos, boolean willRetain, const char* willMessage) {
    return doSetConnect(id,NULL,NULL,willTopic,willQos,willRetain,willMessage,1);
}

boolean ESPPubSubClientWrapper::connect(const char *id, const char *user, const char *pass, const char* willTopic, uint8_t willQos, boolean willRetain, const char* willMessage) {
    return doSetConnect(id,user,pass,willTopic,willQos,willRetain,willMessage,1);
}

boolean ESPPubSubClientWrapper::connect(const char *id, const char *user, const char *pass, const char* willTopic, 
		uint8_t willQos, boolean willRetain, const char* willMessage, boolean cleanSession) {
		return doSetConnect(id,user,pass,willTopic,willQos,willRetain,willMessage, cleanSession);
	}

boolean ESPPubSubClientWrapper::doSetConnect(const char *id, const char *user, const char *pass, const char* willTopic, 
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
	return true;
	}		
	

void ESPPubSubClientWrapper::receivedCallback(char* topic, uint8_t* payload, unsigned int payloadLen) {
onEventItem* found = NULL;
onEventItem* onEvent;
int i;
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

	if (found) {
		Serial.printf("Found: %s, cb1=%d, cb2=%d, cb3=%d\r\n", found->topic,
			(bool)found->callback1, (bool)found->callback2, 
			(bool)found->callback3);Serial.flush();
		Serial.printf("FWIW: sizeof(cb1)=%d\r\n", sizeof(found->callback1));	
		MQTT_CALLBACK_SIGNATURE1 callback1 = found->callback1;
		if (!callback1 && !found->callback2 && !found->callback3) {
			callback1 = this->callback;
			Serial.println("Using default callback!");Serial.flush();
		}
		if (callback1 || found->callback2 || found->callback3) {
#if defined(QUEUE_CALLBACKS)
//	_pendingCallbacks.push_back(new PendingCallbackItem(topic, payload, payloadLen, _onEvents[i]->callback));
			PendingCallbackItem* pendingCallback = NULL;
			if (callback1) 
				pendingCallback = new PendingCallbackItem(topic, payload, payloadLen, callback1);
			else if (found->callback2)
				pendingCallback = new PendingCallbackItem(topic, payload, payloadLen, found->callback2);
			else if (found->callback3)
				pendingCallback = new PendingCallbackItem(topic, payload, payloadLen, found->callback3, found->data);
			if (pendingCallback) {
				if (_lastPendingCallback)
					_lastPendingCallback->_next = pendingCallback;
				else
					_firstPendingCallback = pendingCallback;
				_lastPendingCallback = pendingCallback;
			}
#else
			if (callback1)
				callback1(topic, payload, payloadLen);	
			else
			{
				char *s = (char*)malloc(payloadLen + 1);
				if (s)
				{
					memcpy(s, payload, payloadLen);
					s[payloadLen] = 0;
					if (found->callback2)
						found->callback2(topic, s);
					else if (found->callback3) {
						Serial.printf("Using Callback3 with data: %ld\r\n", found->data);Serial.flush();
						found->callback3(topic, s, found->data);
					}
					free(s);
				}

			}	
#endif
		}
	}
}

boolean ESPPubSubClientWrapper::subscribe(const char* topic, uint8_t qos) {
	newOnEvent(topic, nullptr, nullptr, nullptr, NULL, qos);
	return true;
}

void ESPPubSubClientWrapper::newOnEvent(const char* topic, MQTT_CALLBACK_SIGNATURE1 cb1,
		MQTT_CALLBACK_SIGNATURE2 cb2, MQTT_CALLBACK_SIGNATURE3 cb3, void *data, uint8_t qos) {
	if (topic) {
		Serial.println("Start newOnEvent");Serial.flush();
		char *t = strdup(topic);
		Serial.println("newOnEvent Topic duplicated");Serial.flush();
		if (t) {
			Serial.println("Start new onEventItem");Serial.flush();
			Serial.printf("topic=%s, cb1=%ld, cb2=%ld, cb3=%ld\r\n", t, cb1, cb2, cb3);Serial.flush();
			onEventItem *onEvent = new onEventItem(t, cb1, cb2, cb3, data, qos); 
			Serial.println("onEventItem created");Serial.flush();
			if (NULL == _firstOnEvent)
				_firstOnEvent = onEvent;
			else
				_lastOnEvent->_next = onEvent;
			_lastOnEvent = onEvent;
			Serial.println("onEventItem chained");Serial.flush();
			if (STATE_MQTT_LOOP == _stateNb)
				onEvent->subscribed = PubSubClient::subscribe(onEvent->topic, onEvent->qos);
		}
	}
}


ESPPubSubClientWrapper& ESPPubSubClientWrapper::on(const char* topic, MQTT_CALLBACK_SIGNATURE1 cb1, uint8_t qos) {
	newOnEvent(topic, cb1, nullptr, nullptr, NULL, qos);
/*
onEventItem *onEvent = new onEventItem(topic, callback, NULL, NULL, NULL, qos, 
			&_firstOnEvent, &_lastOnEvent);
	if (NULL == _firstOnEvent)
		_firstOnEvent = onEvent;
	else
		_lastOnEvent->_next = onEvent;
	_lastOnEvent = onEvent;
	//Verkettung sollte passen. _onEvents wird hier nicht mehr benötigt	
//	_onEvents.push_back(onEvent);
	if (STATE_MQTT_LOOP == _stateNb)
		onEvent->subscribed = PubSubClient::subscribe(onEvent->topic, onEvent->qos);
	*/
	return *this;
}

ESPPubSubClientWrapper& ESPPubSubClientWrapper::on(const char* topic, MQTT_CALLBACK_SIGNATURE2 cb2, uint8_t qos) {
	newOnEvent(topic, nullptr, cb2, nullptr, NULL, qos);
/*
onEventItem *onEvent = new onEventItem(topic, NULL, callback2, NULL, NULL, qos, 
			&_firstOnEvent, &_lastOnEvent);
onEventItem *onEvent = new onEventItem(topic, callback2, qos);
	if (NULL == _firstOnEvent)
		_firstOnEvent = onEvent;
	else
		_lastOnEvent->_next = onEvent;
	_lastOnEvent = onEvent;
	//Verkettung sollte passen. _onEvents wird hier nicht mehr benötigt	
//	_onEvents.push_back(onEvent);

	if (STATE_MQTT_LOOP == _stateNb)
		onEvent->subscribed = PubSubClient::subscribe(onEvent->topic, onEvent->qos);
*/
	return *this;
}

ESPPubSubClientWrapper& ESPPubSubClientWrapper::on(const char* topic, MQTT_CALLBACK_SIGNATURE3 cb3, void *data, uint8_t qos) {
	newOnEvent(topic, nullptr, nullptr, cb3, data, qos);
/*
onEventItem *onEvent = new onEventItem(topic, NULL, NULL, callback3, data, qos, 
			&_firstOnEvent, &_lastOnEvent);
onEventItem *onEvent = new onEventItem(topic, callback3, data, qos);
	if (NULL == _firstOnEvent)
		_firstOnEvent = onEvent;
	else
		_lastOnEvent->_next = onEvent;
	_lastOnEvent = onEvent;
	//Verkettung sollte passen. _onEvents wird hier nicht mehr benötigt	
//	_onEvents.push_back(onEvent);

	if (STATE_MQTT_LOOP == _stateNb)
		onEvent->subscribed = PubSubClient::subscribe(onEvent->topic, onEvent->qos);
*/
	return *this;
}


boolean ESPPubSubClientWrapper::unsubscribe(const char* topic) {
boolean ret = false;
onEventItem *onEvent = _firstOnEvent;
onEventItem *prev = NULL;
	while (!ret && (NULL != onEvent))
		if (!(ret = (0 == strcmp(topic, onEvent->topic)))) {
			prev = onEvent;
			onEvent = onEvent->_next;
		}
	if (onEvent) {
		if (prev)
			prev->_next = onEvent->_next;
		else
			_firstOnEvent = onEvent->_next;
		if (!onEvent->_next)
			_lastOnEvent = prev;
		if (this->connected())
			ret = PubSubClient::unsubscribe(onEvent->topic);
		delete onEvent;		
	}
	return ret;
}



onEventItem::onEventItem(char* t, MQTT_CALLBACK_SIGNATURE1 cb1 , MQTT_CALLBACK_SIGNATURE2 cb2,
		MQTT_CALLBACK_SIGNATURE3 cb3, void *d, uint8_t q) {
			this->topic = t;
			char *hashPos = strchr(this->topic, '#');
			if (hashPos != NULL) {
				this->hasHash = 1 + hashPos - this->topic; 
				hashPos[1] = 0;
			}
			else
				this->hasHash = 0;
			hasLevelWildcard = strchr(this->topic, '+') != NULL;	
			this->callback1 = cb1;
			this->callback2 = cb2;
			this->callback3 = cb3;
			this->qos = q;
			this->data = d;
			_next = NULL;
}

/*
onEventItem::onEventItem(const char *t, MQTT_CALLBACK_SIGNATURE, uint8_t qos) {
	if (setValues(t, qos))
		this->callback = callback;
}

onEventItem::onEventItem(const char *t, MQTT_CALLBACK_SIGNATURE2, uint8_t qos) {
	if (setValues(t, qos))
		this->callback2 = callback2;
}

onEventItem::onEventItem(const char *t, MQTT_CALLBACK_SIGNATURE3, void *data, uint8_t qos) {
	if (setValues(t, qos)) {
		this->callback3 = callback3;
		this->data = data;
	}
}

onEventItem::onEventItem(const char *t, MQTT_CALLBACK_SIGNATURE, MQTT_CALLBACK_SIGNATURE2, 
					MQTT_CALLBACK_SIGNATURE3, void *data, uint8_t qos, 
					onEventItem** first, onEventItem** last) {

	if (this->topic = strdup(t)) {
		char *hashPos = strchr(this->topic, '#');
		if (hashPos != NULL) {
			this->hasHash = 1 + hashPos - this->topic; 
			hashPos[1] = 0;
		}
		else
			this->hasHash = 0;
		hasLevelWildcard = strchr(topic, '+') != NULL;	
		this->callback = callback;
		this->callback2 = callback2;
		this->callback3 = callback3;
		this->data = data;
		this->qos = qos;
		if (NULL == *first)
			*first = this;
		else
			(*last)->next = this;
		*last = this;
	}
}
*/


onEventItem::~onEventItem() {
	if (callback3)	
		if (data)
			callback3(topic, NULL, data);
	if (topic)
		free(topic);
}

/*
bool onEventItem::setValues(const char *t, uint8_t qos) {
	callback = NULL;
	callback2 = NULL;
	callback3 = NULL;
	data = NULL;
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
		return true;
	}
	else
		return false;

}
*/

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
