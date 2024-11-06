/*

====Sweet Insight====

Sweet Insight is a physical visualisation of blood glucose levels.

There is a top reservoir which holds sugar, that will be released into a glass funnel, to show the blood glucose level. There is a bottom reservoir that is used when the sugar level needs to be reduced.

The mapping of blood sugar is 1 gram of sugar to 1 mg/dL of blood glucose.
The blood glucose values come from a Dexcom G7 Continous Glucose Monitor (CGM), exported as a CSV file, and then loaded as an array of integers "blood_sugar_readings".
The blood glucose data from the CGM is in 5-minute increments.

There is also a demo mode where instead of reading the CGM values from an array, I can simulate the readings by inputting numbers through the serial monitor.

To measure the sugar in the glass funnel, this code uses the HX711 Library to read values from two HX711 Analog-to-Digital Converters conencted to Load Cells (I'm using the variables A and B to identify them).
By default the HX711 library takes 16 samples of data and averages them to get a more accurate value.
For a more responsive scale, I set the number of samples to 4, to provide some smoothing, but also respond in less than a second (since these loadcells are unmodified and send at 10 samples/s).

Author: Mark Altosaar
https://github.com/MarkErik/sweet-insight

Incorporates Libraries:
- Reading from the HX711 ADC connected to a Load Cell: https://github.com/olkal/HX711_ADC
- Adruino Servo control library

Using code from tutorial on reading values from serial monitor, robustly, and without blocking: 
https://forum.arduino.cc/t/serial-input-basics-updated/382007/3  

*/

#include <HX711_ADC.h>
#include <Servo.h>
#include "blood_sugar_data.h"


bool inRange(int reading, int target, int range) {
  return ((reading >= (target - range)) && (reading <= (target + range)));
}

//Loadcell pins:
const int A_HX711_dout = 4;  //HX711 dout pin
const int A_HX711_sck = 5;   //HX711 sck pin
const int B_HX711_dout = 7;  //HX711 dout pin
const int B_HX711_sck = 6;   //HX711 sck pin

//HX711 constructors:
HX711_ADC A_LoadCell(A_HX711_dout, A_HX711_sck);
HX711_ADC B_LoadCell(B_HX711_dout, B_HX711_sck);

//number of samples to average for loadcells
const int numSamples = 4;

//variables to store whether new data is available from HX711 Load Cell ADC
boolean A_newDataReady = 0;
boolean B_newDataReady = 0;

//Servo constructors:
Servo topServo;     // create Servo object for the top, sugar dispensing servo
Servo bottomServo;  // create Servo object for the lower servo, to control how much is removed from funnel

//Values for Servo open and closed positions
const int open = 55;
const int closed = 90;

//Value to hold "blood sugar" weight in funnel, and the new target
int sugar_weight = 0;
int new_target = -1;        //negative to start so that we won't begin emptying the reservoir until first target is set
const int scale_range = 2;  //how much variability we will accept in the scale readings
int sugar_array_counter = 0;
const int arrLen = sizeof(blood_sugar_readings) / sizeof(blood_sugar_readings[0]);
const long time_between_readings = 300000;  //5 minutes between readings
long waiting_time = 0; //counter to see how long between readings to load from array

//Variables to read data from the serial monitor
const byte numChars = 32;
char receivedChars[numChars];
boolean newData = false;

//DEMO
//Set demo_mode to true if you want to input the sugar values via the serial monitor rather than read sugar values from array
const bool demo_mode = true;

//TEST
//Set testing to true to print all values and messages to serial for debugging
const bool testing = true;


