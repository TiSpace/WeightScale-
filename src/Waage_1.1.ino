// Quelle: https://wolles-elektronikkiste.de/hx711-basierte-waage
//  https://github.com/olkal/HX711_ADC

// Kalibrierwert 465.31

#include "setting.h"
#include <EEPROM.h>
#include <HX711_ADC.h>
#include <Wire.h>
#include <Streaming.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define OLED_RESET 7 // we don't have a reset, but the constructor expects it

// construct functions
Adafruit_SSD1306 display(OLED_RESET);
HX711_ADC LoadCell(4, 5);

// varaible declarations
byte interruptPin = 2;
volatile bool taraRequest = false;
const int calVal_eepromAdress = 5;
byte measureMode = 0;
byte pushButtonStatus = 0xFF; //(1 <<ButtonStatusUp)|(1<<ButtonStatusEnter)|(1<<ButtonStatusDown);
float weightReference = 0.0;  // for modus counting

//=======================================================================
//                    Setup
//=======================================================================
void setup()
{
  Serial.begin(38400);
  /* int ii = 0;
  while (1)
  {
    Serial << "Hallo" << ii << endl;
    ii++;
  } */
  versionsInfo();
  Serial.println("Start Setup");
  pinMode(interruptPin, INPUT);

  pinMode(buttonUp, INPUT);
  digitalWrite(buttonUp, HIGH);

  pinMode(buttonEnter, INPUT);
  digitalWrite(buttonEnter, HIGH);

  pinMode(buttonDown, INPUT);
  digitalWrite(buttonDown, HIGH);

  attachInterrupt(digitalPinToInterrupt(interruptPin), taraEvent, RISING);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // initialize with the I2C addr 0x3C (for the 128x64)

  display.clearDisplay();
  display.setTextSize(4);
  display.setTextColor(WHITE);
  display.setCursor(10, 4);
  display.println("Wait");
  display.display();
  LoadCell.begin();
  LoadCell.start(2000);
  float calibrationValue;
  EEPROM.get(calVal_eepromAdress, calibrationValue);

  LoadCell.setCalFactor(calibrationValue);
  Serial.print("Kalibrierwert ");
  Serial.println(calibrationValue);
}
//=======================================================================
//                    Loop
//=======================================================================
void loop()
{
  float weightAsFloat = 0.0;

  unsigned long t = 0;

  static boolean newDataReady = 0;
  const int serialPrintInterval = 0; // increase value to slow down serial print activity
  // check for new data/start next conversion:
  if (LoadCell.update())
    newDataReady = true;
  // get smoothed value from the dataset:
  if (newDataReady)
  {
    if (millis() > t + serialPrintInterval)
    {
      weightAsFloat = LoadCell.getData(); // get the weight
      // we have the standard simple measurement
      if (measureMode == measureStandard)
      {
        displayWeight(weightAsFloat);
      }
      if (measureMode == measureCount)
      {
        Serial << weightAsFloat << "-" << weightReference << endl;
        // displayWeight(weightAsFloat/weightReference);
        displayWeightReference(weightAsFloat, weightReference);
      }
    }
  }

  // check pushbutton-changes
  pushButtonStatus = (pushButtonStatus & 0b11111000) | (digitalRead(buttonUp) << ButtonStatusUp) | (digitalRead(buttonEnter) << ButtonStatusEnter) | (digitalRead(buttonDown) << ButtonStatusDown);

  // Serial << pushButtonStatus << " " << (digitalRead(buttonUp) << ButtonStatusUp) << endl;
  /* Serial.print(pushButtonStatus);
  Serial.print();
  Serial.println((digitalRead(buttonUp) <<ButtonStatusUp));
  */

  if ((highByte(pushButtonStatus) & (1 << ButtonStatusUp)) == (lowByte(pushButtonStatus) & (1 << ButtonStatusUp)))
  {                // Event Up
    measureMode++; // go to next mode
    if (measureMode > 1)
    {
      measureMode = 0;
    }

    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(0, 2);

    switch (measureMode)
    {
    case 0:
      display.println("Standard");

      break;
    case 1:
      display.println("Counting");
      display.setTextSize(1);
      display.setCursor(0, 20);
      display.println("set reference");
      display.display();
      delay(1000); // allow user to release button
      while (digitalRead(buttonUp) == HIGH)
      {
      }
      // user pressed again
      weightReference = LoadCell.getData(); // get the weight
      Serial << "Referenz weight " << weightReference << endl;

      break;
    }
    display.display();

    Serial << "Up" << endl;
    delay(1000);
  }

  if ((highByte(pushButtonStatus) & (1 << ButtonStatusDown)) == (lowByte(pushButtonStatus) & (1 << ButtonStatusDown)))
  { // Event Down
    Serial << "Down" << endl;
    delay(1000);
  }

  if ((highByte(pushButtonStatus) & (1 << ButtonStatusEnter)) == (lowByte(pushButtonStatus) & (1 << ButtonStatusEnter)))
  { // Event Enter
    Serial << "Enter" << endl;
    taraRequest = true;
    // delay(1000);
  }

  // run Tara procedure
  if (taraRequest)
  {
    doTara();
    taraRequest = false;
  }

  // Serial.print("Taste ");
  // Serial.println(digitalRead(buttonUp));

  /* if (digitalRead(buttonUp) == LOW)
   {
     Serial.println("buttonUp");
   }

   if (digitalRead(buttonDown) == LOW)
   {
     Serial.println("buttonDown");
   } */

  if (digitalRead(buttonEnter) == LOW)
  {
    Serial.println("Tara");
    taraRequest = true;
  }

  // shift button status and store
  pushButtonStatus = pushButtonStatus << 4;
}
//=======================================================================
//                    Display weigth
//=======================================================================
void displayWeight(float weight)
{
  String weightAsString = "";
  weightAsString = floatToDisplayString(weight);
  display.clearDisplay();
  display.setCursor(0, 4);

  // display.println(weightAsString);
  display.setTextSize(4);
  display.println(weight, 0);
  display.display();
  // Serial.println(weightAsString);
  //          Serial.println(weight, 3);
}
//=======================================================================
//                    Display weigth
//=======================================================================
void displayWeightReference(float weight, float referenceWeight)
{
  String weightAsString = "";
  weightAsString = floatToDisplayString(weight / referenceWeight);
  display.clearDisplay();
  display.setCursor(0, 0);

  // display.println(weightAsString);
  display.setTextSize(2);
  display.println(weightAsString);
  display.setTextSize(1);
  display.setCursor(10, 24);
  display.print(weight);
  display.print(" / ");
  display.print(referenceWeight);
  display.display();
  // Serial.println(weightAsString);
  //          Serial.println(weight, 3);
}

