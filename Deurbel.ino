
/* Defines that control the loading of modules */
#define WM_DEBUG_LEVEL 2
#define WM_MDNS

/* Include all required libraries */
#include "WiFiManager.h"
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <FS.h> // SPIFFS
#include "ButtonInput.h"


/* Application Defines */
#define __SKETCH_NAME__ "Deurbel"
#define __AP_MODE_SSID__ "Deurbel_AP"
#define __AP_MODE_PASSWORD__ "Deurbel123"

/* Application Constants */
const int belLedPin = D2;
const int wifiLedPin = D4;
const int mqttLedPin = D3;
const int inPin = D6;
const int testPin = D7;
const int relayPin = D8;
const int statusFrequency = 300; // sec

/* Application Variables */
const int loopDelay = 10;
int loopCounter = 0;
bool relayStatus = 0;
bool sendStatusUpdate = 0;
bool settingRingerOn = 1;
bool hassConfigRequested = 1;
long lastReconnectAttempt = 0;

const char* defaultMQTTServerIP = "192.168.0.180";
const char* defaultMQTTPort = "1883";
const char* defaultMQTTUsername = "--Username here--";
const char* defaultMQTTPassword = "--Password here--";
const char* defaultMQTTHASSBirthTopic = "homeassistant/status";

/* Initialized during Setup */
String espId;
String hostName;
String mqttHASSConfigTopic;
String mqttHASSStateTopic;
String mqttHASSCommandTopic;

ButtonInput* belButton;
ButtonInput* testButton;

WiFiManager wm;
WiFiManagerParameter* parameterMQTTServerIP;
WiFiManagerParameter* parameterMQTTServerPort;
WiFiManagerParameter* parameterMQTTUsername;
WiFiManagerParameter* parameterMQTTPassword;
WiFiManagerParameter* parameterMQTTHASSBirthTopic;

WiFiClient espWiFiClient;
PubSubClient pubSubClient(espWiFiClient);

void setup() {
  /* Initialize Variables */
  espId = String(ESP.getChipId(), HEX);
  String sketchName = String(__SKETCH_NAME__);
  hostName = sketchName + "-" + espId;
  mqttHASSConfigTopic = "homeassistant/binary_sensor/" + sketchName + "/" + espId + "/config";
  mqttHASSStateTopic = "homeassistant/binary_sensor/" + sketchName + "/" + espId + "/state";
  mqttHASSCommandTopic = "homeassistant/binary_sensor/" + sketchName + "/" + espId + "/command";

  /* Configure Hardware */
  pinMode(belLedPin, OUTPUT);
  pinMode(wifiLedPin, OUTPUT);
  pinMode(mqttLedPin, OUTPUT);
  pinMode(relayPin, OUTPUT);

  belButton = new ButtonInput( inPin, true, true);
  testButton = new ButtonInput( testPin, true, true);

  Serial.begin(115200);
  Serial.println("---- Booting ----");

  Serial.println("---- Intializing FS ----");
  SPIFFS.begin();

  if (testButton->getPressed() == true) {
    Serial.println("---- Test button Pressed during boot, erasing Settings ----");
    wm.erase();
  }

  Serial.println("-> Connecting to WiFi...");
  setupWiFiManager();

  fsReadConfigFile();

  bool res = wm.autoConnect(__AP_MODE_SSID__, __AP_MODE_PASSWORD__);
  if (!res)
  {
    Serial.println("WiFi FAILED to connect!");
  }
  else
  {
    Serial.println("WiFi CONNECTED!");
    setupOTA();

    wm.startWebPortal();
  }
}

void setupOTA() {
  Serial.println("Starting OTA!");
  ArduinoOTA.setHostname( hostName.c_str());
  ArduinoOTA.setPassword( __AP_MODE_PASSWORD__);

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();
}

