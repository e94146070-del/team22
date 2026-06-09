// =========================================================================
// 1. 腳位與全域變數定義
// =========================================================================
const int enableLeft  = 25; 
const int motorLeft1  = 26; 
const int motorLeft2  = 27; 
const int motorRight1 = 14; 
const int motorRight2 = 32; 
const int enableRight = 13; 

const int SENSOR_PINS[5] = {16, 4, 17, 18, 5}; 

const int LED_R = 19;     
const int LED_G = 21;     
const int LED_B = 2;      
const int START_BUTTON = 23; 

enum CarState { STATE_SAFE, STATE_AUTO };
CarState currentStatus = STATE_SAFE; 

unsigned long previousMillis = 0;
const long interval2Hz = 250; 
bool ledState = LOW;

// =========================================================================
// 幾何進階專用參數調校區
// =========================================================================
double Kp = 120.0;   
double Ki = 0.8;     
double Kd = 75.0;   

const int CRUISE_SPEED    = 140; 
const int ROCKET_SPEED    = 230; 
int currentBaseSpeed      = CRUISE_SPEED; 

unsigned long straightStartTime = 0; 
bool isStraight                 = false;

double integral         = 0.0;
const int INT_LIMIT     = 30; 
const int MAX_INNER_ADJUST = 220; 

const int SHARP_TURN_SPEED = 255; 
const int SHARP_REV_SPEED  = -210; 

const double LEFT_MOTOR_COMPENSATE  = 1.00; 
const double RIGHT_MOTOR_COMPENSATE = 0.96; 

int lastError = 0;
int lastValidDirection = 0; 

// =========================================================================
// 2. 核心驅動子函式
// =========================================================================
void moveCar(int leftSpeed, int rightSpeed) {
  if (leftSpeed != 0)  leftSpeed  = (int)(leftSpeed * LEFT_MOTOR_COMPENSATE);
  if (rightSpeed != 0) rightSpeed = (int)(rightSpeed * RIGHT_MOTOR_COMPENSATE);

  leftSpeed  = constrain(leftSpeed, -240, 255);
  rightSpeed = constrain(rightSpeed, -240, 255);

  if (leftSpeed > 0) {
    digitalWrite(motorLeft1, LOW);  digitalWrite(motorLeft2, HIGH); 
    analogWrite(enableLeft, leftSpeed);
  } else if (leftSpeed < 0) {
    digitalWrite(motorLeft1, HIGH); digitalWrite(motorLeft2, LOW);  
    analogWrite(enableLeft, abs(leftSpeed));
  } else {
    digitalWrite(motorLeft1, HIGH);  digitalWrite(motorLeft2, HIGH);  
    analogWrite(enableLeft, 255);
  }

  if (rightSpeed > 0) {
    digitalWrite(motorRight1, LOW);  digitalWrite(motorRight2, HIGH); 
    analogWrite(enableRight, rightSpeed);
  } else if (rightSpeed < 0) {
    digitalWrite(motorRight1, HIGH); digitalWrite(motorRight2, LOW);  
    analogWrite(enableRight, abs(rightSpeed));
  } else {
    digitalWrite(motorRight1, HIGH);  digitalWrite(motorRight2, HIGH);  
    analogWrite(enableRight, 255);
  }
}

int getPositionError() {
  int l2 = digitalRead(SENSOR_PINS[0]); 
  int l1 = digitalRead(SENSOR_PINS[1]); 
  int m  = digitalRead(SENSOR_PINS[2]); 
  int r1 = digitalRead(SENSOR_PINS[3]); 
  int r2 = digitalRead(SENSOR_PINS[4]); 

  int count = l2 + l1 + m + r1 + r2;
  if (count == 0) return 0; 

  int sum = (l2 * -5) + (l1 * -4) + (m * 0) + (r1 * 4) + (r2 * 5);
  int error = sum / count;

  if (error < 0) lastValidDirection = -1;
  if (error > 0) lastValidDirection = 1;

  return error;
}

void updateLED() {
  unsigned long currentMillis = millis();
  switch (currentStatus) {
    case STATE_SAFE:
      digitalWrite(LED_R, LOW);  digitalWrite(LED_G, HIGH); digitalWrite(LED_B, LOW);
      break;
    case STATE_AUTO:
      digitalWrite(LED_G, LOW);  digitalWrite(LED_B, LOW);
      if (currentMillis - previousMillis >= interval2Hz) {
        previousMillis = currentMillis;
        ledState = !ledState;
        digitalWrite(LED_R, ledState); 
      }
      break;
  }
}

