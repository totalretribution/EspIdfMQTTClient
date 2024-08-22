#ifndef ESP_MQTT_CLIENT_H
#define ESP_MQTT_CLIENT_H

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include "mqtt_client.h"
#include <vector>

#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "ESP32HTTPUpdateServer.h"

#define DEFAULT_MQTT_CLIENT_NAME "ESP32"
#define ESPHTTPUpdateServer ESP32HTTPUpdateServer

void onConnectionEstablished(); // MUST be implemented in your sketch. Called once everythings is connected (Wifi, mqtt).

typedef std::function<void()> ConnectionEstablishedCallback;
typedef std::function<void(const String &message)> MessageReceivedCallback;
typedef std::function<void(const String &topicStr, const String &message)> MessageReceivedCallbackWithTopic;
typedef std::function<void()> DelayedExecutionCallback;

class EspIdfMQTTClient
{
private:
  // Wifi related
  bool _handleWiFi;
  bool _wifiConnected;
  bool _connectingToWifi;
  unsigned long _lastWifiConnectionAttemptMillis;
  unsigned long _nextWifiConnectionAttemptMillis;
  unsigned int _wifiReconnectionAttemptDelay;
  const char* _wifiSsid;
  const char* _wifiPassword;
  WiFiClient _wifiClient;

  // MQTT related
  bool _mqttConnected;
  unsigned long _nextMqttConnectionAttemptMillis;
  unsigned int _mqttReconnectionAttemptDelay;
  const char* _mqttServerIp;
  const char* _mqttUsername;
  const char* _mqttPassword;
  const char* _mqttClientName;
  uint16_t _mqttServerPort;
  bool _mqttCleanSession;
  char* _mqttLastWillTopic;
  char* _mqttLastWillMessage;
  bool _mqttLastWillRetain;
  unsigned int _failedMQTTConnectionAttemptCount;

  uint16_t _mqttKeepAliveSeconds;
  uint16_t _mqttBufferSize;
  esp_mqtt_client_handle_t _mqtt_handle;
  enum class MqttState
  {
    MQTT_UNKNOWN,
    MQTT_DISCONNECTED,
    MQTT_CONNECTING,
    MQTT_CONNECTED,
    MQTT_ERROR
  };
  MqttState _mqttState;

  const char* _root_ca = NULL;
  const char* _client_cert = NULL;
  const char* _client_key = NULL;


  struct TopicSubscriptionRecord {
    String topic;
    MessageReceivedCallback callback;
    MessageReceivedCallbackWithTopic callbackWithTopic;
  };
  std::vector<TopicSubscriptionRecord> _topicSubscriptionList;

  // HTTP/OTA update related
  char* _updateServerAddress;
  char* _updateServerUsername;
  char* _updateServerPassword;
  WebServer* _httpServer;
  ESPHTTPUpdateServer* _httpUpdater;
  bool _enableOTA;

  // Delayed execution related
  struct DelayedExecutionRecord {
    unsigned long targetMillis;
    DelayedExecutionCallback callback;
  };
  std::vector<DelayedExecutionRecord> _delayedExecutionList;

  // General behaviour related
  ConnectionEstablishedCallback _connectionEstablishedCallback;
  bool _enableDebugMessages;
  bool _drasticResetOnConnectionFailures;
  unsigned int _connectionEstablishedCount; // Incremented before each _connectionEstablishedCallback call

public:
  EspIdfMQTTClient(
    // port and client name are swapped here to prevent a collision with the MQTT w/o auth constructor
    const uint16_t mqttServerPort = 1883,
    const char* mqttClientName = DEFAULT_MQTT_CLIENT_NAME);

  /// Wifi + MQTT with no MQTT authentification
  EspIdfMQTTClient(
    const char* wifiSsid,
    const char* wifiPassword,
    const char* mqttServerIp,
    const char* mqttClientName = DEFAULT_MQTT_CLIENT_NAME,
    const uint16_t mqttServerPort = 1883);

  /// Wifi + MQTT with MQTT authentification
  EspIdfMQTTClient(
    const char* wifiSsid,
    const char* wifiPassword,
    const char* mqttServerIp,
    const char* mqttUsername,
    const char* mqttPassword,
    const char* mqttClientName = DEFAULT_MQTT_CLIENT_NAME,
    const uint16_t mqttServerPort = 1883);

  /// Only MQTT handling (no wifi), with MQTT authentification
  EspIdfMQTTClient(
    const char* mqttServerIp,
    const uint16_t mqttServerPort,
    const char* mqttUsername,
    const char* mqttPassword,
    const char* mqttClientName = DEFAULT_MQTT_CLIENT_NAME);

  /// Only MQTT handling without MQTT authentification
  EspIdfMQTTClient(
    const char* mqttServerIp,
    const uint16_t mqttServerPort,
    const char* mqttClientName = DEFAULT_MQTT_CLIENT_NAME);

  ~EspIdfMQTTClient();

