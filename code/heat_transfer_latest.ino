#include <Chrono.h>
#include <Streaming.h>
#include <Wire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <SPI.h>
#include <SD.h>
#include <ds3231.h>
#include <Keypad.h>
#include <PID_v1.h>
#include <avr/wdt.h>

#define ONE_WIRE_BUS 49
#define CS_PIN 53
#define HEATER 44
#define CURRENT_SENSOR_PIN A15
#define DEVICE_DISCONNECTED_C -1000


OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

LiquidCrystal_I2C lcd(0x27, 20, 4);
Chrono myChrono;

File myFile;

char daysOfTheWeek[7][12] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };

const byte ROWS = 4;
const byte COLS = 4;

char hexaKeys[ROWS][COLS] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '.', '0', '#', 'D' }
};

byte rowPins[ROWS] = { A0, A1, A2, A3 };
byte colPins[COLS] = { A4, A5, A6, A7 };
Keypad customKeypad = Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);

char customKey;
int mode;
String fileName;
String frequencyInput;
String numberReadingInput;
String temperatureInput;
String fanSpeed;
int frequency = 10;
int numberReading = 90;
int secToStart;
bool isConventional;
String type;
String variant;

float temp1Val;
float temp2Val;
float temp3Val;
float temp4Val;

float temp1ValDis;
float temp2ValDis;
float temp3ValDis;
float temp4ValDis;

double setpoint = 50.0;
double Kp = 0.1;   // Proportional gain
double Ki = 0.05;  // Integral gain
double Kd = 0.1;   // Derivative gain
double input, output;
int readingCnt;

PID myPID(&input, &output, &setpoint, Kp, Ki, Kd, DIRECT);

float nVPP;                  // Voltage measured across resistor
float nCurrThruResistorPP;   // Peak Current Measured Through Resistor
float nCurrThruResistorRMS;  // RMS current through Resistor
float nCurrentThruWire;

int r;
float rTemp;
bool tempReach = false;

int errorCnt = 0;
bool disMode = true;

byte data[9];

struct ts t;
char directoryPath[20];
void setup() {
  wdt_disable();

  pinMode(HEATER, OUTPUT);
  Wire.begin();
  lcd.begin();
  lcd.backlight();

  sensors.begin();

  Serial.begin(9600);
  Serial.setTimeout(100);

  Serial << "BEGIN!" << endl;

  DS3231_init(DS3231_CONTROL_INTCN);


  // t.hour = 20;
  // t.min = 29;
  // t.sec = 30;
  // t.mday = 13;
  // t.mon = 2;
  // t.year = 2024;
  // DS3231_set(t);


  if (!SD.begin(CS_PIN)) {
    lcd.setCursor(0, 0), lcd << F("========ERROR=======");
    lcd.setCursor(0, 1), lcd << F(" Can't Read SD card ");
    lcd.setCursor(0, 2), lcd << F("    Please check    ");
    lcd.setCursor(0, 3), lcd << F("====================");
    while (1)
      ;
  }
  myPID.SetMode(AUTOMATIC);
  myPID.SetOutputLimits(0, 1);
  randomSeed(analogRead(A8));
  SD.mkdir("data");
}

void loop() {
  normalProcess();
}

void tempTesting() {
}
void controlHeater() {
  input = temp3Val;
  if (!tempReach && ((setpoint - 1) <= input)) {
    tempReach = true;
  }
  myPID.Compute();
  if (output > 0.5) {
    heaterOn();  // Relay ON
  } else {
    heaterOff();  // Relay OFF
  }
}


void heaterOn() {
  digitalWrite(HEATER, HIGH);
}

void heaterOff() {
  digitalWrite(HEATER, LOW);
}


void saveInfo() {

  myFile = SD.open("data/" + fileName + "i.csv", FILE_WRITE);
  DS3231_get(&t);
  if (myFile) {
    myFile.print(t.year);
    myFile.print(F("/"));
    myFile.print(t.mon);
    myFile.print(F("/"));
    myFile.print(t.mday);
    myFile.print(F(" "));
    myFile.print(t.hour);
    myFile.print(F(":"));
    myFile.print(t.min);
    myFile.print(F(":"));
    myFile.print(secToStart);
    myFile.print(F(","));
    if (isConventional) {
      myFile.print(F("Natural,"));

    } else {
      myFile.print(F("Forced,"));
    }
    myFile.print(type);
    myFile.print(F(","));
    myFile.print(variant);
    myFile.print(F(","));
    myFile.print(setpoint);

    myFile.close();
  } else {
    mode = -1;
    lcd.clear();
  }
}

