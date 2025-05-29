// ========= Initialisation écran OLED ========= //
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <SoftwareSerial.h>

#define nombreDePixelsEnLargeur 128
#define nombreDePixelsEnHauteur 64
#define brocheResetOLED -1
#define adresseI2CecranOLED 0x3C
Adafruit_SSD1306 ecranOLED(nombreDePixelsEnLargeur, nombreDePixelsEnHauteur, &Wire, brocheResetOLED);

// ========= Initialisation flex sensor ========= //
const int flexPin = A1;
const float VCC = 5;
const float R_DIV = 33000.0;
const float flatResistance = 33000.0;
const float bendResistance = 60000.0;

// ========= Initialisation capteur graphène ========= //
float R1 = 100000.0;
float R2 = 1000.0;
float R3 = 100000.0;
float R5 = 10000.0;
float Rc = 0.0;
const int analogInPin = A0;

// ========= Initialisation encodeur rotatoire ========= //
#define CLK 2
#define DT 4
#define Switch 5
volatile int menuIndex = 0;
volatile bool menuChanged = false;
bool buttonState = HIGH;
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;
int lastStateCLK;
const char* menuItems[] = {"Calib Potar", "FlexSensor", "Capteur Graph","Bleutooth"};

// ========= Initialisation potentiometre digital ========= //
const byte csPin = 10;
const int maxPositions = 256;
const long rAB = 46000;
const byte rWiper = 125;
const byte pot0 = 0x11;
const int resistanceOptions[4] = {100, 1000, 10000, 100000};
volatile int resistanceIndex = 1;  // par défaut R2 = 1000

void setPotWiper(int addr, int pos) {
  pos = constrain(pos, 0, 255);
  digitalWrite(csPin, LOW);
  SPI.transfer(addr);
  SPI.transfer(pos);
  digitalWrite(csPin, HIGH);
  long resistanceWB = ((rAB * pos) / maxPositions ) + rWiper;
  Serial.print("Wiper position : ");
  Serial.print(pos);
  Serial.print(" Resistance wiper to B terminal : ");
  Serial.print(resistanceWB);
  Serial.println(" ohms");
}

// ========= Module Bluetooth ========= //
#define rxPin 8
#define txPin 7
#define baudrate 9600
SoftwareSerial mySerial(rxPin, txPin);
volatile bool inBluetoothMode = false;

// ========= Setup ========= //
void setup() {
  Serial.begin(9600);
  ecranOLED.begin(SSD1306_SWITCHCAPVCC, adresseI2CecranOLED);
  ecranOLED.setTextSize(1);
  ecranOLED.setTextColor(SSD1306_WHITE);

  pinMode(flexPin, INPUT);
  pinMode(CLK, INPUT_PULLUP);
  pinMode(DT, INPUT_PULLUP);
  pinMode(Switch, INPUT_PULLUP);
  lastStateCLK = digitalRead(CLK);

  pinMode(csPin, OUTPUT);
  digitalWrite(csPin, HIGH);
  SPI.begin();

  pinMode(rxPin, INPUT);
  pinMode(txPin, OUTPUT);
  mySerial.begin(baudrate);
}

void loop() {
  int reading = digitalRead(Switch);
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == LOW) {
        if (inBluetoothMode){
          inBluetoothMode = false;
          drawMenu();
        } else {
          handleMenuSelection(menuIndex);
        }
        delay(500);
      }
    }
  }
  lastButtonState = reading;

  if (!inBluetoothMode) {
    int currentStateCLK = digitalRead(CLK);
    if (currentStateCLK != lastStateCLK) {
      if (digitalRead(DT) != currentStateCLK) menuIndex++;
      else                                   menuIndex--;
      if (menuIndex < 0) menuIndex = 3;
      if (menuIndex > 3) menuIndex = 0;
      menuChanged = true;
    }
    lastStateCLK = currentStateCLK;

    if (menuChanged) {
      drawMenu();
      menuChanged = false;
    }
  }
}

void drawMenu() {
  ecranOLED.clearDisplay();
  ecranOLED.setTextSize(1);
  ecranOLED.setTextColor(SSD1306_WHITE);

  for (int i = 0; i < 4; i++) {
    if (i == menuIndex) {
      ecranOLED.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    } else {
      ecranOLED.setTextColor(SSD1306_WHITE);
    }
    ecranOLED.setCursor(0, i * 10);
    ecranOLED.print(menuItems[i]);
  }
  ecranOLED.display();
}

void handleMenuSelection(int index) {
  ecranOLED.clearDisplay();
  ecranOLED.setCursor(0, 0);
  ecranOLED.setTextColor(SSD1306_WHITE);
  switch (index) {
    case 0:
      ecranOLED.println("Calibration Potar");
      ecranOLED.display();
      calibratePotentiometer();
      delay(5000);
      break;
    case 1:
      ecranOLED.println("Mesure Flex:");
      ecranOLED.display();
      measureFlexSensor();
      delay(5000);
      break;
    case 2:
      ecranOLED.println("Mesure Graphite:");
      ecranOLED.display();
      measureGraphiteSensor();
      delay(5000);
      break;
    case 3:
      ecranOLED.println("Bleutooth");
      ecranOLED.display();
      inBluetoothMode = true;
      bluetooth();
      delay(5000);
      break;
  }
  delay(2000);
  drawMenu();
}

void calibratePotentiometer() {
  for (int i = 0; i <= 255; i += 20) {
    setPotWiper(pot0, i);
    delay(100);
  }
  setPotWiper(pot0, 128);
}

void measureFlexSensor() {
  int ADCflex = analogRead(flexPin);
  float Vflex = ADCflex * VCC / 1023.0;
  float Rflex = R_DIV / (VCC / Vflex - 1.0);
  ecranOLED.setCursor(0, 20);
  ecranOLED.print("Rflex: ");
  ecranOLED.print(Rflex / 1000.0);
  ecranOLED.println(" kOhm");
  ecranOLED.display();
}

void measureGraphiteSensor() {
  int sensorValue = analogRead(analogInPin);
  float Vsensor = sensorValue * VCC / 1023.0;
  float Rsensor = ((R1*(1+R3/R2)*(VCC/Vsensor))-R1-R5) * 0.000001;
  ecranOLED.setCursor(0, 20);
  ecranOLED.print("Graphite: ");
  ecranOLED.println(sensorValue);
  ecranOLED.print("R: ");
  ecranOLED.print(Rsensor);
  ecranOLED.println(" MOhm");
  ecranOLED.display();

  char Rsensor_ASCII[10];
  dtostrf(Rsensor, 5, 2, Rsensor_ASCII);
  mySerial.write(Rsensor_ASCII);
}

void bluetooth(){
  while (inBluetoothMode) {
    int sensorValue = analogRead(analogInPin);
    float Vsensor = sensorValue * VCC / 1023.0;
    float Rsensor = ((R1*(1+R3/R2)*(VCC/Vsensor)) - R1 - R5) * 1e-6;
    // Affichage OLED (rafraîchissement continu)
    ecranOLED.clearDisplay();
    ecranOLED.setCursor(0,0);
    ecranOLED.println("Bluetooth:");
    ecranOLED.setCursor(0,20);
    ecranOLED.print("R: ");
    ecranOLED.print(Rsensor, 3);
    ecranOLED.println(" MOhm");
    ecranOLED.display();
  
    char Rsensor_ASCII[10];
    dtostrf(Rsensor, 5, 2, Rsensor_ASCII);
    mySerial.write(Rsensor_ASCII);
    delay(2000);
  }
}