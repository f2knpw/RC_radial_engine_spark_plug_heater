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
int THR = 0;
long startTime, stopTime;
int firstTime = 0;

int startThr = 40;
int stopThr = 50;


//sparks
int spark[] = { 19, 23, 18, 17, 16, 4, 13 };
int channels[] = { 0, 1, 2, 3, 4, 5, 6 };

#define LED_PIN 22

//PWM
long freq = 1000;
int resolution = 11;
int dutyCycle;
int maxPower = 1850;  //or 1s at 3.9V  1550 for 1s at 3.6V

#define MIN_POWER 2047
int power = MIN_POWER;

//Preferences
#include <Preferences.h>
Preferences preferences;

//battery voltage
#define PIN_BAT 36
float Vbat;


void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  Serial.begin(115200);

  //battery voltage
  //analogSetClockDiv(255);
  //analogReadResolution(12);           // Sets the sample bits and read resolution, default is 12-bit (0 - 4095), range is 9 - 12 bits
  analogSetWidth(12);              // Sets the sample bits and read resolution, default is 12-bit (0 - 4095), range is 9 - 12 bits
  analogSetAttenuation(ADC_11db);  // Sets the input attenuation for ALL ADC inputs, default is ADC_11db, range is ADC_0db, ADC_2_5db, ADC_6db, ADC_11db


  //sparks drivers
  for (int i = 0; i < 8; i++) {


    ledcSetup(channels[i], freq, resolution);
    ledcAttachPin(spark[i], channels[i]);
    dutyCycle = MIN_POWER;  //0% duty cycle
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
  maxPower = preferences.getInt("maxPower", 1880);
  

  Serial.println("read preferences :");
  Serial.print("\tmaxPower ");
  Serial.println(maxPower);

  //preferences.end();  // Close the Preferences


  Serial.println(" ");
  Serial.println("start decoding : \n");


  //delay(3000);
}

void loop() {
  //battery voltage
  Vbat = 0;
  for (int i = 0; i < 100; i++) {
    Vbat += analogRead(PIN_BAT);
  }
  Vbat = Vbat / 100;
  Vbat = Vbat * 4.08 / 1860;  //conversion to Volts

  // Reading the actual pulse width of Throttle channel
  speedRaw = constrain((pwm_get_rawPwm(0) - 1000), 0, 1000);  //clip throttle value between 0 and 5

  if (pwm_get_state_name(1) == "STABLE")  // if we receive "volume" channel then compute volIndex and save it if it changes
  {
    volIndex = constrain((pwm_get_rawPwm(2) - 1000), 0, 1000);  //clip volume value between 0 and 100
    volIndex = map(volIndex, 0, 1000, 0, 100);

    if (volIndex != prevVolIndex) {  //change volume

      prevVolIndex = volIndex;
      preferences.putInt("volIndex", volIndex);
    }
  }


  // Do something with the pulse width...
  cycleSparks();

  speedRawPrev = speedRaw;

  if (Serial.available())  // if there is data comming from the ESP32 USB serial port
  {
    String command = Serial.readStringUntil('\n');  // read string until meet newline character

    if ((command.substring(0, 4) == "MAX=") || (command.substring(0, 4) == "max=") || (command.substring(0, 4) == "Max="))  // MAX=xx to change the volume
    {
      maxPower = command.substring(4).toInt();
      maxPower = constrain(maxPower, 1500, 2047);
      preferences.putInt("maxPower", maxPower);
      Serial.print("maxPower set to  ");
      Serial.println(maxPower);
    }
  }
}

void cycleSparks() {
 
  if (Vbat > 3.5) THR = speedRaw; //41; XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
  else THR = 0;  //will stop sparks if battery is low

  power = maxPower + 200*Vbat -800 ; //+ 510 * Vbat - 2040; //500 * Vbat - 2080;  //linear fit from 1780 to 1680 when Vbat goes from 4.0V to 3.5V
  if (power < 1500) power = 1500;
  /// map(value, fromLow, fromHigh, toLow, toHigh)
 
  dutyCycle = map(THR, startThr, 1000, power, MIN_POWER);
  if (THR < startThr) dutyCycle = 2047;  //will stop sparks is throttle is low

  for (int i = 0; i < 7; i++) {  //else output PWM on each spark
    ledcWrite(channels[i], dutyCycle);
  }
//  if (THR > 0) {
    Serial.print("switch on spark (throttle / dutyCycle / maxPower ) ");
    Serial.print(THR);
    Serial.print(" / ");
    Serial.print(dutyCycle);
    Serial.print(" / ");
    Serial.print(maxPower);
    Serial.print(" V bat = ");
    Serial.println(Vbat);
 // }
}