bool connectToMQTT() {
  bool connectedToMqtt = pubSubClient.connected();
  if (!connectedToMqtt) {
    const char* mqttServerIP = parameterMQTTServerIP->getValue();
    const char* mqttServerPort = parameterMQTTServerPort->getValue();
    const char* mqttUsername = parameterMQTTUsername->getValue();
    const char* mqttPassword = parameterMQTTPassword->getValue();
    const char* mqttBirthTopic = parameterMQTTHASSBirthTopic->getValue();

    Serial.print("-> Connecting to MQTT Server: ");
    Serial.print(mqttServerIP);
    Serial.print(":");
    Serial.println(mqttServerPort);

    pubSubClient.setBufferSize( 1024);
    pubSubClient.setServer(mqttServerIP, atoi(mqttServerPort));
    pubSubClient.setCallback(mqttCallback);

    if (pubSubClient.connect(hostName.c_str(), mqttUsername, mqttPassword))
    {
      connectedToMqtt = true;
     
      // Subscribe to all required topics
      const char* mqttCommandTopic = mqttHASSCommandTopic.c_str();
      if (pubSubClient.subscribe(mqttBirthTopic))
      {
        Serial.print("MQTT SUBSCRIBED To: ");
        Serial.println(mqttBirthTopic);
      }

      Serial.println("MQTT CONNECTED !");
      if (pubSubClient.subscribe(mqttCommandTopic))
      {
        Serial.print("MQTT SUBSCRIBED To: ");
        Serial.println(mqttCommandTopic);
      }

      hassConfigRequested = 1;
    }
    else
    {
      Serial.print("MQTT FAILED to connect with state: ");
      Serial.println(pubSubClient.state());
      delay(2000);
    }    
  }

  return connectedToMqtt;
}

void sendHassConfig() {

  hassConfigRequested = 0;

  Serial.print("Sending HASS Config: ");

  Serial.print("Topic: ");
  Serial.println(mqttHASSConfigTopic);

  String uniqueId = hostName + "/Ringer";

  const size_t capacity = JSON_OBJECT_SIZE(8);
  DynamicJsonDocument doc = createHassDeviceConfig( capacity);
  
  doc["payload_off"] = "idle";
  doc["payload_on"] = "ringing";
  doc["value_template"] = "{{ value_json.status }}";
  doc["state_topic"] = mqttHASSStateTopic.c_str();
  doc["device_class"] = "occupancy";
  doc["name"] = uniqueId.c_str();
  doc["unique_id"] = uniqueId.c_str();

  char mqttMessageBody[1000];
  serializeJson(doc, mqttMessageBody);

  Serial.print("Message:");
  Serial.println(mqttMessageBody);

  bool success = pubSubClient.publish(mqttHASSConfigTopic.c_str(), mqttMessageBody);
  if (!success)
    Serial.println("Publish Failed");
}

DynamicJsonDocument createHassDeviceConfig(size_t outerDocCapacity) {
  const size_t capacity = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(5) + outerDocCapacity;
  DynamicJsonDocument doc(capacity);

  JsonObject device = doc.createNestedObject("device");
  JsonArray device_identifiers = device.createNestedArray("identifiers");
  device_identifiers.add(hostName.c_str());
  device["manufacturer"] = "Erik Tamminga";
  device["model"] = "Deurbel PCB";
  device["name"] = hostName.c_str();
  device["sw_version"] = "21.1.30 HASS";
  
  return doc;
}

void fsReadConfigFile() {
  Serial.println( "-> Reading config from SPIFFS.");

  String path = "/config.json";
  if (!SPIFFS.exists(path)) {
    Serial.println( "Config file not yet found!");
  }
  else {
    // If the file exists
    Serial.println( "Config file found!");
    File configFile = SPIFFS.open(path, "r");                 // Open it
    if (configFile) {
      Serial.println( "Config file opened");
      DynamicJsonDocument jsonDoc(2048);
      deserializeJson( jsonDoc, configFile);
      configFile.close();

      if (jsonDoc.containsKey("mqtt_server_ip"))
        setParameterValue(parameterMQTTServerIP, jsonDoc["mqtt_server_ip"]);
      if (jsonDoc.containsKey("mqtt_server_port"))
        setParameterValue(parameterMQTTServerPort, jsonDoc["mqtt_server_port"]);
      if (jsonDoc.containsKey("mqtt_username"))
        setParameterValue(parameterMQTTUsername, jsonDoc["mqtt_username"]);
      if (jsonDoc.containsKey("mqtt_password"))
        setParameterValue(parameterMQTTPassword, jsonDoc["mqtt_password"]);
      if (jsonDoc.containsKey("mqtt_birth_topic"))
        setParameterValue(parameterMQTTHASSBirthTopic, jsonDoc["mqtt_birth_topic"]);

      Serial.print( "Config file: MQTT Server: ");
      Serial.print( jsonDoc["mqtt_server_ip"].as<char*>());
      Serial.print( ", Port: ");
      Serial.print( jsonDoc["mqtt_server_port"].as<char*>());
      Serial.print( ", Username: ");
      Serial.print( jsonDoc["mqtt_username"].as<char*>());
      Serial.print( ", Password: ");
      Serial.println( jsonDoc["mqtt_password"].as<char*>());

      Serial.print( "HASS Birth Topic: ");
      Serial.println( jsonDoc["mqtt_birth_topic"].as<char*>());
    }
  }
}

