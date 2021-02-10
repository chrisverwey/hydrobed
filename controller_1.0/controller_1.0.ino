/** WeMos D1 R2 code
  The controller is used as a master device to control slave boards hosting the 5V components.
  It sends instructions via I2C to these driver boards.

  This code uses HTTP to to read control information and report its status to a master server.
  
  Commands to the slaves can be:
    Device on commands 
      <in> nd   n is the pinumber
                d is the duration to be on in seconds, max 255 
      e.g. 1,30 will turn digital pin 1 on for 30 seconds. 
    
    Sensor read commands
      <out> unit_16[] {n,v,n,v...}  
                  n is the pin number
                  v is the value, left shifted 2 digits (e.g.12.34 transmits as 1234}
      e.g. 1,84.80,2,43.99 becomes {1,84,80,2,43,99} 
**/
#include <ESP8266WiFi.h>
//#include <Wire.h>

#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <TimeLib.h>

#define BOARDTYPE "HYDROFARM_v1"
#define SERIALDEBUG true
#define DEBUGLEVEL 1  // 0=INFO 1=DEBUG 2=TRACE
#define HOMESERVER "chris-mbp-3"
#define HOMEPORT 8081

#ifndef STASSID
#define STASSID "WLAN300N"
#define STAPSK  "ourhome1"
#endif

void info(String s) { if (SERIALDEBUG && DEBUGLEVEL>=0) Serial.println(s); }
void debug(String s) { if (SERIALDEBUG && DEBUGLEVEL>=1) Serial.println(s); }
void trace(String s) { if (SERIALDEBUG && DEBUGLEVEL>=2) Serial.println(s); }


const char* ssid = STASSID;
const char* password = STAPSK;

// ---------------------------------------------------
// ----------------- Main data structs ---------------
// ---------------------------------------------------
typedef struct {
  int activation_id;
  // t_time start_time;
  byte duration;
} t_Activation;

typedef struct {
  int pin_id;
  int pin_number;
  t_Activation schedule[] = {};
} t_Pin;

typedef struct {
  int driver_id;
  int i2c_port;
  int schedule_read_freq = 600;
  t_Pin pins[] = {};
} t_Driver;

int controller_id = NULL;
int checkin_delay = 60;
int checkin_countdown = 0;
// t_time schedule_time = null;
t_Driver driver[] = {};

// ---------------------------------------------------
// ----------------- Arduino core methods ------------
// ---------------------------------------------------
// TODO Move this down to Wifi section.
WiFiServer server(80);  

/*
 * Initialization 
 */
void setup() {
  if (SERIALDEBUG) 
    Serial.begin(115200);           // Enable serial monitor
    
  pinMode(LED_BUILTIN, OUTPUT);  
  pinMode(LED_BUILTIN, HIGH);       // Turn off LED to save power
  connectToWiFi();
  setupOTA();
  enableWebServer();
  
  webRegisterWithHome();

  info("Startup complete");
}

/* 
 * Main program loop 
 */
int previousSecond = 70;
void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(ssid, password);
  }
  if (second()!=previousSecond) {
    previousSecond = second();
    trace("Timer checkin_countdown:"+String(checkin_countdown));
    if (countdownTimer(checkin_countdown, checkin_delay)) webCheckinWithHome();
  }
  ArduinoOTA.handle();
  WiFiClient client = server.available();
//  if (client) 
//    webClientRquest(client);
//  }
  bool hadAClient = false;
  while (client.available()) {
    hadAClient = true;
    char c = client.read();
    Serial.write(c);
  }

  // if the server's disconnected, stop the client:
  if (hadAClient && !client.connected()) {
    Serial.println();
    Serial.println("disconnecting from server.");
    client.stop();
  }
  delay(100);
}

// ---------------------------------------------------
// ----------------- Utility methods -----------------
// ---------------------------------------------------
/*
 * Utility method to count down the value of a timer
 * until it is zero, then reset it.
 * The timer value is passed by reference &timer so that
 * this method can update the value.
 */
boolean countdownTimer(int &timer, int resetvalue) {
  timer --;
  if (timer<1) {
    trace("Reset value "+String(timer)+" to "+String(resetvalue));
    timer = resetvalue;
    return true;
  }
  else
    return false;
}

