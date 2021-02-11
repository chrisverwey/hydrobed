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
#include <NTPClient.h>

#define BOARDTYPE "HYDROFARM_v1"
#define SERIALDEBUG true
#define HOMESERVER "chris-mbp-3"
#define HOMEPORT 8081

#ifndef STASSID
#define STASSID "WLAN300N"
#define STAPSK  "ourhome1"
#endif

WiFiUDP UDP;

// https://github.com/arduino-libraries/NTPClient
// https://randomnerdtutorials.com/esp8266-nodemcu-date-time-ntp-client-server-arduino/
// You can specify the time server pool and the offset, (in seconds)
// additionaly you can specify the update interval (in milliseconds).
NTPClient timeClient(UDP, "ntp.is.co.za", 7200, 86400000); // 24h refresh

const char* ssid = STASSID;
const char* password = STAPSK;

#define DEBUGLEVEL 1  // 0=INFO 1=DEBUG 2=TRACE
void info(String s) { if (SERIALDEBUG && DEBUGLEVEL>=0)  Serial.println("INFO  : "+s); }
void debug(String s) { if (SERIALDEBUG && DEBUGLEVEL>=1) Serial.println("DEBUG : "+s); }
void trace(String s) { if (SERIALDEBUG && DEBUGLEVEL>=2) Serial.println("TRACE : "+s); }
void errorlog(String s) { Serial.println("ERROR : "+s); }


// ---------------------------------------------------
// ----------------- Main data structs ---------------
// ---------------------------------------------------
typedef struct {
  int activation_id;
  tm start_time;
  byte duration; 
} t_Activation;

typedef struct {
  int pin_id;
  int pin_number;
  int pin_type;
  int alert_high;
  int alert_low;
  int warn_high;
  int warn_low;
  int schedule_count=-1;
  t_Activation schedule[] = {};
} t_Pin;

typedef struct {
  int driver_id;
  int i2c_port;
  int schedule_read_freq = 600;
  int pin_count = -1;
  t_Pin *pins ;
} t_Driver;

int controller_id = NULL;
long checkin_delay = 60;
long checkin_countdown = 60;
tm schedule_time ;

int driver_count = -1;
t_Driver *driver;
bool missingpins = true;
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
  info("\n\n");
  pinMode(LED_BUILTIN, OUTPUT);  
  pinMode(LED_BUILTIN, HIGH);       // Turn off LED to save power
  connectToWiFi();
  setupOTA();
  
  enableWebServer();
  webRegisterWithHome();
  webRefreshSchedule();
  
  timeClient.begin();
  info("Startup complete");
}

/* 
 * Main program loop 
 */