//=======================================================================
//                    DoTara
//=======================================================================
void doTara()
{
  LoadCell.tareNoDelay();
  display.clearDisplay();
  display.setTextSize(4);
  display.setCursor(10, 4);
  display.println("Wait");
  display.display();
  while (LoadCell.getTareStatus() == false)
  {
    LoadCell.update();
    delay(50);
  }
  Serial.println("tara");
}

//=======================================================================
//                    Tara-Event
//=======================================================================
void taraEvent() { taraRequest = true; }
String floatToDisplayString(float floatValue)
{
  String stringValue = " ";
  int intValue = (int)(round(floatValue));
  if (intValue < 0)
  {
    stringValue = "";
  }
  uint8_t blanks = 3 - int(log10(abs(intValue)));
  for (int i = 0; i < blanks; i++)
  {
    stringValue += " ";
  }
  stringValue += (String)intValue;
  return stringValue;
}

//=======================================================================
//                    Versionsinfo
//=======================================================================
void versionsInfo()
{
  Serial.print("\nArduino is running Sketch: ");
  // Serial.println(myFileName );
  Serial.println(__FILE__);
  Serial.print("Compiled on: ");
  Serial.print(__DATE__);
  Serial.print(" at ");
  Serial.print(__TIME__);
  Serial.print("\n\n");
}

//=======================================================================
//                    measure mode
//=======================================================================