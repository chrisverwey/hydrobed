/** Arduino Nano code
  The driver is used as a slave device to interact with 5V components.
  It gets instructions via I2C from a controller board (which is networking enabled) 
  
  The intention is to make this board completely ignorant of what is attached to it so that the code does not need 
  to be modified every time a device is added or removed.
  
  Commands can be:
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
// Moisture sensor range test : 30 .. 650 (504 max under real world test)
#include <Wire.h>
#include <TimeLib.h>

#define SERIALDEBUG true
#define DEBUGLEVEL 2  
// 0=INFO 1=DEBUG 2=TRACE

void info(String s) { if (SERIALDEBUG && DEBUGLEVEL>=0) Serial.println(s); }
void debug(String s) { if (SERIALDEBUG && DEBUGLEVEL>=1) Serial.println(s); }
void trace(String s) { if (SERIALDEBUG && DEBUGLEVEL>=2) Serial.println(s); }

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);  
  pinMode(LED_BUILTIN, HIGH);       // Turn off LED to save power
  if (SERIALDEBUG) 
    Serial.begin(115200);           // Enable serial monitor
  
  dEnablePins();
  aEnablePins();  

  Wire.begin(1);                    // Broadcast on I2C channel & 
  Wire.onRequest(receiveData);      // set callback handlers
  Wire.onReceive(receiveRequest);
}

// ---------------------------------------------------
// ----------------- Analog sensor section -----------
// ---------------------------------------------------

#define READ_DELAY 60
#define ANALOG_READINGS 20
#define SENSORdPIN PD4

uint8_t sensorpins[] = {A0, A1, A2, A3, A6, A7};
uint16_t pinvalue[]   = { 0,  0,  0,  0,  0,  0};
uint8_t i2c_databuffer[sizeof(sensorpins)*3];
uint16_t readTimeout = READ_DELAY; 
/* 
 *  Called from setup() to enable the 
 *  pins needed for analog sensors.
 */
void aEnablePins() {
  trace("aEnablePins:start");
  pinMode(SENSORdPIN, OUTPUT);       // Turn on the sensor enable pin
  for (int i=0; i<sizeof(sensorpins); i++) {
    pinMode(sensorpins[i], INPUT);
  }
  trace("aEnablePins:end");
}

/*
 * Read each sensor value ANALOG_READINGS times and
 * return the  average to avoid getting a 
 * fluctuating reading. 
 * Since some sensors are float values, and we only 
 * transfer int over i2c, we * 100 to get 2 decimal
 * resolution. On controller side we / 100 to get 
 * back to floats.
 */
float getAnalogSensorValue(int PinNo) {
  trace("getAnalogSensorValue:start:"+String(PinNo));
  float sensorValue = 0;
  
  for (int i = 0; i <= ANALOG_READINGS; i++) { 
    sensorValue = sensorValue + analogRead(PinNo); 
    delay(1); 
  } 
  sensorValue = sensorValue/ANALOG_READINGS;
  sensorValue = sensorValue * 100;
  
  trace("getAnalogSensorValue:end:"+String(sensorValue));
  return sensorValue; 
}

/*  Keeping resistive soil moisture sensors powered
 *  constantly speeds up electrolysis on the metal 
 *  plates. Only power the sensor (on SEONSORdPIN) 
 *  when you are ready to read then turn it off 
 *  again after.
 */
void aReadAll() {
  trace("aReadAll:start");
  digitalWrite(SENSORdPIN, HIGH);
  delay(10);
  
  String debugString = "aReadAll";
  for (int i=0; i<sizeof(sensorpins); i++) {
    pinvalue[i] = getAnalogSensorValue(sensorpins[i]); // float to int16
    debugString+=" pin"+String(sensorpins[i])+":"+String(pinvalue[i]);
  }
  debug(debugString);
  digitalWrite(SENSORdPIN, LOW);

  // Prepare the data buffer for an I2C request.
  for (int i=0; i<sizeof(sensorpins); i++) {
    i2c_databuffer[i*3]=sensorpins[i];               // pin no
    i2c_databuffer[i*3+1]=(pinvalue[i] >> 8) & 0xFF; // left sensor byte
    i2c_databuffer[i*3+2]= pinvalue[i]       & 0xFF; // right sensor byte
  } 
  trace("aReadAll:end");
}

/*
 * Check the timer to see if its time to do the 
 * net sensor reading.
 */
bool aTimeForReading() {
  aDecreaseTimer();
  
  if (readTimeout<1) {
    readTimeout = READ_DELAY;
    return true;
  } else {
    return false;
  }
};

