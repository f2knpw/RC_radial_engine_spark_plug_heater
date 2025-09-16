//Rx servo reader library : https :  //github.com/rewegit/esp32-rmt-pwm-reader
#include <esp32-rmt-pwm-reader.h>
// #include "esp32-rmt-pwm-reader.h" // if the lib is located directly in the project directory

// init channels and pins
uint8_t pins[] = { 32, 33 };  // desired input pins
int numberOfChannels = sizeof(pins) / sizeof(uint8_t);



//motor
enum { statusIdle,
       statusStarting,
       statusRunning,
       statusStopping };
int status = statusIdle;  //motor supposed stopped after booting the ESP32
int speedIndex = -1, currentSpeedIndex = 0;
int speedRaw = -1;
int speedRawPrev = 1000;
int volIndex = 0, prevVolIndex = 0;
int startDuration = 200;  //duration of start music in ms
long startTime, stopTime;
int firstTime = 0;

int startThr = 40;
int stopThr = 50;
int down = 0;
int up = 1;

//sparks
int spark[] = { 23, 19, 17, 18, 4, 16, 13, 27 };
int sparkDelay = 4000;
long sparkTime;
int currentSpark = 0;

#define LED_PIN 22


//Preferences
#include <Preferences.h>
Preferences preferences;





void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  Serial.begin(115200);

  //sparks drivers
  for (int i = 0; i < 8; i++) {
    pinMode(spark[i], OUTPUT);
    digitalWrite(spark[i], LOW);  //swith off all TPS22996 drivers
  }

  // init Rx channels
  pwm_reader_init(pins, numberOfChannels);
  pwm_set_channel_pulse_neutral(0, 1000);  // throttle neutral is set to "zero"
  pwm_set_channel_pulse_neutral(1, 1000);  //vol neutral is set to "zero"

  // here you can change channel defaults values before reading (if needed)
  // e.g. pwm_set_channel_pulse_min() /max/neutral
  // e.g. set auto_zero/auto_min_max for channel 0-2
  // for (int ch = 0; ch < 3; ch++) {
  //   pwm_set_auto_zero(ch, true);     // set channel to auto zero
  //   pwm_set_auto_min_max(ch, true);  // set channel to auto min/max calibration
  // }

  // begin reading
  esp_err_t err = pwm_reader_begin();
  if (err != ESP_OK) {
    Serial.printf("begin() err: %i", err);
  } else {
    Serial.println("***************");
    Serial.println("program started");
    Serial.println("***************");
    Serial.println(" ");
  }



  //Preferences
  preferences.begin("starMotor", false);

  //preferences.clear();              // Remove all preferences under the opened namespace
  //preferences.remove("counter");   // remove the counter key only
  volIndex = preferences.getInt("volIndex", 30);
  prevVolIndex = volIndex;

  startThr = preferences.getInt("startThr", 40);  //throttle value where motor will start
  stopThr = preferences.getInt("stopThr", 50);    //throttle value where motor will stop

  Serial.println("read preferences :");
  Serial.print("\tstart at ");
  Serial.println(startThr);
  Serial.print("\tstop at ");
  Serial.println(stopThr);

  //preferences.end();  // Close the Preferences


  Serial.println(" ");
  Serial.println("start decoding : \n");


  //delay(3000);
}

void loop() {


  // Reading the actual pulse width of Throttle channel
  speedRaw = constrain((pwm_get_rawPwm(0) - 1000), 0, 1000);  //clip throttle value between 0 and 5

  speedIndex = map(speedRaw, 0, 1000, 0, 5);
  if (speedRaw > 800) speedIndex = 5;
  else if (speedRaw > 600) speedIndex = 4;
  else if (speedRaw > 400) speedIndex = 3;
  else if (speedRaw > 200) speedIndex = 2;
  else if (speedRaw > startThr) speedIndex = 1;
  else speedIndex = 0;

  if (speedRaw + 5 < speedRawPrev) down = 1;
  else down = 0;
  if (speedRaw > speedRawPrev + 3) up = 1;
  else up = 0;
  //Serial.print(down);
  //Serial.print(" ");
  // Serial.println(up);
  //Serial.print("speed\t");
  //Serial.print(speedIndex);



  if (pwm_get_state_name(1) == "STABLE")  // if we receive "volume" channel then compute volIndex and save it if it changes
  {
    volIndex = constrain((pwm_get_rawPwm(2) - 1000), 0, 1000);  //clip volume value between 0 and 100
    volIndex = map(volIndex, 0, 1000, 0, 100);

    if (volIndex != prevVolIndex) {  //change volume

      prevVolIndex = volIndex;
      preferences.putInt("volIndex", volIndex);
    }
  }


  // Do something with the pulse width... throttle and gun
/*
  switch (status) {  //handle motor and gun
    case statusIdle:
      startTime = millis();
      //sparks drivers off
      for (int i = 0; i < 8; i++) {
        digitalWrite(spark[i], LOW);  //swith off all TPS22996 drivers
      }
      if ((speedRaw > startThr) && (up == 1)) {
        status = statusStarting;

        currentSpeedIndex = -1;  //will force exit from startmotor playing

        Serial.println("starting");
        Serial.print("stop at ");
        Serial.println(stopThr);
        //sparks drivers
        sparkTime = millis();
        currentSpark = 0;
      }
      break;
    case statusStarting:
      if (((millis() - startTime) > startDuration)) {  // wait for the startPayed event of the startmotor track (or failsafe time out)
        status = statusRunning;

        Serial.println("running");
      } else {
        if (speedRaw == 0) status = statusStopping;  //shutdown motor immediately if throttle = 0
      }
      break;
    case statusRunning:
      stopTime = millis();
      if ((speedRaw < stopThr) && (down == 1)) {  // change status to stop motor if below low threshold and decreasing throttle
        status = statusStopping;
      }
      if ((speedIndex != currentSpeedIndex)) {
        currentSpeedIndex = speedIndex;
        Serial.print("speed index ");
        Serial.println(speedIndex);
      }
      if (speedIndex < 3) {
        cycleSparks();
      } else {
        //sparks drivers off
        for (int i = 0; i < 8; i++) {
          digitalWrite(spark[i], LOW);  //swith off all TPS22996 drivers
        }
      }

      break;
    case statusStopping:

      Serial.println("stopping");
      status = statusIdle;
      //sparks drivers off
      break;
    default:
      // statements
      break;
  }
*/

  speedRawPrev = speedRaw;
  cycleSparks();
}

void cycleSparks() {
  if ((millis() - sparkTime) > sparkDelay) {
    digitalWrite(spark[currentSpark], LOW);  //swith off all TPS22996 drivers
    sparkTime = millis();
    currentSpark++;
    if (currentSpark > 1) currentSpark = 0;
    digitalWrite(spark[currentSpark], HIGH);  //swith on next TPS22996 drivers
     digitalWrite(spark[currentSpark +1], HIGH);  //swith on next TPS22996 drivers
    Serial.print("switch on spark ");
    Serial.println(currentSpark);
  }
}