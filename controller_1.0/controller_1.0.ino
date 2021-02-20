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
#include <Wire.h>

#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <NTPClient.h>
#include "structs.h"
#include "secrets.h"

#define BOARDTYPE "HYDROFARM_v1"
#define SERIALDEBUG true

#define HOMEPORT 8081

#define SDA_PIN 4
#define SCL_PIN 5

const char* ssid = STASSID;
const char* password = STAPSK;

WiFiUDP UDP;

// https://github.com/arduino-libraries/NTPClient
// https://randomnerdtutorials.com/esp8266-nodemcu-date-time-ntp-client-server-arduino/
// You can specify the time server pool and the offset, (in seconds)
// additionaly you can specify the update interval (in milliseconds).
NTPClient timeClient(UDP, "ntp.is.co.za", 7200, 86400000); // 24h refresh

#define DEBUGLEVEL 1  // 0=INFO 1=DEBUG 2=TRACE
void info(String s) { if (SERIALDEBUG && DEBUGLEVEL>=0)  Serial.println("INFO  : "+s); Serial.flush();}
void debug(String s) { if (SERIALDEBUG && DEBUGLEVEL>=1) Serial.println("DEBUG : "+s); Serial.flush();}
void trace(String s) { if (SERIALDEBUG && DEBUGLEVEL>=2) Serial.println("TRACE : "+s); Serial.flush();}
void errorlog(String s) { Serial.println("ERROR : "+s);  Serial.flush();}

DynamicJsonDocument doc(3072);

int controller_id = NULL;
long checkin_delay = 10;
long checkin_countdown = 10;

int driver_count = -1;
t_Driver driver[MAXDRIVERS];
bool missingpins = true;
time_t latest_schedule_read;

// ---------------------------------------------------
// ----------------- Arduino core methods ------------
// ---------------------------------------------------
// TODO Move this down to Wifi section.
WiFiServer server(80);  

/*
 * Initialization 
 */