/*
 * Count down the timer to when we do the next 
 * sensor reading.
 */
void aDecreaseTimer() {
    readTimeout -= 1;
};


/*
 * Sends the contents of the sensor readings over
 * the I2C wire back to the controller on request.
 * The preparation of the buffer was moved out of 
 * this method to reduce the latency on the I2C 
 * response.
 */
void i2c_sendSensorValues() {
  trace("i2c_sendSensorValues:start");
  Wire.write(i2c_databuffer, sizeof(i2c_databuffer));
  trace("i2c_sendSensorValues:end");
}

// ---------------------------------------------------
// ----------------- Digtal section --------------------
// ---------------------------------------------------
uint8_t dPins[]            = {PD5, PD6, PD7} ; // { PD2, PD3, PD5, PD6, PD7, PB0, PB1, PB2, PB3, PB4};
uint8_t dPinDurationLeft[] = {   0,   0,   0}; //,   0,   0,   0,   0,   0,   0,   0};
bool dAreAnyPinsOn         = false;

/*
 * Called from setup() to enable the digital pins.
 */
void dEnablePins() {  
  trace("dEnablePins:start");
  for (int i=0; i<sizeof(dPins); i++) {
        pinMode(dPins[i], OUTPUT);  // Turn on digital pins
        digitalWrite(dPins[i], HIGH);
  }  
  trace("dEnablePins:end");
}

void dTurnPinOn(int pin, int duration) {
  trace("dTurnPinOn:start:"+String(pin)+":"+String(duration));
  for (int i=0; i<sizeof(dPins); i++) {
    if (dPins[i]==pin) {
      digitalWrite(pin, LOW);
      info("dTurnPinOn:Turning on pin_"+String(pin)+",duration_"+String(duration));
      dPinDurationLeft[i]=duration;
      trace("duration now="+String(dPinDurationLeft[i]));
      dAreAnyPinsOn = true;
    }
  }
  trace("dTurnPinOn:end");
}

void dTurnPinOff(int pin) {
  trace("dTurnPinOn:start:"+String(pin));
  info("dTurnPinOff:Turning off pin_"+String(pin));
  digitalWrite(pin, HIGH);
  trace("dTurnPinOn:end");
}

/*
 * Run through all the pins and decrease their time left 
 * to run. If any get to 0 then turn them off.
 */
void dDecreaseTimers() {
  trace("dDecreaseTimers:start");
  bool areAnyOn = false;
  
  for (int i=0; i<sizeof(dPins); i++) {
    trace("dDecreaseTimers:Checking if pin "+String(dPins[i])+" is on : "+dPinDurationLeft[i]);
    if (dPinDurationLeft[i] > 0) {
      dPinDurationLeft[i]--;
      debug("dPinDurationLeft_"+String(i)+":"+String(dPinDurationLeft[i]));

      if (dPinDurationLeft[i] < 1) 
        dTurnPinOff(dPins[i]);
      else
        areAnyOn = true;
    }
  }
  dAreAnyPinsOn = areAnyOn;
}

// ---------------------------------------------------
// ----------------- Main loop -----------------------
// ---------------------------------------------------
uint8_t i2c_dpinno   = 0;
uint8_t i2c_dpintime = 0;
bool    i2c_command  = false;

uint8_t previousSecond = 0;
void loop() {
//  trace("loop:start");
  int s = second();
  if (previousSecond!=s) {
    if (dAreAnyPinsOn) {
       dDecreaseTimers();
    }

    if (aTimeForReading()) {
      aReadAll();
    }
        
    previousSecond=s;
  }
   
  // Check if we got an I2C command to handle
  if (i2c_command) {
    info("loop:I2C received \
           pinno="+String(i2c_dpinno)+
         " duration="+String(i2c_dpintime));
    dTurnPinOn(i2c_dpinno, i2c_dpintime);
    i2c_command=false;
  }

  delay(100);
//  trace("loop:end");
}

// ---------------------------------------------------
// ----------------- I2C section ---------------------
// ---------------------------------------------------
void receiveData (int16_t byteCount) {
  trace("receiveData:start");
  i2c_sendSensorValues();
  trace("receiveData:end");
}

void receiveRequest() {
  trace("receiveRequest:start");
  if (!i2c_command) { // don't read if there already one in the queue
    noInterrupts();
    i2c_dpinno   = Wire.read();
    i2c_dpintime = Wire.read();
    i2c_command  = true;
    trace("receiveRequest: got new request pin="+String(i2c_dpinno)+" duration="+String(i2c_dpintime));
    interrupts();
  }
  trace("receiveRequest:end");
}