int previousSecond = 70;
void loop() {
  timeClient.update();
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(ssid, password);
  }
  if (timeClient.getSeconds()!=previousSecond) {
    previousSecond = timeClient.getSeconds();

    if (driver_count<0) webDownloadDriverConfig();
    if (driver_count>0 && missingpins) webDownloadPinConfig();
    trace("Timer checkin_countdown:"+String(checkin_countdown));
    if (countdownTimer(checkin_countdown, checkin_delay)) webCheckinWithHome();

    //unsigned long epochTime = timeClient.getEpochTime();
    //struct tm *ptm = gmtime ((time_t *)&epochTime );
    //trace(String(1900+ptm->tm_year)+"/"+String(ptm->tm_mon+1)+"/"+String(ptm->tm_mday)+" "+String(ptm->tm_hour)+":"+String(ptm->tm_min)+":"+String(ptm->tm_sec));
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
boolean countdownTimer(long &timer, long resetvalue) {
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
  StaticJsonDocument<2048> aDoc; // incoming document
  DeserializationError error = deserializeJson(aDoc, resultset);
  debug("stripJSONresultset:aDoc="+String(aDoc.memoryUsage()));
  if (error) {
    info(F("deserializeJson() failed: "));
    info(error.f_str());
    //webLogMessage(3, "Register:updateOurSettings() failed="+String(error.f_str())+":"+payload);
    return "";
  }
  // 
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

/*
 * Utility function to parse a string date/time from JSON 
 * into a tm structure.
 *  http://www.cplusplus.com/reference/ctime/tm/
 */
void parseDateTime(tm &datetime, String s) {
    //2021-02-10T12:34:56.000Z"
    trace("updateOurSettings:schedule received="+s);
    datetime.tm_mday = s.substring(8,10).toInt();
    datetime.tm_mon  = s.substring(5,7).toInt()-1;
    datetime.tm_year = s.substring(0,4).toInt()-1900;
    datetime.tm_hour = s.substring(11,13).toInt();
    datetime.tm_min  = s.substring(14,16).toInt();
    datetime.tm_sec  = s.substring(17,19).toInt();
}

// ---------------------------------------------------
// ----------------- OTA Section ---------------------
// ---------------------------------------------------
void setupOTA() {
  ArduinoOTA.onStart([]() {
      info("Starting OTA");
  });
  
  ArduinoOTA.onEnd([] {
      info("Done with OTA");
  });
  
  ArduinoOTA.onProgress([] (unsigned int progress, unsigned int total) {
      info("Progress: "+String(progress / (total / 100))+"%");
  });
  
  ArduinoOTA.onError([] (ota_error_t err) {
    errorlog("Error[" + String(err)+"]: ");
    if (err == OTA_AUTH_ERROR) errorlog ("Auth failed");
    else if (err == OTA_BEGIN_ERROR) errorlog ("Begin Failed");
    else if (err == OTA_CONNECT_ERROR) errorlog ("Connect Failed");
    else if (err == OTA_RECEIVE_ERROR) errorlog ("Recieve Failed");
    else if (err == OTA_END_ERROR) errorlog ("End Failed");  
  });
  
  ArduinoOTA.begin();
  info("OTA Ready");
  info("IP address: ");
  info(WiFi.localIP().toString());
}

// ---------------------------------------------------
// ----------------- WiFi Section --------------------
// ---------------------------------------------------
void connectToWiFi() {
  // Connect to WiFi network
  info("Mac address:"+WiFi.macAddress());

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    info(F("Connecting"));
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
  StaticJsonDocument<156> doc;
  
  DeserializationError error2 = deserializeJson(doc, data);
  debug("updateOurSettings:doc="+String(doc.memoryUsage()));

  int save_controller_id = controller_id;
  int save_checkin_delay = checkin_delay;

  controller_id = doc["controller_id"];
  checkin_delay = doc["checkin_delay"];
  String schedule = doc["schedule_time"];
  if (schedule.length()>4) { //2021-02-10T12:34:56.000Z"
    parseDateTime(schedule_time, schedule);
  }
  // Protect if server is offline and we get empty data back.
  if (controller_id == 0) controller_id = save_controller_id;
  if (checkin_delay == 0) checkin_delay = save_checkin_delay;
  // If this is our first checkin, controller_id is ok to be 0.
  if (checkin_delay == 0) {
    info("updateOurSettings:Checkin delay invalid. Forcing it to 60"); 
    checkin_delay=60;
  } 
   char buffer [80];
   strftime (buffer,80,"%F %T",&schedule_time);

  debug("updateOurSettings:Setting values controller_id="+String(controller_id)+" checkin_delay="+String(checkin_delay)+" schedule="+buffer);
}

/*
 * Called from the setup routine to report ourselves to the
 * central server and to download our controller settings.
 */
void webRegisterWithHome() {
  HTTPClient client;
  info("webRegisterWithHome:Register with home after startup");
  webSendHeaders(client, "/controller");

  unsigned long mint = millis() / 1000;
  StaticJsonDocument<64> doc;
  doc["mac_address"] = WiFi.macAddress();
  doc["uptime"] = (int) mint;

  String sendString = "";
  serializeJson(doc, sendString);
  debug("webRegisterWithHome:Web Request:"+sendString);
    
  int err = client.POST(sendString);
  String payload = client.getString();
  
  debug("webRegisterWithHome:Web Response:"+payload);
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
  info("webCheckinWithHome:Check in with home periodically");
  webSendHeaders(client, "/controller");

  unsigned long mint = millis() / 1000;
  StaticJsonDocument<64> doc;
  doc["mac_address"] = WiFi.macAddress();
  doc["uptime"] = (int) mint;

  String sendString = "";
  serializeJson(doc, sendString);
  debug("webCheckinWithHome:Web Request:"+sendString);
    
  int err = client.POST(sendString);
  String payload = client.getString();
  
  debug("webCheckinWithHome:Web Response:"+payload);
  updateOurSettings(payload);
  if (controller_id != NULL && isScheduleOutOfDate()) {
    webRefreshSchedule(); 
  }
  client.end();
}

void webDownloadDriverConfig() {
  // GET http://localhost:8081/controller/1/driver
  HTTPClient client;

  info("webDownloadDriverConfig:Downloading driver details...");
  webSendHeaders(client, "/controller/"+String(controller_id)+"/driver");
  int err = client.GET();
  if (err==200) {
    String payload = client.getString();
    debug("webDownloadDriverConfig:Web Response:"+payload);
    StaticJsonDocument<512> doc;
    
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
      errorlog(F("webDownloadDriverConfig:deserializeJson() failed: "));
      errorlog(error.f_str());
    } else {

     driver = (t_Driver *) malloc(sizeof(t_Driver)*doc.size());
     if (driver) { // malloc succeeded
        driver_count = doc["rowsAffected"][0];
        trace ("Driver_Count = "+String(driver_count)); // todo
        int counter = 0;
        for (JsonObject elem : doc["recordsets"][0].as<JsonArray>()) {
          driver[counter].driver_id          = elem["driver_id"]; // 1, 2
          driver[counter].i2c_port           = elem["i2c_port"]; // 1, 69
          driver[counter].schedule_read_freq = elem["schedule_read_freq"]; // 600, 300
          driver[counter].pin_count          = -1; // not yet updated
          counter++;
        }
      }  else {
        errorlog("webDownloadDriverConfig:malloc error"); // TODO malloc error handling
      }
    }
  } else debug ("webDownloadDriverConfig:http error "+String(err)+" getting schedule"); 
  
  client.end();  
}

void webDownloadPinConfig() {
  // GET http://localhost:8081/driver/1/pin
  HTTPClient client;
  missingpins = false;
  info("webDownloadPinConfig:Downloading pin details...");
  
  DynamicJsonDocument doc(3072);
  for (int t=0; t<driver_count; t++) {
    trace("Checking driver board "+String(t)+" having pin count "+String(driver[t].pin_count));
  
    if (driver[t].pin_count < 0) {
      info("webDownloadPinConfig:Driver "+String(driver[t].driver_id)+" needs pin details...");
      webSendHeaders(client, "/driver/"+String(driver[t].driver_id)+"/pin");
      int err = client.GET();
      if (err==200) {
        String payload = client.getString();
        debug("webDownloadPinConfig:Web Response:"+payload);
    
        DeserializationError error = deserializeJson(doc, payload);
        
        if (error) {
          errorlog(F("webDownloadPinConfig:deserializeJson() failed: "));
          errorlog(error.f_str());
          missingpins=true;
        } else {
    
         driver[t].pins = (t_Pin *) malloc(sizeof(t_Pin)*doc.size());
         if (driver[t].pins) { // malloc succeeded
            driver[t].pin_count = doc["rowsAffected"][0];
            int counter = 0;
            for (JsonObject elem : doc["recordsets"][0].as<JsonArray>()) {
              driver[t].pins[counter].pin_id     = elem["pin_id"]; // 1, 2, 3, 4, 5, 6, 7, 8, 9
              driver[t].pins[counter].pin_number = elem["pin_number"]; // 5, 6, 7, 14, 15, 16, 17, 18, 19
              driver[t].pins[counter].pin_type   = elem["pin_type"]; // 1, 1, 1, 2, 2, 2, 2, 2, 2
              driver[t].pins[counter].alert_high = elem["alert_high"];
              driver[t].pins[counter].alert_low  = elem["alert_low"];
              driver[t].pins[counter].warn_high  = elem["warn_high"];
              driver[t].pins[counter].warn_low   = elem["warn_low"];
              counter++;
              downloadPinSchedule(driver[t].pins[counter]);
            }
          } else {
            missingpins = true;  // TODO malloc error handling
            errorlog("webDownloadPinConfig:malloc error");
          }
        }
      } else {
        missingpins=true;
        debug ("webDownloadPinConfig:http error "+String(err)+" getting schedule"); 
      }
      client.end();   
    }
  }
}

void downloadPinSchedule(t_Pin &pin) {
  // GET http://localhost:8081/pin/1/schedule
  HTTPClient client;
  missingpins = false;
  debug("webDownloadPinConfig:Updating pin schedule "+String(pin.pin_id));
}

/*
 * TODO : Download the drivers, pins and activations
 * and update our internal variables
 */
void webRefreshSchedule() {



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