// =========================================================================
// 3. 主程式區
// =========================================================================
void setup() {
  Serial.begin(115200);
  analogWrite(enableLeft, 0); analogWrite(enableRight, 0);

  pinMode(motorLeft1, OUTPUT);  pinMode(motorLeft2, OUTPUT);  pinMode(enableLeft, OUTPUT);
  pinMode(motorRight1, OUTPUT); pinMode(motorRight2, OUTPUT); pinMode(enableRight, OUTPUT);
  pinMode(LED_R, OUTPUT); pinMode(LED_G, OUTPUT); pinMode(LED_B, OUTPUT);
  
  for (int i = 0; i < 5; i++) pinMode(SENSOR_PINS[i], INPUT);
  pinMode(START_BUTTON, INPUT_PULLUP);
  
  digitalWrite(motorLeft1, LOW);  digitalWrite(motorLeft2, LOW);
  digitalWrite(motorRight1, LOW); digitalWrite(motorRight2, LOW);
  Serial.println("DYNAMIC ROCKET ENGINE ONLINE");
}

void loop() {
  updateLED();

  if (currentStatus == STATE_SAFE) {
    moveCar(0, 0); 
    integral = 0; 
    currentBaseSpeed = CRUISE_SPEED;
    isStraight = false;
    if (digitalRead(START_BUTTON) == LOW) {
      delay(50);
      if (digitalRead(START_BUTTON) == LOW) {
        lastError = 0; 
        currentStatus = STATE_AUTO; previousMillis = millis(); 
        while(digitalRead(START_BUTTON) == LOW) { delay(10); } 
      }
    }
  }
  
  else if (currentStatus == STATE_AUTO) {
    int l2 = digitalRead(SENSOR_PINS[0]); 
    int l1 = digitalRead(SENSOR_PINS[1]); 
    int m  = digitalRead(SENSOR_PINS[2]); 
    int r1 = digitalRead(SENSOR_PINS[3]); 
    int r2 = digitalRead(SENSOR_PINS[4]); 

    if (l2 == 0 && l1 == 0 && m == 0 && r1 == 0 && r2 == 0) {
      unsigned long offTrackTime = millis();
      integral = 0; 
      currentBaseSpeed = CRUISE_SPEED;
      isStraight = false;
      
      while (digitalRead(SENSOR_PINS[0]) == 0 && digitalRead(SENSOR_PINS[1]) == 0 && 
             digitalRead(SENSOR_PINS[2]) == 0 && digitalRead(SENSOR_PINS[3]) == 0 && 
             digitalRead(SENSOR_PINS[4]) == 0) {
        
        if (millis() - offTrackTime > 300) { 
          moveCar(0, 0); 
          currentStatus = STATE_SAFE; 
          return; 
        }
        updateLED();
        if (lastValidDirection == -1) moveCar(130, -130); 
        else moveCar(-130, 130);
      }
      return; 
    }

    if (l2 == 1 && l1 == 1 && m == 1 && r1 == 1 && r2 == 1) {
      moveCar(CRUISE_SPEED, CRUISE_SPEED); 
      lastError = 0; 
      integral = 0; 
      currentBaseSpeed = CRUISE_SPEED;
      isStraight = false;
      return; 
    }

    if ((l2 == 1 && r2 == 0) || (r2 == 1 && l2 == 0)) {
      integral = 0; 
      currentBaseSpeed = CRUISE_SPEED; 
      isStraight = false;

      if (l2 == 1) moveCar(SHARP_TURN_SPEED, SHARP_REV_SPEED); 
      else         moveCar(SHARP_REV_SPEED, SHARP_TURN_SPEED);
      return;
    }

    int error = getPositionError();

    if (error == 0 && l1 == 0 && r1 == 0 && l2 == 0 && r2 == 0) {
      if (!isStraight) {
        straightStartTime = millis();
        isStraight = true;
      }
      if (millis() - straightStartTime > 200) {
        currentBaseSpeed = ROCKET_SPEED; 
      }
    } else {
      currentBaseSpeed = CRUISE_SPEED;    
      isStraight = false;
    }

    double P = error;                                            
    double D = error - lastError;                                
    lastError = error;                                           

    if (error == 0) {
      integral *= 0.8; 
    } else {
      integral += error;
      integral = constrain(integral, -INT_LIMIT, INT_LIMIT); 
    }

    int turnAdjust = -(int)((Kp * P) + (Ki * integral) + (Kd * D));

    if (l2 == 0 && r2 == 0) {
      turnAdjust = constrain(turnAdjust, -MAX_INNER_ADJUST, MAX_INNER_ADJUST);
    }

    int leftSpeed  = currentBaseSpeed + turnAdjust;
    int rightSpeed = currentBaseSpeed - turnAdjust;

    moveCar(leftSpeed, rightSpeed);
  }
}