void setup() {

  //Initialize the servos and start them closed
  topServo.attach(9);  //pin 9 (PWM)
  topServo.write(closed);
  bottomServo.attach(10);  //pin 10 (PWM)
  bottomServo.write(closed);

  //Start the serial port will be using to feed commands for the demo / print values
  Serial.begin(57600);
  delay(20);

  //Initialize the LoadCells
  A_LoadCell.begin();
  B_LoadCell.begin();

  float A_calibrationValue = 398.68;  // uncomment this if you want to set the calibration value in the sketch
  float B_calibrationValue = 393.50;  // uncomment this if you want to set the calibration value in the sketch

  unsigned long stabilizingtime = 2500;  // preciscion right after power-up can be improved by adding a few seconds of stabilizing time
  boolean _tare = true;                  //set this to false if you don't want tare to be performed in the next step

  A_LoadCell.start(stabilizingtime, _tare);
  A_LoadCell.setSamplesInUse(numSamples);  //set how many samples to average across
  if (A_LoadCell.getTareTimeoutFlag()) {
    if (testing) {
      Serial.println("A: Timeout, check MCU>HX711 wiring and pin designations, and restart");
    }
    while (1)
      ;
  } else {
    A_LoadCell.setCalFactor(A_calibrationValue);  // set calibration value (float)
    if (testing) {
      Serial.println("Loadcell A: Startup is complete");
    }
  }


  B_LoadCell.start(stabilizingtime, _tare);
  B_LoadCell.setSamplesInUse(numSamples);  //set how many samples to average across
  if (B_LoadCell.getTareTimeoutFlag()) {
    if (testing) {
      Serial.println("B: Timeout, check MCU>HX711 wiring and pin designations, and restart");
    }
    while (1)
      ;
  } else {
    B_LoadCell.setCalFactor(B_calibrationValue);
    if (testing) {
      Serial.println("Loadcell B: Startup is complete");
    }
  }

  Serial.println("Ready for blood sugar display.");

}  //end setup()

void loop() {

  if (demo_mode) {
    //check the serial whether new text input has arrived
    recvWithEndMarker();
  }

  if (demo_mode) {
    if (newData) {
      new_target = 0;
      new_target = atoi(receivedChars);
      newData = false;
    }
  } else {
    //see if 5 minutes has passed since reading the last blood sugar value from the array
    if (millis() - waiting_time > time_between_readings) {
      if (sugar_array_counter < arrLen) {
        new_target = blood_sugar_readings[sugar_array_counter];
        sugar_array_counter++;
        waiting_time = 0; //reset the 5-minute counter
      }
      else
      {
        new_target = -1; //out of data, close both servos
      }
    }
  }

  // check for new data on load cells
  if (A_LoadCell.update()) A_newDataReady = true;
  if (B_LoadCell.update()) B_newDataReady = true;

  // read values from the loadcell sensors
  // using && because scales are linked to give final value
  if (A_newDataReady && B_newDataReady) {
    if (testing) {
      Serial.println("Scale data ready.");
    }
    float A_data = A_LoadCell.getData();
    float B_data = B_LoadCell.getData();
    if (A_data < 0) {
      A_data = 0;
    }
    if (B_data < 0) {
      B_data = 0;
    }
    //Add the two values of the scales, take the floor since using whole numbers
    sugar_weight = floor(A_data + B_data);
  }

  if (testing) {
    Serial.print("Weight of sugar in scale A + B: ");
    Serial.println(sugar_weight);
    Serial.print("Target: ");
    Serial.println(new_target);
  }

  if (new_target == -1) {
    //Starting/ending state of -1, both openings in closed position
    topServo.write(closed);
    bottomServo.write(closed);
  } else if (inRange(sugar_weight, new_target, scale_range)) {
    //if the weight in the funnel equals the target, close both openings
    topServo.write(closed);
    bottomServo.write(closed);
  } else if (new_target > sugar_weight) {
    //need more sugar, open up the top
    topServo.write(open);
    bottomServo.write(closed);
  } else if (new_target < sugar_weight) {
    //need to remove sugar, open up the bottom
    topServo.write(closed);
    bottomServo.write(open);
  }

  if (testing) {
    //Increase delay for printing values through the Arduino IDE Serial Monitor
    delay(60);
  } else {
    delay(5);
  }

  //reset the booleans for whether the loadcell sensors are ready with data
  A_newDataReady = 0;
  B_newDataReady = 0;

}  //end main loop()


void recvWithEndMarker() {
  static byte ndx = 0;
  char endMarker = '\n';
  char rc;

  if (Serial.available() > 0) {
    rc = Serial.read();

    if (rc != endMarker) {
      receivedChars[ndx] = rc;
      ndx++;
      if (ndx >= numChars) {
        ndx = numChars - 1;
      }
    } else {
      receivedChars[ndx] = '\0';
      ndx = 0;
      newData = true;
    }
  }
}