void setParameterValue( WiFiManagerParameter* parameter, const char * value) {
  parameter->setValue( value);
}

void setupWiFiManager()
{
  WiFi.mode(WIFI_STA); /* Do not launch AP mode unless autoConnect fails, default = STA+AP */

  wm.setHostname(hostName.c_str());

  /* Setup configuration parameters */
  parameterMQTTServerIP = new WiFiManagerParameter("MQTTServerIP", "MQTT Server IP", defaultMQTTServerIP, 100);
  parameterMQTTServerPort = new WiFiManagerParameter("MQTTServerPort", "MQTT Server Port", defaultMQTTPort, 10);
  parameterMQTTUsername = new WiFiManagerParameter("MQTTUsername", "MQTT Username", defaultMQTTUsername, 40);
  parameterMQTTPassword = new WiFiManagerParameter("MQTTPassword", "MQTT Password", defaultMQTTPassword, 40);
  parameterMQTTHASSBirthTopic = new WiFiManagerParameter("MQTTBirthTopic", "MQTT Birth Topic", defaultMQTTHASSBirthTopic, 100);

  wm.addParameter(parameterMQTTServerIP);
  wm.addParameter(parameterMQTTServerPort);
  wm.addParameter(parameterMQTTUsername);
  wm.addParameter(parameterMQTTPassword);
  wm.addParameter(parameterMQTTHASSBirthTopic);

  /* Setup callbacks */
  wm.setAPCallback(wmConfigModeCallback);
  wm.setSaveConfigCallback(wmSaveConfigCallback);
  wm.setSaveParamsCallback(wmSaveParamsCallback);

  wm.setShowPassword(false); /* Do not show password in UI */
  wm.setEnableConfigPortal(true); /* Enable the portal if autoConnect fails */
  wm.setConfigPortalTimeout(300); /* Disables the portal after 300 sec */
  wm.setBreakAfterConfig(true);
}

//gets called when WiFiManager enters configuration mode
void wmConfigModeCallback (WiFiManager *myWiFiManager)
{
  Serial.println("[CALLBACK] wifiConfigModeCallback fired.");
  Serial.println("WiFiManager Waiting\nIP: " + WiFi.softAPIP().toString() + "\nSSID: " + WiFi.softAPSSID());
}

void wmSaveConfigCallback()
{
  Serial.println("[CALLBACK] saveConfig fired");
}

void wmSaveParamsCallback()
{
  Serial.println("[CALLBACK] saveCallback fired");

  StaticJsonDocument<200> configDoc;
  configDoc["mqtt_server_ip"] = parameterMQTTServerIP->getValue();
  configDoc["mqtt_server_port"] = parameterMQTTServerPort->getValue();
  configDoc["mqtt_username"] = parameterMQTTUsername->getValue();
  configDoc["mqtt_password"] = parameterMQTTPassword->getValue();
  configDoc["mqtt_birth_topic"] = parameterMQTTHASSBirthTopic->getValue();

  File configFile = SPIFFS.open("/config.json", "w");
  serializeJson( configDoc, configFile);
  configFile.close();

  Serial.println( "Configuration file saved to Flash");
}

void mqttCallback(char* topic, byte* payload, unsigned int length)
{
  Serial.print("Message arrived in topic: ");
  Serial.println(topic);
  Serial.print("Message:");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  const char* mqttBirthTopic = parameterMQTTHASSBirthTopic->getValue();
  if (strcmp(topic, mqttBirthTopic) == 0)
  {
    Serial.println("Message is HASS Birth message.");
    hassConfigRequested = 1;
  }
  else if (strcmp(topic, mqttHASSCommandTopic.c_str()) == 0)
  {
    if (length < 200)
    {
      StaticJsonDocument<256> mqttReceivedMessage;
      deserializeJson(mqttReceivedMessage, payload, length);

      if (mqttReceivedMessage["ringer"] == "off")
        settingRingerOn = 0;
      else if (mqttReceivedMessage["ringer"] == "on")
        settingRingerOn = 1;

      if (mqttReceivedMessage["doRestart"] == "yes-sir")
      {
        ESP.restart();
      }

      sendStatusUpdate = 1;
    }
  }
}