void setup() {
   trace("setup:start");
  if (SERIALDEBUG) 
    Serial.begin(115200);           // Enable serial monitor
  info("\n\n");
  pinMode(LED_BUILTIN, OUTPUT);  
  pinMode(LED_BUILTIN, HIGH);       // Turn off LED to save power
  connectToWiFi();
  setupOTA();
  
  enableWebServer();
  webRegisterWithHome();
  i2c_setup();
  
  timeClient.begin();
  info("Startup complete");
  trace("validation");  
//  treedump();
  trace("setup:end");  
  
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

  // Do this every second
  if (timeClient.getSeconds()!=previousSecond) {
    previousSecond = timeClient.getSeconds();

    // if our driver dl failed previously, retry
    // if driver succeeded, but any pins failed, retry
    if (driver_count<0) webDownloadDriverConfig(); 
    if (driver_count>0 && missingpins) {
      webDownloadPinConfig();
      if (!missingpins) webRefreshSchedule(); // initial schedule update
    }

    // decrement any timers and check if they are 0
//    trace("Timer checkin_countdown:"+String(checkin_countdown));
    if (countdownTimer(checkin_countdown, checkin_delay)) webCheckinWithHome();

    scheduleCheck();
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
 * Use this during debugging to see what values are being
 * stored in the device hierarchy.
 */
void treedump() {
  for (int d=0;d<MAXDRIVERS;d++) 
    for (int p=0;p<MAXPINS;p++)
      for (int s=0;s<MAXSCHEDULES;s++)
       debug("TREE_TEST: ("+String(d)+","+String(p)+","+String(s)+") "+String(driver[d].pins[p].schedule[s].activation_id));
}

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
  trace("stripJSONresultset:start");
//  StaticJsonDocument<2048> aDoc; // incoming document
  DeserializationError error = deserializeJson(doc, resultset);
  trace("stripJSONresultset:aDoc="+String(doc.memoryUsage()));
  if (error) {
    errorlog(F("deserializeJson() failed: "));
    errorlog(error.f_str());
    //webLogMessage(3, "Register:updateOurSettings() failed="+String(error.f_str())+":"+payload);
    return "";
  }
  // 
  String subDocString = doc["recordsets"][0][0];
  trace("stripJSONresultset:end");  
  return subDocString;
}

/* 
 *  Calculates whether our schedule timestamp is older than
 *  the one from the server.
 */
boolean isScheduleOutOfDate(time_t last_schedule) {
  trace("isScheduleOutOfDate last_schedule="+String(last_schedule)+" latest_schedule_read="+String(latest_schedule_read));
  if (last_schedule > latest_schedule_read) {
    info("There is a newer schedule");
    return true;
  }
  info("Schedule is up to date");
  return false;
}

/*
 * Utility function to parse a string date/time from JSON 
 * into a tm structure.
 *  http://www.cplusplus.com/reference/ctime/tm/
 *  Caused by random reboots because of undiagnosed 
 *  memory overwrites.
 */
time_t parseDateTime(String s) {
  trace("parseDateTime:start="+s);
    tm tmptime;
    //2021-02-10T12:34:56.000Z"
    tmptime.tm_mday = s.substring(8,10).toInt();
    tmptime.tm_mon  = s.substring(5,7).toInt()-1;
    tmptime.tm_year = s.substring(0,4).toInt()-1900;
    tmptime.tm_hour = s.substring(11,13).toInt();
    tmptime.tm_min  = s.substring(14,16).toInt();
    tmptime.tm_sec  = s.substring(17,19).toInt();
  trace("parseDateTime:end="+String(mktime(&tmptime)));  
    return mktime(&tmptime);
}

// ---------------------------------------------------
// ----------------- OTA Section ---------------------
// ---------------------------------------------------
void setupOTA() {
  trace("setupOTA:start");
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
  trace("setupOTA:end");  
}

// ---------------------------------------------------
// ----------------- WiFi Section --------------------
// ---------------------------------------------------
void connectToWiFi() {
  trace("connectToWiFi:start");
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
  trace("connectToWiFi:end");
}

void enableWebServer() {
  trace("enableWebServer:start");
  // Start the server
  server.begin();
  info(F("Server started"));
  trace("enableWebServer:end");  
}

void webClientRequest(WiFiClient client) {
  trace("webClientRequest:start");
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
  trace("webClientRequest:end");  
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
  trace("webSendHeaders:start "+url);
  String server = String(HOMESERVER);
  String port = String(HOMEPORT);
  http.begin("http://"+server+":"+port+url);
  http.addHeader("Content-Type", "application/json");
  trace("webSendHeaders:end");  
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
  trace("webSendLoggingMessage:start");
  HTTPClient client;
  debug("webSendLoggingMessage:Sending (level "+String(loglevel)+") message home: "+message);
  webSendHeaders(client, "/logmessage");
//  StaticJsonDocument<128> doc;
  doc["controller"]=controller_id;
  doc["priority"]=loglevel;
  doc["message"]=message;
  String sendString = "";
  serializeJson(doc, sendString);
  int err = client.POST(sendString);
  String rsp = client.getString();
  if (err!=200) errorlog ("webLogMessage:error - unexpected reply during logging : "+String(err));
  client.end();
  trace("webSendLoggingMessage:end");  
}

/*
 * Utility method to update our controller settings after calling the 
 * main server registration (both from setup, and periodially from
 * loop).
 */
time_t updateOurSettings(String payload) {
  time_t returntime ;
  trace("updateOurSettings:start");
  String data = stripJSONresultset(payload);
  
  DeserializationError error2 = deserializeJson(doc, data);
  trace("updateOurSettings:doc="+String(doc.memoryUsage()));

  int save_controller_id = controller_id;
  int save_checkin_delay = checkin_delay;

  controller_id = doc["controller_id"];
  checkin_delay = doc["checkin_delay"];

// Convert this to return from the function:
  if (!doc["schedule_time"].isNull()) { //2021-02-10T12:34:56.000Z"
    String schedule = doc["schedule_time"];
    returntime = parseDateTime(schedule);
  }

  // Protect if server is offline and we get empty data back.
  if (controller_id == 0) controller_id = save_controller_id;
  if (checkin_delay == 0) checkin_delay = save_checkin_delay;
  // If this is our first checkin, controller_id is ok to be 0.
  if (checkin_delay == 0) {
    info("updateOurSettings:Checkin delay invalid. Forcing it to 60"); 
    checkin_delay=60;
  } 
  
  debug("updateOurSettings:Setting values controller_id="+String(controller_id)+" checkin_delay="+String(checkin_delay));
  trace("updateOurSettings:end");  
  return returntime;
}

/*
 * Called from the setup routine to report ourselves to the
 * central server and to download our controller settings.
 */
void webRegisterWithHome() {
  trace("webRegisterWithHome:start");
  HTTPClient client;
  info("webRegisterWithHome:Register with home after startup");
  webSendHeaders(client, "/controller");

  unsigned long mint = millis() / 1000;
//  StaticJsonDocument<64> doc;
  doc["mac_address"] = WiFi.macAddress();
  doc["uptime"] = (int) mint;

  String sendString = "";
  serializeJson(doc, sendString);
  debug("webRegisterWithHome:Web Request:"+sendString);
    
  int err = client.POST(sendString);
  String payload = client.getString();
  
  debug("webRegisterWithHome:Web Response:"+payload);
  time_t ignore_first_setup = updateOurSettings(payload);
  
  client.end();
  trace("webRegisterWithHome:end");  
}

void webDownloadDriverConfig() {
  trace("webDownloadDriverConfig:start");
  // GET http://localhost:8081/controller/1/driver
  HTTPClient client;

  info("webDownloadDriverConfig:Downloading driver details...");
  webSendHeaders(client, "/controller/"+String(controller_id)+"/driver");
  int err = client.GET();
  if (err==200) {
    String payload = client.getString();
    debug("webDownloadDriverConfig:Web Response:"+payload);
//    StaticJsonDocument<512> doc;
    
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
      errorlog(F("webDownloadDriverConfig:deserializeJson() failed: "));
      errorlog(error.f_str());
    } else {
      driver_count = doc["rowsAffected"][0];
      trace ("Driver_Count = "+String(driver_count));
      int counter = 0;
      for (JsonObject elem : doc["recordsets"][0].as<JsonArray>()) {
        driver[counter].driver_id          = elem["driver_id"]; // 1, 2
        driver[counter].i2c_port           = elem["i2c_port"]; // 1, 69
        driver[counter].schedule_read_freq = elem["schedule_read_freq"]; // 600, 300
        counter++;
      }
    }
  } else debug ("webDownloadDriverConfig:http error "+String(err)+" getting schedule"); 
  
  client.end();  
  trace("webDownloadDriverConfig:end");  
}

