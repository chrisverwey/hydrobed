/* 
 *  This module used because of an Arduino bug with passing
 *  structures as parameters. 
 *  
 *  controller_1.0:556:29: error: variable or field 'downloadDriverSchedule' declared void
 void downloadDriverSchedule(t_Driver *driver) {
 *                             ^
 * controller_1.0:556:29: error: 't_Driver' was not declared in this scope
 * 
 * https://forum.arduino.cc/index.php?topic=44890.0
 */
#define MAXDRIVERS 4
#define MAXPINS 10
#define MAXSCHEDULES 20
// ---------------------------------------------------
// ----------------- Main data structs ---------------
// ---------------------------------------------------
typedef struct {
  int activation_id;
  time_t start_time;
  time_t end_time;
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
  boolean switchedOn=false;
  int schedule_count=-1;
  t_Activation schedule[MAXSCHEDULES];
} t_Pin;

typedef struct {
  int driver_id;
  int driver_type;
  int i2c_port;
  long schedule_read_freq = 60l;
  long schedule_read_countdown = 0l;
  int pin_count = -1;
  t_Pin pins[MAXPINS] ;
} t_Driver;