void local_yield()
{
  yield();
  pubSubClient.loop();
}

unsigned long elapsed_time(unsigned long start_time_ms)
{
  return millis() - start_time_ms;
}

void local_delay( unsigned long millisecs)
{
  unsigned long start = millis();
  local_yield();
  if (millisecs > 0)
  {
    while (elapsed_time(start) < millisecs) {
      local_yield();
    }
  }
}

void sendHassStatusUpdate( int relayStatus, int settingRingerOn, int freeMem) {
  const int JSON_STATUS_UPDATE_CAPACITY = JSON_OBJECT_SIZE(3);
  StaticJsonDocument<JSON_STATUS_UPDATE_CAPACITY> mqttStatusMessage;

  mqttStatusMessage["status"] = (relayStatus == 1) ? "ringing" : "idle";
  mqttStatusMessage["ringerOn"] = (settingRingerOn == 1) ? "on" : "off";
  mqttStatusMessage["freeMem"] = freeMem;
  
  char mqttMessageBody[250];
  serializeJson(mqttStatusMessage, mqttMessageBody);

  Serial.print("Publishing: ");
  Serial.print("Topic: ");
  Serial.print(mqttHASSStateTopic);
  Serial.print(", Message:");
  Serial.println(mqttMessageBody);

  pubSubClient.publish(mqttHASSStateTopic.c_str(), mqttMessageBody);
}

void loop()
{
  // Call all module handlers
  ArduinoOTA.handle();
  wm.process();

  bool mqttConnected = pubSubClient.connected();
  // Make sure we're connected to MQTT
  if (!mqttConnected) {
    long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      if (connectToMQTT()) {
        lastReconnectAttempt = 0;
        mqttConnected = true;
        sendStatusUpdate = true;
      }
    }
  }
  else {
    pubSubClient.loop();

    if (hassConfigRequested) {
      sendHassConfig();
    }
  }

  digitalWrite( wifiLedPin, espWiFiClient.connected());
  digitalWrite( mqttLedPin, pubSubClient.connected());

  // Do the doorbel logic
  belButton->Test();
  testButton->Test();

  if (belButton->getChanged()) {
    Serial.print("BelButtonState -Changed-: ");
    Serial.println( belButton->getPressed());
    digitalWrite( belLedPin,  belButton->getPressed());
  }
  if (testButton->getChanged()) {
    Serial.print("TestButtonState -Changed-: ");
    Serial.println( testButton->getPressed());
    digitalWrite( belLedPin,  testButton->getPressed());
  }

  if (belButton->getChanged() || testButton->getChanged())
  {
    bool belShouldRing = ((belButton->getPressed() && (settingRingerOn == 1)) || testButton->getPressed());
    if (belShouldRing)
    {
      sendStatusUpdate = 1;
      relayStatus = 1;
      digitalWrite(relayPin, HIGH);  // turn relay ON
    }
    else
    {
      sendStatusUpdate = 1;
      relayStatus = 0;
      digitalWrite(relayPin, LOW);  // turn relay OFF
    }
  }

  if (((loopCounter * loopDelay) / 1000.0F) > 5000)
  {
    Serial.println("Still working.");
  }

  // How many seconds are we waiting
  loopCounter++;
  if ((sendStatusUpdate == 1) || (((loopCounter * loopDelay) / 1000.0F) > statusFrequency))
  {
    sendStatusUpdate = 0;
    loopCounter = 0;

    int freeMem = ESP.getFreeHeap();
    if (freeMem < 20000)
    {
      Serial.println("Restarting ESP because of FreeMem < 20000.");
      local_delay(loopDelay);
      ESP.restart();
    }

    if (mqttConnected)
      sendHassStatusUpdate(relayStatus, settingRingerOn, freeMem);
  }
  /* Give everything a rest until we go again */
  local_delay(loopDelay);
}
