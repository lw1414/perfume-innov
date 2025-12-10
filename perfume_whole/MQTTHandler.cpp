#include "MQTTHandler.h"

MQTTHandler::MQTTHandler()
    : _mqttClient(_espClient), _incomingTopic(""), _incomingPayload("") {}

void MQTTHandler::init(const char* mqttServer, int mqttPort, const char* mqttUser, const char* mqttPassword, const char* deviceESN, const char* willTopic, const char* willMessage)  {

    _mqttServer = mqttServer;
    _mqttPort = mqttPort;
    _mqttUser = mqttUser;
    _mqttPassword = mqttPassword;
    _deviceESN = deviceESN;
    _willTopic = willTopic;
    _willMessage = willMessage;
    
    _mqttClient.setServer(mqttServer, mqttPort);
    _mqttClient.setCallback([this](char* topic, byte* payload, unsigned int length) {
        this->callback(topic, payload, length);
    });
}


boolean MQTTHandler::connect() {
    if (_mqttClient.connect(_deviceESN, _mqttUser, _mqttPassword, _willTopic, 1, false, _willMessage, true)) {
        // Publish the initial message if defined
        if (!_initialMessageTopic.isEmpty() && !_initialMessagePayload.isEmpty()) {
            _mqttClient.publish(_initialMessageTopic.c_str(), _initialMessagePayload.c_str());
        }

        // Subscribe to all topics dynamically
        subscribeToTopics();

        return true;
    } else {
        Serial.println("Failed connecting to MQTT.");
        return false;
    }
}

boolean MQTTHandler::checkConnectivity() {
    if (!_mqttClient.connected()) {
        return connect(); // Reconnect if disconnected
    }
    _mqttClient.loop(); // Handle incoming messages
    return true;
}

void MQTTHandler::subscribeToTopics() {
    for (const String& topic : _subscriptionTopics) {
        _mqttClient.subscribe(topic.c_str());
        Serial.println("Subscribed to: " + topic);
    }
}



void MQTTHandler::setInitialMessage(const char* topic, const char* message) {
    _initialMessageTopic = String(topic);
    _initialMessagePayload = String(message);
}

void MQTTHandler::addSubscriptionTopic(const char* topic) {
    _subscriptionTopics.push_back(String(topic));
}


void MQTTHandler::subscribe(const char* topic) {
    _mqttClient.subscribe(topic);
}

void MQTTHandler::publish(const char* topic, const char* payload) {
    _mqttClient.publish(topic, payload);
}

boolean MQTTHandler::messageAvailable() {
    return !_incomingTopic.isEmpty();
}

String MQTTHandler::getMessageTopic() {
    return _incomingTopic;
}

String MQTTHandler::getMessagePayload() {
    return _incomingPayload;
}

void MQTTHandler::callback(char* topic, byte* payload, unsigned int length) {
    _incomingTopic = String(topic);

//    Serial.print("Topic: "); Serial.println(_incomingTopic);

    _incomingPayload = "";
    for (int i = 0; i < length; i++) 
        _incomingPayload += (char)payload[i];
}

void MQTTHandler::clearMessageFlag() {
 
    _incomingTopic = "";
    _incomingPayload = "";
}

void MQTTHandler::startup(const char* topic, const char* payload, bool retain) {
  if (!_mqttClient.connected()) return;
  _mqttClient.publish(topic, payload, retain);
}