  // Optional functionality
  void enableDebuggingMessages(const bool enabled = true); // Allow to display useful debugging messages. Can be set to false to disable them during program execution
  void enableHTTPWebUpdater(const char* username, const char* password, const char* address = "/"); // Activate the web updater, must be set before the first loop() call.
  void enableHTTPWebUpdater(const char* address = "/"); // Will set user and password equal to _mqttUsername and _mqttPassword
  void enableOTA(const char *password = NULL, const uint16_t port = 0); // Activate OTA updater, must be set before the first loop() call.
  void enableMQTTPersistence(); // Tell the broker to establish a persistent connection. Disabled by default. Must be called before the first loop() execution
  void enableLastWillMessage(const char* topic, const char* message, const bool retain = false); // Must be set before the first loop() call.
  void enableDrasticResetOnConnectionFailures() {_drasticResetOnConnectionFailures = true;} // Can be usefull in special cases where the ESP board hang and need resetting (#59)

  /// Main loop, to call at each sketch loop()
  void loop();

  // MQTT related
  bool setMaxPacketSize(const uint16_t size); // Pubsubclient >= 2.8; override the default value of MQTT_MAX_PACKET_SIZE

  bool publish(const char* topic, const uint8_t* payload, unsigned int plenght, bool retain);
  bool publish(const String &topic, const String &payload, bool retain = false);

  bool enqueue(const String &topic, const String &payload, bool retain);
  bool enqueue(const char* topic, const uint8_t* payload, unsigned int plength, bool retain);
  bool subscribe(const String &topic, MessageReceivedCallback messageReceivedCallback, uint8_t qos = 0);
  bool subscribe(const String &topic, MessageReceivedCallbackWithTopic messageReceivedCallback, uint8_t qos = 0);
  bool unsubscribe(const String &topic);   //Unsubscribes from the topic, if it exists, and removes it from the CallbackList.
  void setKeepAlive(uint16_t keepAliveSeconds); // Change the keepalive interval (15 seconds by default)
  inline void setMqttClientName(const char* name) { _mqttClientName = name; }; // Allow to set client name manually (must be done in setup(), else it will not work.)
  inline void setMqttServer(const char* server, const char* username = "", const char* password = "", const uint16_t port = 1883) { // Allow setting the MQTT info manually (must be done in setup())
    _mqttServerIp   = server;
    _mqttUsername   = username;
    _mqttPassword   = password;
    _mqttServerPort = port;
  };

  // Wifi related
  void setWifiCredentials(const char* wifiSsid, const char* wifiPassword);

  // Other
  void executeDelayed(const unsigned long delay, DelayedExecutionCallback callback);

  inline bool isConnected() const { return isWifiConnected() && isMqttConnected(); }; // Return true if everything is connected
  inline bool isWifiConnected() const { return _wifiConnected; }; // Return true if wifi is connected
  inline bool isMqttConnected() const { return _mqttConnected; }; // Return true if mqtt is connected
  // inline bool isMqttConnected() const { return _mqttState == MqttState::MQTT_CONNECTED; }; // Return true if mqtt is connected
  inline unsigned int getConnectionEstablishedCount() const { return _connectionEstablishedCount; }; // Return the number of time onConnectionEstablished has been called since the beginning.

  inline const char* getMqttClientName() { return _mqttClientName; };
  inline const char* getMqttServerIp() { return _mqttServerIp; };
  inline uint16_t getMqttServerPort() { return _mqttServerPort; };

  // Default to onConnectionEstablished, you might want to override this for special cases like two MQTT connections in the same sketch
  inline void setOnConnectionEstablishedCallback(ConnectionEstablishedCallback callback) { _connectionEstablishedCallback = callback; };

  // Allow to set the minimum delay between each MQTT reconnection attempt. 15 seconds by default.
  inline void setMqttReconnectionAttemptDelay(const unsigned int milliseconds) { _mqttReconnectionAttemptDelay = milliseconds; };

  // Allow to set the minimum delay between each WiFi reconnection attempt. 60 seconds by default.
  inline void setWifiReconnectionAttemptDelay(const unsigned int milliseconds) { _wifiReconnectionAttemptDelay = milliseconds; };

private:
  bool handleWiFi();
  bool handleMQTT();
  void onWiFiConnectionEstablished();
  void onWiFiConnectionLost();
  void onMQTTConnectionEstablished();
  void onMQTTConnectionLost();

  void connectToWifi();
  bool connectToMqttBroker();
  void processDelayedExecutionRequests();
  bool mqttTopicMatch(const String &topic1, const String &topic2);
  void mqttMessageReceivedCallback(char* topic, uint8_t* payload, unsigned int length);
  void mqtt_event_handler(esp_event_base_t event_base, int32_t event_id, void* event_data);
  static void s_handle_mqtt_event(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
  void setRootCA(const char* ca);
  void setClientCertificate(const char* cert);
  void setClientKey(const char* key);
};

#endif
