#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <TimeLib.h>

const String ssid = "WLAN150Fibre";
const String password = "ourhome1";

const int WIFI_CONNECTED      = 1;
//const int PREFS_UPDATED       = 2;
const int UNKNOWN_SETTING     = 2;
const int HTTP_CALL_FAIL      = 3;
const int WIFI_CONNECT_FAIL   = 4;

const int PUMP_PIN = 13;

typedef struct {
    int hr;
    int mn;
} t_pumpTime;

int pumpsCount = 0;
t_pumpTime pumpTime[20];

uint8_t LED1pin = D0;
time_t currentTime ;
int pumpRunTime = 30;

bool wifiConnected = false;

void showStatus(int errorCode) {
  for (int t = 0; t< errorCode; t++) {
    digitalWrite (LED1pin, LOW);
    delay(500);
    digitalWrite (LED1pin, HIGH);
    delay(500);
  }
}

void connectWiFi() {
  Serial.print("Connecting");
  WiFi.begin (ssid, password);
  int counter = 0;
  while (WiFi.status() != WL_CONNECTED && counter < 10)
  {
      delay(500);
      Serial.print(".");  
      counter++;
  }
  Serial.println();
  checkWiFiStatus();
  if (wifiConnected) {
    Serial.print("Connected, IP address: ");
    Serial.println (WiFi.localIP());
    showStatus(WIFI_CONNECTED);
  } else {
    Serial.println("Unable to connect.");
    showStatus(WIFI_CONNECT_FAIL);
  }
}

void checkWiFiStatus() {
  wifiConnected = (WiFi.status() == WL_CONNECTED);
}

void setTime(String timestring) {
  int hr = timestring.substring(0,2).toInt();
  int mn = timestring.substring(3,5).toInt();
  int sc = timestring.substring(6,8).toInt();
  setTime(hr,mn,sc,0,0,0);
//  Serial.print ("Time set to ");
//  Serial.print (hour());
//  Serial.print (":");
//  Serial.print (minute());
//  Serial.print (":");
//  Serial.println (second());
      //Serial.println (now());
}

void setPTime(int count, String timestring) {
  int hr = timestring.substring(0,2).toInt();
  int mn = timestring.substring(3,5).toInt();
  pumpTime[count].hr = hr;
  pumpTime[count].mn = mn;
//  Serial.print("Setting schedule ");
//  Serial.print(count);
//  Serial.print(" to ");
//  Serial.print(hr);
//  Serial.print(":");
//  Serial.println(mn);
}

void useSetting(String set, String val) {
  if (set.equals("currentTime")) {
    setTime(val);
  } else if (set.equals("schedule")) {
      int count = 0;
      int currentPos = 0;
      int tStart = 0;
      currentPos = val.indexOf(";");
      while (currentPos!=-1) {
        String mid = val.substring(tStart, currentPos);
        setPTime(count,mid);
        count ++;
        tStart = currentPos+1;
        currentPos = val.indexOf(";", tStart);
      }
      String mid = val.substring(tStart,val.length());
      setPTime(count, mid);
      pumpsCount = count+1; // 0 based;
//    Serial.print("Schedules found : ");
//    Serial.println(pumpsCount);
  } else if (set.equals("pumpSeconds")) {
    pumpRunTime = val.toInt();
  } else {
    showStatus(UNKNOWN_SETTING);
    Serial.print("Unknown setting '");
    Serial.print(set);
    Serial.println ("'");
  }
    
}

void processSetting(String line) {
  //Serial.print ("Found a setting : ");
  //Serial.println (setting);
  int mid = line.indexOf("=");
  if (mid!=-1) {
    String setting = line.substring(0,mid);
    String value = line.substring(mid+1,line.length());
    useSetting(setting, value);  
  }
}

void processPayload (String payload) {
  int currentPos = 0;
  int wordStart = 0;
  currentPos = payload.indexOf(",");
  while (currentPos!=-1) {
    String mid = payload.substring(wordStart, currentPos);
    processSetting(mid);
    wordStart = currentPos+1;
    currentPos = payload.indexOf(",", currentPos + 1);
  }
  String mid = payload.substring(wordStart,payload.length());
  processSetting(mid);
}

void getSettings() {
  HTTPClient http;

  http.begin("http://172.16.1.162/~chrisv/getTankSettings.pl");
  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    Serial.println (payload);
    processPayload(payload);
  } else {
    Serial.print ("HTTP Failure=");
    Serial.println (httpCode);
    showStatus(HTTP_CALL_FAIL);
  }
  http.end();

}


bool checkPumpNow() {
  for (int t=0; t<pumpsCount; t++) {
    if (hour()==pumpTime[t].hr && minute()==pumpTime[t].mn) {
      return true;
    }
  }
  return false;
}

void startPump() {
    
    digitalWrite (LED1pin, LOW);
    digitalWrite (PUMP_PIN, LOW);
    delay(pumpRunTime*1000);
    digitalWrite (LED1pin, HIGH);
    digitalWrite (PUMP_PIN, HIGH);
}

void setup() {
  Serial.begin(115200);
  Serial.println ();
  pinMode (LED1pin, OUTPUT);
  pinMode (PUMP_PIN, OUTPUT);
  digitalWrite (PUMP_PIN, HIGH);
}


void loop() {
  checkWiFiStatus();
  if (!wifiConnected) connectWiFi();
  
  getSettings();
  if (checkPumpNow()) {
    startPump();
  }
//  if (wifiConnected) { 
    delay(60*1000); 
//  } else {
//    delay(10*000); // Wifi disconnected while in pump minute could lead to double watering
//  }
}