void webDownloadPinConfig() {
  trace("webDownloadPinConfig:start");
  // GET http://localhost:8081/driver/1/pin
  missingpins = false;
  info("webDownloadPinConfig:Downloading pin details...");
  
  for (int t=0; t<driver_count; t++) {
    HTTPClient client;
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
          // TODO http error if count > max allowed
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
          }
        }
      } else {
        missingpins=true;
        debug ("webDownloadPinConfig:http error "+String(err)+" getting schedule"); 
      }
      client.end();   
    }
//    if (!missingpins) downloadDriverSchedule(driver[t]);
  }
  trace("webDownloadPinConfig:end");  
}

boolean downloadDriverSchedule(int d) {
  boolean successful = true;
  
  trace("downloadDriverSchedule:start");
  // GET http://localhost:8081/pin/1/schedule
  info("downloadDriverSchedule:Updating driver "+String(driver[d].driver_id) + " schedule for " + driver[d].pin_count + " pins" );
  for (int t=0; t<driver[d].pin_count; t++) {
    HTTPClient client;
    webSendHeaders(client, "/pin/"+String(driver[d].pins[t].pin_id)+"/schedule");
    int err = client.GET();
    if (err==200) {

      String payload = client.getString();
      debug("downloadDriverSchedule:Web Response:"+payload);
      DeserializationError error = deserializeJson(doc, payload);
      if (error) {
        errorlog(F("downloadDriverSchedule:deserializeJson() failed: "));
        errorlog(error.f_str());
        successful = false;
      } else {
        int rows = doc["rowsAffected"][0];
        driver[d].pins[t].schedule_count = rows;
        trace("downloadDriverSchedule:"+String(d)+","+String(t)+":"+rows);

        int counter = 0;
        for (JsonObject elem : doc["recordsets"][0].as<JsonArray>()) {
          driver[d].pins[t].schedule[counter].activation_id = elem["activation_id"]; // 1, 2, 3, 4
          driver[d].pins[t].schedule[counter].duration      = elem["duration"]; // 30, 30, 30, 30
          String d1 = elem["start_time"];
          trace("elem('start_time']="+d1);
          if (!elem["start_time"].isNull()) {  // null=4
            driver[d].pins[t].schedule[counter].start_time = parseDateTime(elem["start_time"]);
            trace("downloadDriverSchedule:start_time="+String(driver[d].pins[t].schedule[counter].start_time));
          } else {
            successful = false;
            errorlog ("downloadDriverSchedule:Expected start_time, got "+d1);
          }

          String d2 = elem["end_time"];
          trace("elem('end_time']="+d2);
          if (!elem["end_time"].isNull()) {  // null=4
            driver[d].pins[t].schedule[counter].end_time = parseDateTime(elem["end_time"]);
            trace("downloadDriverSchedule:end_time="+String(driver[d].pins[t].schedule[counter].end_time));
          } else {
            errorlog ("downloadDriverSchedule:Expected end_time, got "+d2);
            successful = false;
          }
//            
          counter++;
        }
      }
    } else {
      successful = false;
      errorlog ("downloadDriverSchedule:http error "+String(err)+" getting driver schedule"); 
    }
    client.end();   
  } 
  trace("downloadDriverSchedule:end="+String(successful));
  return successful;
}
/*
 * TODO : Get the updated schedule
 */