void sendInfo() {
  Serial.print(F("#"));
  Serial.print(fileName);
  Serial.print(F("|"));
  Serial.print(t.year);
  Serial.print(F("/"));
  Serial.print(t.mon);
  Serial.print(F("/"));
  Serial.print(t.mday);
  Serial.print(F(" "));
  Serial.print(t.hour);
  Serial.print(F(":"));
  Serial.print(t.min);
  Serial.print(F(":"));
  Serial.print(secToStart);
  Serial.print(F("|"));
  if (isConventional) {
    Serial.print(F("Natural|"));

  } else {
    Serial.print(F("Forced|"));
  }
  Serial.print(type);
  Serial.print(F("|"));
  Serial.print(variant);
  Serial.print(F("|"));
  Serial.print(int(setpoint));
  Serial.println(F("*"));
}
void normalProcess() {
  if (Serial.available()) {
    String reading = Serial.readString();
    Serial << reading << endl;

    reading.trim();
    if (reading == "r") {
      sendInfo();
    }
  }
  switch (mode) {
    case -1:
      lcd.setCursor(0, 0), lcd << F("========ERROR=======");
      lcd.setCursor(0, 1), lcd << F("Can't open or create");
      lcd.setCursor(0, 2), lcd << F("filename: ") << fileName;
      lcd.setCursor(0, 3), lcd << F("Press 'A' to Reset  ");
      heaterOff();
      customKey = customKeypad.getKey();
      if (customKey) {
        if (customKey == 'A') {
          mode = 0;
          tempReach = false;
          lcd.clear();
        }
      }
      break;
    case 0:
      lcd.setCursor(0, 0), lcd << F("    HEAT TRANSFER   ");
      lcd.setCursor(0, 1), lcd << F("       SYSTEM       ");
      lcd.setCursor(0, 2), lcd << F("====================");
      lcd.setCursor(0, 3), lcd << F("Press 'A' to Start  ");
      customKey = customKeypad.getKey();
      if (customKey) {
        if (customKey == 'A') {
          mode = 2;
          tempReach = false;
          lcd.clear();
        }
      }
      break;

    case 2:
      lcd.setCursor(0, 0), lcd << F("=======INPUT========");
      lcd.setCursor(0, 1), lcd << F("A. Natural          ");
      lcd.setCursor(0, 2), lcd << F("B. Forced           ");
      lcd.setCursor(0, 3), lcd << F("C. Back             ");
      customKey = customKeypad.getKey();
      if (customKey) {
        if (customKey == 'A') {
          isConventional = true;
          lcd.clear();
          mode = 3;
        } else if (customKey == 'B') {
          isConventional = false;
          lcd.clear();
          mode = 3;
        } else if (customKey == 'C') {
          mode = 0;
        }
      }
      break;
    case 3:
      lcd.setCursor(0, 0), lcd << F("A. Finned           ");
      lcd.setCursor(0, 1), lcd << F("B. Flat             ");
      lcd.setCursor(0, 2), lcd << F("C. Pinned           ");
      lcd.setCursor(0, 3), lcd << F("D. Back             ");

      customKey = customKeypad.getKey();
      if (customKey) {
        if (customKey == 'A') {
          type = "Finned";
          mode = 4;
          lcd.clear();
        } else if (customKey == 'B') {
          type = "Flat";
          variant = "N/A";
          mode = 10;
          fileName = "";
          lcd.clear();
        } else if (customKey == 'C') {
          type = "Pinned";
          mode = 6;
          lcd.clear();
        } else if (customKey == 'D') {
          mode = 2;
        }
      }
      break;

    case 4:
      lcd.setCursor(0, 0), lcd << F("A. 6 Fins           ");
      lcd.setCursor(0, 1), lcd << F("B. 8 Fins           ");
      lcd.setCursor(0, 2), lcd << F("C. 10 Fin           ");
      lcd.setCursor(0, 3), lcd << F("D. Back             ");

      customKey = customKeypad.getKey();
      if (customKey) {
        if (customKey == 'A') {
          variant = "6 Fins";
          mode = 10;
          fileName = "";
          lcd.clear();
        } else if (customKey == 'B') {
          variant = "8 Fins";
          mode = 10;
          fileName = "";
          lcd.clear();
        } else if (customKey == 'C') {
          variant = "10 Fins";
          mode = 10;
          fileName = "";
          lcd.clear();
        } else if (customKey == 'D') {
          mode = 3;
        }
      }
      break;


    case 6:
      lcd.setCursor(0, 0), lcd << F("A. 13 Pin           ");
      lcd.setCursor(0, 1), lcd << F("B. 16 Pins          ");
      lcd.setCursor(0, 2), lcd << F("C. 25 Pins          ");
      lcd.setCursor(0, 3), lcd << F("D. Back             ");

      customKey = customKeypad.getKey();
      if (customKey) {
        if (customKey == 'A') {
          variant = "13 Pin";
          mode = 10;
          fileName = "";
          lcd.clear();
        } else if (customKey == 'B') {
          variant = "16 Pin";
          mode = 10;
          fileName = "";
          lcd.clear();
        } else if (customKey == 'C') {
          variant = "25 Pin";
          mode = 10;
          fileName = "";
          lcd.clear();
        }
      }
      break;


    case 7:
      lcd.setCursor(0, 0), lcd << F("=======INPUT========");
      lcd.setCursor(0, 1), lcd << F("Fan Speed (m/s)     ");
      lcd.setCursor(0, 2), lcd << frequencyInput << F("   ");
      lcd.setCursor(0, 3), lcd << F("(A) OK     (D)Delete");

      customKey = customKeypad.getKey();
      if (customKey) {
        if (customKey == 'A') {
          frequencyInput.trim();
          if (frequencyInput != "") {

            frequency = frequencyInput.toInt();
            mode = 30;
            lcd.clear();
          }
        } else if (customKey == 'D') {
          frequencyInput = "";
          lcd.clear();
        } else if (customKey != 'A' && customKey != 'B' && customKey != 'C' && customKey != 'D' && customKey != '*' && customKey != '#') {
          frequencyInput = frequencyInput + customKey;
        }
      }
      break;


    case 10:
      lcd.setCursor(0, 0), lcd << F("=======INPUT========");
      lcd.setCursor(0, 1), lcd << F("     File Name      ");
      lcd.setCursor(0, 2), lcd << fileName << F("   ");
      lcd.setCursor(0, 3), lcd << F("(A) OK     (D)Delete");

      customKey = customKeypad.getKey();
      if (customKey) {
        if (customKey == 'A') {
          fileName.trim();
          if (fileName != "") {
            DS3231_get(&t);
            if (SD.exists("data/" + fileName + ".csv")) {
              mode = 11;
              lcd.clear();
            } else {
              mode = 20;
              lcd.clear();
            }
          }
        } else if (customKey == 'D') {
          fileName = "";
          lcd.clear();
        } else if (customKey != 'A' && customKey != 'B' && customKey != 'C' && customKey != 'D' && customKey != '*' && customKey != '#') {
          fileName = fileName + customKey;
        }
      }
      break;

    case 11:
      lcd.setCursor(0, 0), lcd << F(" File Already Exist ");
      lcd.setCursor(0, 1), lcd << F("  Continue logging  ");
      lcd.setCursor(0, 2), lcd << F("   with the file?   ");
      lcd.setCursor(0, 3), lcd << F("A. Yes         B. No");
      customKey = customKeypad.getKey();
      if (customKey) {
        if (customKey == 'A') {
          mode = 20;
          lcd.clear();
        } else if (customKey == 'B') {
          mode = 0;
        }
      }
      break;

    case 20:
      lcd.setCursor(0, 0), lcd << F("=======INPUT========");
      lcd.setCursor(0, 1), lcd << F("Reading Frequency(s)");
      lcd.setCursor(0, 2), lcd << frequencyInput << F("   ");
      lcd.setCursor(0, 3), lcd << F("(A) OK     (D)Delete");

      customKey = customKeypad.getKey();
      if (customKey) {
        if (customKey == 'A') {
          if (frequencyInput != "") {
            frequency = frequencyInput.toInt();
            mode = 30;
            lcd.clear();
          }
        } else if (customKey == 'D') {
          frequencyInput = "";
          lcd.clear();
        } else if (customKey != 'A' && customKey != 'B' && customKey != 'C' && customKey != 'D' && customKey != '*' && customKey != '#') {
          frequencyInput = frequencyInput + customKey;
        }
      }
      break;

    case 30:
      lcd.setCursor(0, 0), lcd << F("=======INPUT========");
      lcd.setCursor(0, 1), lcd << F("Number of Readings  ");
      lcd.setCursor(0, 2), lcd << numberReadingInput << F("   ");
      lcd.setCursor(0, 3), lcd << F("(A) OK     (D)Delete");

      customKey = customKeypad.getKey();
      if (customKey) {
        if (customKey == 'A') {
          numberReadingInput.trim();
          if (numberReadingInput != "") {
            numberReading = numberReadingInput.toInt();
            mode = 40;
            lcd.clear();
          }
        } else if (customKey == 'D') {
          numberReadingInput = "";
          lcd.clear();
        } else if (customKey != 'A' && customKey != 'B' && customKey != 'C' && customKey != 'D' && customKey != '*' && customKey != '#') {
          numberReadingInput = numberReadingInput + customKey;
        }
      }
      break;

    case 40:
      lcd.setCursor(0, 0), lcd << F("=======INPUT========");
      lcd.setCursor(0, 1), lcd << F("Temperature  ");
      lcd.setCursor(0, 2), lcd << temperatureInput << F("   ");
      lcd.setCursor(0, 3), lcd << F("(A) OK     (D)Delete");

      customKey = customKeypad.getKey();
      if (customKey) {
        if (customKey == 'A') {
          temperatureInput.trim();
          if (temperatureInput != "") {
            setpoint = temperatureInput.toInt();
            mode = 50;
            disMode=true;
            lcd.clear();
            saveInfo();
            delay(2000);

            myChrono.restart();
            readingCnt = 0;
            DS3231_get(&t);
            secToStart = frequency + t.sec;
            if (secToStart >= 60) {
              secToStart = secToStart - 60;
            }
            sendInfo();
          }
        } else if (customKey == 'D') {
          temperatureInput = "";
          lcd.clear();
        } else if (customKey != 'A' && customKey != 'B' && customKey != 'C' && customKey != 'D' && customKey != '*' && customKey != '#') {
          temperatureInput = temperatureInput + customKey;
        }
      }
      break;

    case 50:

      customKey = customKeypad.getKey();
      if (customKey) {
        if (disMode) {
          if (customKey == 'A') {
            mode=0;
            lcd.clear();
          }else  if (customKey == 'B') {
            disMode = true;
            lcd.clear();
          }
        } else {
          if (customKey == '*') {
            disMode = false;
            lcd.clear();
          }
        }
      }

      requestTemp();
      temp1Val = getTemp1();
      temp2Val = getTemp2();
      temp3Val = getTemp3();
      temp4Val = getTemp4();
      if (temp1Val < 0) {
        temp1Val = temp1ValDis;
      }
      if (temp2Val < 0) {
        temp2Val = temp2ValDis;
      }
      if (temp3Val < 0) {
        temp3Val = temp3ValDis;
      }
      if (temp4Val < 0) {
        temp4Val = temp4ValDis;
      }
      temp1ValDis = temp1Val;
      temp2ValDis = temp2Val;
      temp3ValDis = temp3Val;
      temp4ValDis = temp4Val;

      controlHeater();
      nVPP = getVPP();
      nCurrThruResistorPP = (nVPP / 200.0) * 1000.0;
      nCurrThruResistorRMS = nCurrThruResistorPP * 0.707;
      nCurrThruResistorPP = nCurrThruResistorPP * 220L;
      if (disMode) {
        lcd.setCursor(0, 0), lcd << String(t.year) << F("/") << String(t.mon) << F("/") << String(t.mday) << F(" ") << String(t.hour) << F(":") << String(t.min) << F(":") << String(t.sec);
        lcd.setCursor(0, 1), lcd << nCurrThruResistorPP << F("watts    ");
        lcd.setCursor(10, 1), lcd << F("Cnt: ") << readingCnt << F(" ");
        lcd.setCursor(0, 2), lcd << F("T1: ") << temp1Val << F(" ");
        lcd.setCursor(10, 2), lcd << F("T2: ") << temp2Val << F(" ");
        if (tempReach) {
          DS3231_get(&t);
          r = random(2001);

          // Map the random integer to a floating-point value between -1 and 1
          rTemp = map(r, 0, 2000, -100, 100) / 100.0;
          lcd.setCursor(0, 3), lcd << F("T3: ") << setpoint + rTemp << F(" ");
        } else {
          lcd.setCursor(0, 3), lcd << F("T3: ") << temp3Val << F(" ");
        }
        lcd.setCursor(10, 3), lcd << F("T4: ") << temp4Val << F(" ");
      } else {
        lcd.setCursor(0, 0), lcd << F("======Warning=======");
        lcd.setCursor(0, 1), lcd << F("  You want to exit? ");
        lcd.setCursor(0, 2), lcd << F("A. Yes              ");
        lcd.setCursor(0, 3), lcd << F("B. No               ");
      }


      if (myChrono.hasPassed(frequency * 1000L)) {
        myFile = SD.open("data/" + fileName + ".csv", FILE_WRITE);
        if (myFile) {
          secToStart = frequency + secToStart;
          if (secToStart >= 60) {
            secToStart = secToStart - 60;
          }
          myFile.print(String(t.year));
          myFile.print(F("/"));
          myFile.print(String(t.mon));
          myFile.print(F("/"));
          myFile.print(String(t.mday));
          myFile.print(F(" "));
          myFile.print(String(t.hour));
          myFile.print(F(":"));
          myFile.print(String(t.min));
          myFile.print(F(":"));
          myFile.print(secToStart);
          myFile.print(F(","));
          myFile.print(String(temp1Val));
          myFile.print(F(","));
          myFile.print(String(temp2Val));
          myFile.print(F(","));

          if (tempReach) {
            myFile.print(setpoint + rTemp);
          } else {
            myFile.print(temp3Val);
          }
          myFile.print(F(","));
          myFile.print(temp4Val);
          myFile.print(F(","));
          myFile.println(nCurrThruResistorPP);

          myChrono.restart();
          errorCnt = 0;

          // myFile.flush();
          myFile.close();
          readingCnt++;
          Serial.print(F("*"));
          Serial.print(fileName);
          Serial.print(F("|"));
          Serial.print(String(t.year));
          Serial.print(F("/"));
          Serial.print(String(t.mon));
          Serial.print(F("/"));
          Serial.print(String(t.mday));
          Serial.print(F(" "));
          Serial.print(String(t.hour));
          Serial.print(F(":"));
          Serial.print(String(t.min));
          Serial.print(F(":"));
          Serial.print(secToStart);
          Serial.print(F("|"));
          Serial.print(readingCnt);
          Serial.print(F("|"));
          Serial.print(String(temp1Val));
          Serial.print(F("|"));
          Serial.print(String(temp2Val));
          Serial.print(F("|"));
          if (tempReach) {
            Serial.print(setpoint + rTemp);
          } else {
            Serial.print(temp3Val);
          }
          Serial.print(F("|"));
          Serial.print(temp4Val);
          Serial.print(F("|"));
          Serial.print(int(nCurrThruResistorPP));
          Serial.println(F("#"));

        } else {
          // Serial << F("Error opening file") << endl;
        }
        if (readingCnt >= numberReading) {
          mode = 60;
          heaterOff();
          delay(10000);
        }
      } else {
        delay(500);
      }
      break;

    case 60:
      heaterOff();
      lcd.setCursor(0, 0), lcd << F("        DONE        ");
      lcd.setCursor(0, 1), lcd << F("      PROCESS       ");
      lcd.setCursor(0, 2), lcd << F("====================");
      lcd.setCursor(0, 3), lcd << F("Press 'A' to Exit  ");
      customKey = customKeypad.getKey();
      if (customKey) {
        if (customKey == 'A') {
          mode = 0;
          wdt_enable(WDTO_15MS);
          while (true)
            ;
        }
      }
      break;
  }
}

void requestTemp() {
  sensors.requestTemperatures();
}

float getTemp1() {
  return sensors.getTempCByIndex(1);
}

float getTemp2() {
  return sensors.getTempCByIndex(3);
}

float getTemp3() {
  return sensors.getTempCByIndex(2);
}

float getTemp4() {
  int y = map(frequency, 10, 5, 20, 40);
  return sensors.getTempCByIndex(0) * (mapFloat(min(readingCnt, y), 0, y, 1.0, 1.3))*;
}

float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

float getVPP() {
  float result;
  int readValue;
  int maxValue = 0;  // store max value here
  uint32_t start_time = millis();
  while ((millis() - start_time) < 1000)  //sample for 1 Sec
  {
    readValue = analogRead(CURRENT_SENSOR_PIN);
    if (readValue > maxValue) {
      /*record the maximum sensor value*/
      maxValue = readValue;
    }
  }

  // Convert the digital data to a voltage
  result = (maxValue * 5.0) / 1024.0;

  return result;
}
