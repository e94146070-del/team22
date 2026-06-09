/*********
  自動循跡自走車 - 2026 幾何制霸 (進階動態加速與積分抗偏完全體)
  極致升級點：
  1. 🚀【動態大直線狂飆】：大直線時速突破至 230！
     - 連續 200ms 完美走在中央 (error == 0) 時，BASE_SPEED 會從 180 線性抽高到 230。
     - 一旦踩線 (L1, R1, L2, R2 觸發)，瞬間一腳煞車踩回 180，用你最滿意的「黃金速差」穩健破彎。
  2. 🟢【長緩彎 PID 全面補強】：加入微量 Ki (0.8) 與抗飽和限幅 (30)。
     - 專治超長緩彎的微幅穩態偏線，出彎或全黑十字時自動清空累積量，防抖防暴走。
  3. ⚡【保留原有完美地基】：25 內側限幅微調、單側外線直角大甩 (-210)、0.96 右輪物理補償全數保留。
*********/

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
// 🌟 幾何進階專用參數調校區
// =========================================================================
double Kp = 120.0;   
double Ki = 0.8;     // 🔥 進階：微量積分項，專治長緩彎穩態偏線
double Kd = 75.0;   

// 【🚀 動態速度控制核心】
const int CRUISE_SPEED    = 180; // 彎道與常態基底時速
const int ROCKET_SPEED    = 230; // 🔥 大直線極致狂飆時速
int currentBaseSpeed      = CRUISE_SPEED; 

unsigned long straightStartTime = 0; // 直線計時器
bool isStraight                 = false;

// 【🟢 積分與限幅核心】
double integral         = 0.0;
const int INT_LIMIT     = 30; // 抗飽和限制：防止 Ki 累積過大導致出彎甩尾抖動
const int MAX_INNER_ADJUST = 25; // 內側微調最大限制量

// 【⚡ 90度 直角大甩參數】
const int SHARP_TURN_SPEED = 255; 
const int SHARP_REV_SPEED  = -210; 

// 【🌟 全時物理出力補償】
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

  // 左馬達
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

  // 右馬達
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
    integral = 0; // 安全模式下清空積分
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

    // ---------------------------------------------------------------------
    // 🚨【最高優先權：五路全白 —— 脫軌/懸空熄火】
    // ---------------------------------------------------------------------
    if (l2 == 0 && l1 == 0 && m == 0 && r1 == 0 && r2 == 0) {
      unsigned long offTrackTime = millis();
      integral = 0; // 脫軌時清空積分
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

    // ---------------------------------------------------------------------
    // ⚡【第二優先權：五路全黑 —— 十字路口推進】
    // ---------------------------------------------------------------------
    if (l2 == 1 && l1 == 1 && m == 1 && r1 == 1 && r2 == 1) {
      moveCar(CRUISE_SPEED, CRUISE_SPEED); // 十字路口維持穩健基礎速推進
      lastError = 0; 
      integral = 0; // 遇到十字清空積分，防止干擾
      currentBaseSpeed = CRUISE_SPEED;
      isStraight = false;
      return; 
    }

    // ---------------------------------------------------------------------
    // ⚡【第三優先權：標準單側 90度直角彎特判 —— 大腳暴力大甩尾】
    // ---------------------------------------------------------------------
    if ((l2 == 1 && r2 == 0) || (r2 == 1 && l2 == 0)) {
      integral = 0; // 直角彎強制清空積分
      currentBaseSpeed = CRUISE_SPEED; // 煞車踩回基準速
      isStraight = false;

      if (l2 == 1) moveCar(SHARP_TURN_SPEED, SHARP_REV_SPEED); 
      else         moveCar(SHARP_REV_SPEED, SHARP_TURN_SPEED);
      return;
    }

    // ---------------------------------------------------------------------
    // 🟢【第四優先權：0度直線 與 45度彎】進階運動學 PID 運算
    // ---------------------------------------------------------------------
    int error = getPositionError();

    // 🚀【大直線動態加速判定核心】
    // 只有當完美走在中央 (error == 0) 且外側及內側兩輪都沒有任何踩黑線痕跡時
    if (error == 0 && l1 == 0 && r1 == 0 && l2 == 0 && r2 == 0) {
      if (!isStraight) {
        straightStartTime = millis();
        isStraight = true;
      }
      // 完美走在直線上超過 200ms，啟動動態抽速加速
      if (millis() - straightStartTime > 200) {
        currentBaseSpeed = ROCKET_SPEED; // 抽高時速至 230！
      }
    } else {
      // ⚡【瞬間踩煞車】：只要一偏線，無條件瞬間踩回 180 穩定過彎
      currentBaseSpeed = CRUISE_SPEED; 
      isStraight = false;
    }

    // 🟢【PID 運算 (含 Ki 抗偏)】
    double P = error;                            
    double D = error - lastError;                
    lastError = error;                           

    // 如果在正中央，就緩步遞減積分；如果偏線，則累積積分
    if (error == 0) {
      integral *= 0.8; // 緩步衰減
    } else {
      integral += error;
      integral = constrain(integral, -INT_LIMIT, INT_LIMIT); // 抗飽和限幅
    }

    int turnAdjust = -(int)((Kp * P) + (Ki * integral) + (Kd * D));

    // 🌟【內側剛性限幅】：只有在純內側微調時，強行定格修正振幅，維持高扭力速差
    if (l2 == 0 && r2 == 0) {
      turnAdjust = constrain(turnAdjust, -MAX_INNER_ADJUST, MAX_INNER_ADJUST);
    }

    // 使用動態計算出的基底速度進行出力
    int leftSpeed  = currentBaseSpeed + turnAdjust;
    int rightSpeed = currentBaseSpeed - turnAdjust;

    moveCar(leftSpeed, rightSpeed);
  }
}