/*
 * aDoc's recordsets need to be extracted. The payload coming back looks like 
 *    recordsets":[[{"controller_id":, etc.
 * 
 * The recordsets bit needs to be extracted so that the main payload can be used.
 */
String stripJSONresultset (String resultset) {
  StaticJsonDocument<384> aDoc; // incoming document
  DeserializationError error = deserializeJson(aDoc, resultset);

  if (error) {
    info(F("deserializeJson() failed: "));
    info(error.f_str());
    //webLogMessage(3, "Register:updateOurSettings() failed="+String(error.f_str())+":"+payload);
    return "";
  }
  // 
  StaticJsonDocument<384> subDoc;
  String subDocString = aDoc["recordsets"][0][0];
  return subDocString;
}

/* 
 *  TODO: calculate whether our schedule timestamp is the same as 
 *  the one from the server.
 */
boolean isScheduleOutOfDate() {
  return true;
}

// ---------------------------------------------------
// ----------------- OTA Section ---------------------
// ---------------------------------------------------
void setupOTA() {
  ArduinoOTA.onStart([]() {
      Serial.println ("Starting OTA");
  });
  
  ArduinoOTA.onEnd([] {
      Serial.println ("Done with OTA");
  });
  
  ArduinoOTA.onProgress([] (unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\n", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([] (ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println ("Auth failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println ("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println ("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println ("Recieve Failed");
    else if (error == OTA_END_ERROR) Serial.println ("End Failed");  
  });
  
  ArduinoOTA.begin();
  Serial.println ("Ready");
  Serial.print ("IP address: ");
  Serial.println (WiFi.localIP());
}
// ---------------------------------------------------
// ----------------- WiFi Section --------------------
// ---------------------------------------------------
void connectToWiFi() {
  // Connect to WiFi network
  info("\n\nMac address:"+WiFi.macAddress());
  info(F("Connecting"));

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    info(F("."));
  }
  info(F("WiFi connected"));  
//  IPAddress ip = WiFi.localIP(); 
//  info(ip.toString());
}

void enableWebServer() {
  // Start the server
  server.begin();
  info(F("Server started"));
}

void webClientRequest(WiFiClient client) {
    // Check if a client has connected

  client.setTimeout(5000); // default is 1000

  // Read the first line of the request
  String req = client.readStringUntil('\r');
  debug(F("request: "));
  debug(req);

  // Match the request
//  int val;
//  if (req.indexOf(F("/gpio/0")) != -1) {
//    val = 0;
//  } else if (req.indexOf(F("/gpio/1")) != -1) {
//    val = 1;
//    debug("writing to i2c"); 
//    Wire.beginTransmission(8);
//    Wire.write("hello               "); 
//    Wire.endTransmission();
//  delay(500);
//  } else {
//    info(F("invalid request"));
//    val = digitalRead(LED_BUILTIN);
//  }

  // read/ignore the rest of the request
  // do not client.flush(): it is for output only, see below
  while (client.available()) {
    // byte by byte is not very efficient
    client.read();
  }

  // Send the response to the client
  // it is OK for multiple small client.print/write,
  // because nagle algorithm will group them into one single packet
//  client.print(F("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>\r\nGPIO is now "));
//  client.print("<HTML><body><h1>Not yet supported</h1></body></HTML>");

  // The client will actually be *flushed* then disconnected
  // when the function returns and 'client' object is destroyed (out-of-scope)
  // flush = ensure written data are received by the other side
  debug(F("Disconnecting from client"));
}

/*
 * This method sets up the header of a HTTP request.
 * WiFiClient must be passed by reference, otherwise the connection is closed
 * once the "client" variable on the stack disposed of. The rest of the payload
 * is then lost.
 * method = POST/GET/PUT/DELET, etc.
 * url    = the http path to call e.g. /some/path/?with=variables
*/
String webSendHeaders(HTTPClient &http, String url) {
  String server = String(HOMESERVER);
  String port = String(HOMEPORT);
  http.begin("http://"+server+":"+port+url);
  http.addHeader("Content-Type", "application/json");
}