boolean webRefreshSchedule() {
  trace("webRefreshSchedule:start");
  bool completed = true;
  for (int d=0; d<driver_count; d++) 
    completed = completed & downloadDriverSchedule(d);
  trace("webRefreshSchedule:end="+String(completed));  
  return completed;
}

void webLogCurrentConfig(int level) {
  trace("webLogCurrentConfig:start");
  String logvalues = 
    "settings={'controller':"       +String(controller_id)+","+
     "'checkin_delay':"    +String(checkin_delay)+","+
     "'checkin_countdown':"+String(checkin_countdown)+", 'drivers':[";
  for (int d=0; d<driver_count; d++) {
    if (d>0) logvalues = logvalues + ",";
    logvalues = logvalues + "{" +
        "'driver_id':"         + String(driver[d].driver_id)+","+
        "'i2c_port':"          + String(driver[d].i2c_port)+","+
        "'schedule_read_freq':"+ String(driver[d].schedule_read_freq)+","+
        "'pins':[";
    for(int p=0; p<driver[d].pin_count;p++) {
        if (p>0) logvalues = logvalues + ",";
        logvalues = logvalues + "{" + 
        "'pin_id':"    + String(driver[d].pins[p].pin_id)+","+
        "'pin_number':"+ String(driver[d].pins[p].pin_number)+","+
        "'pin_type':"  + String(driver[d].pins[p].pin_type)+","+
        "'alert_high':"+ String(driver[d].pins[p].alert_high)+","+
        "'alert_low':" + String(driver[d].pins[p].alert_low)+","+
        "'warn_high':" + String(driver[d].pins[p].warn_high)+","+
        "'warn_low':"  + String(driver[d].pins[p].warn_low)+","+
        "'schedule':["  ;
      for (int a=0; a<driver[d].pins[p].schedule_count; a++) {
        if (a>0) logvalues = logvalues + ",";
        logvalues = logvalues + "{"+
        "'activation_id':" + String(driver[d].pins[p].schedule[a].activation_id) +"," +
        "'start_time':'"  + String("TIME HERE")+"'," +
        "'duration':"      + String(driver[d].pins[p].schedule[a].duration) + "}";
      }
      logvalues = logvalues + "]}";
    }
    logvalues = logvalues + "]}";
  }
  logvalues = logvalues + "]}";

  webSendLoggingMessage(level,logvalues);
  trace("webLogCurrentConfig:end");  
}