/*
 * Utility method to log some or other message in the central server
 * database. 
 *    loglevel 1=INFO, 2=WARNING, 3=ALERT/ERROR!!
 * Don't forget to format your message nicely for searching and extracting data
 * e.g. method that logged:a searchable message keyword,variable list
 * e.g. checkSensors:The farm exploded!,driver=0x43, sensorpin=1, value=189234
 */
void webSendLoggingMessage(int loglevel, String message) {
  HTTPClient client;
  info("Sending ("+String(loglevel)+") message home : "+message);
  webSendHeaders(client, "/logmessage");
  StaticJsonDocument<128> doc;
  doc["controller"]=controller_id;
  doc["priority"]=loglevel;
  doc["message"]=message;
  String sendString = "";
  serializeJson(doc, sendString);
  int err = client.POST(sendString);
  String rsp = client.getString();
  if (rsp!=NULL) info ("webLogMessage:error - unexpected reply during logging : "+rsp);
  client.end();
}

/*
 * Utility method to update our controller settings after calling the 
 * main server registration (both from setup, and periodially from
 * loop).
 */
void updateOurSettings(String payload) {
  String data = stripJSONresultset(payload);
  StaticJsonDocument<384> doc;
  
  DeserializationError error2 = deserializeJson(doc, data);

  controller_id = doc["controller_id"];
  checkin_delay = doc["checkin_delay"];
  
  debug("Setting values controller_id="+String(controller_id)+" checkin_delay="+String(checkin_delay));
//// TODO t_time schedule_time = null;
}

/*
 * Called from the setup routine to report ourselves to the
 * central server and to download our controller settings.
 */
void webRegisterWithHome() {
  HTTPClient client;
  info("Register with home after startup");
  webSendHeaders(client, "/controller");

  unsigned long mint = millis() / 1000;
  StaticJsonDocument<64> doc;
  doc["mac_address"] = WiFi.macAddress();
  doc["uptime"] = (int) mint;

  String sendString = "";
  serializeJson(doc, sendString);
  debug("Web Request:"+sendString);
    
  int err = client.POST(sendString);
  String payload = client.getString();
  
  debug("Web Response:"+payload);
  updateOurSettings(payload);
  
  client.end();
}

/*
 * Periodically called from main loop to check in with the
 * central server to report our uptime, and to check whether
 * there is a newer version of the schedule that we don't have.
 */
void webCheckinWithHome() {
  HTTPClient client;
  info("Check in with home periodically");
  webSendHeaders(client, "/controller");

  unsigned long mint = millis() / 1000;
  StaticJsonDocument<64> doc;
  doc["mac_address"] = WiFi.macAddress();
  doc["uptime"] = (int) mint;

  String sendString = "";
  serializeJson(doc, sendString);
  debug("Web Request:"+sendString);
    
  int err = client.POST(sendString);
  String payload = client.getString();
  
  debug("Web Response:"+payload);
  updateOurSettings(payload);
  if (controller_id != NULL && isScheduleOutOfDate()) {
    webRefreshSchedule(); 
  }
  client.end();
}

/*
 * TODO : Download the drivers, pins and activations
 * and update our internal variables
 */
void webRefreshSchedule() {
  // GET http://localhost:8081/controller/1/configuration
  // Unpacked reply={"driver_id":1,"i2c_port":1,"schedule_read_freq":600,"pin_id":1,"pin_number":5,"activation_id":1,"start_time":"1900-01-01T06:57:00.000Z","duration":30},{"driver_id":1,"i2c_port":1,"schedule_read_freq":600,"pin_id":1,"pin_number":5,"activation_id":2,"start_time":"1900-01-01T06:55:00.000Z","duration":30},{"driver_id":1,"i2c_port":1,"schedule_read_freq":600,"pin_id":1,"pin_number":5,"activation_id":3,"start_time":"1900-01-01T15:00:00.000Z","duration":30},{"driver_id":1,"i2c_port":1,"schedule_read_freq":600,"pin_id":1,"pin_number":5,"activation_id":4,"start_time":"1900-01-01T15:02:00.000Z","duration":30}
}

// ---------------------------------------------------
// ----------------- I2C Section ---------------------
// ---------------------------------------------------
void i2c_setup() {
  
}

// ---------------------------------------------------
// ----------------- Power monitor section -----------
// ---------------------------------------------------

// ---------------------------------------------------
// ----------------- Power monitor section -----------
// ---------------------------------------------------