/*
 * Periodically called from main loop to check in with the
 * central server to report our uptime, and to check whether
 * there is a newer version of the schedule that we don't have.
 */
void webCheckinWithHome() {
  trace("webCheckinWithHome:start");
  HTTPClient client;
  info("webCheckinWithHome:Check in with home periodically");
  webSendHeaders(client, "/controller");

  unsigned long mint = millis() / 1000;
//  StaticJsonDocument<64> doc;
  doc.clear();
  doc["mac_address"] = WiFi.macAddress();
  doc["uptime"] = (int) mint;

  String sendString = "";
  serializeJson(doc, sendString);
  debug("webCheckinWithHome:Web Request:"+sendString);
    
  int err = client.POST(sendString);
  String payload = client.getString();
  client.end();
  
  debug("webCheckinWithHome:Web Response:"+payload);
  time_t latest_schedule_available = updateOurSettings(payload);
//  debug("webCheckinWithHome:latest_schedule_available="+String(lastest_schedule_available));
  // treedump();
  if (!missingpins && isScheduleOutOfDate(latest_schedule_available)) {
    if (webRefreshSchedule()) {
      trace("webCheckinWithHome updating latest_schedule_read="+String(latest_schedule_read)+" to latest_schedule_available="+String(latest_schedule_available));
      latest_schedule_read = latest_schedule_available; 
    // webLogCurrentConfig(1); // INFO log our config to home
    }
  }

  trace("webCheckinWithHome:end");  
}

// ---------------------------------------------------
// ----------------- I2C Section ---------------------
// ---------------------------------------------------
void i2c_setup() {
  trace("i2c_s:start");  
  Wire.begin(SDA_PIN, SCL_PIN);
  trace("i2c_s:end");  
}

void i2c_send(int i2c_port, uint8_t pin, uint8_t duration) {
  trace("i2c_send:start writing to port="+String(i2c_port)+" pin="+String(pin)+" duration="+duration);
//  uint8_t buf[2] = { 0, 0}; // buffer
//  buf[0]=pin;
//  buf[1]=duration;
  Wire.beginTransmission(i2c_port);
  Wire.write(pin);
  Wire.write(duration);
  Wire.endTransmission();
  trace("i2c_send:end");    
}
// ---------------------------------------------------
// ----------------- Schedule section ----------------
// ---------------------------------------------------
void scheduleCheck(){
  time_t tNow = timeClient.getHours()   * 60 * 60
              + timeClient.getMinutes() * 60
              + timeClient.getSeconds();  //Hack
  for (int d=0; d<driver_count; d++) 
    for (int p=0; p<driver[d].pin_count; p++) {
      bool mustBeOn = false;
      bool isOn = driver[d].pins[p].switchedOn;
      int duration = 0; 
      
      for (int s=0; s<driver[d].pins[p].schedule_count;s++) {
        time_t tStart = driver[d].pins[p].schedule[s].start_time; 
        time_t tEnd   = driver[d].pins[p].schedule[s].end_time; 
        if (tNow >= tStart && tNow <= tEnd) {
          mustBeOn = true;
          duration = tEnd - tStart;
        }
      }
      if (isOn && ! mustBeOn)
        scheduleTurnOff(d,p);
      else if (mustBeOn && ! isOn) {
        scheduleTurnOn(d,p,duration);
      }
    }
}

void scheduleTurnOff (int d, int p) {
  driver[d].pins[p].switchedOn = false;
  info("Turning off pin "+String(p)+" on device "+String(d));
}

void scheduleTurnOn (int d, int p, int duration) {
  int i2c = driver[d].i2c_port;
  int pin = driver[d].pins[p].pin_number;
  info("Turning on pin "+String(pin)+" on device "+String(d)+ " for "+String(duration)+" seconds");
  i2c_send (i2c, pin , duration);
  driver[d].pins[p].switchedOn = true;
}
// ---------------------------------------------------
// ----------------- Power monitor section -----------
// ---------------------------------------------------
