// PIN_PWM_L1,PIN_PWM_L2,PIN_PWM_R1,PIN_PWM_R2  通过PWM控制底层电机
// PIN_SPEED_L, PIN_SPEED_R                     通过(光电)测量一定时间内码盘孔转动数量计算转速
// PIN_VIN                                      通过分压电路测量电源电压
// PIN_TRIGGER                                  超声波触发引脚
// PIN_ECHO                                     超声波回应引脚
// PIN_LED_LB, PIN_LED_RB                       左右指示灯

// 是否开启脱机模式
// 如果开启脱机模式
// - 电机将会以75%的速度转定
// - 超声波传感器检测到障碍物后, 将会减速
// - 如果与障碍物的距离在STOP_THRESHOLD内, 小车将会转向
// 注意: 如果没有安装超声波传感器, 小车将会全速前进
#define NO_PHONE_MODE 0

//------------------------------------------------------//
// 输出引脚
//------------------------------------------------------//

// 引脚定义
#define PIN_PWM_L1 5
#define PIN_PWM_L2 6
#define PIN_PWM_R1 9
#define PIN_PWM_R2 10
#define PIN_SPEED_L 2
#define PIN_SPEED_R 3
#define PIN_VIN A7
#define PIN_TRIGGER 12
#define PIN_ECHO 11
#define PIN_LED_LB 4
#define PIN_LED_RB 7

//------------------------------------------------------//
// 初始化
//------------------------------------------------------//

#include <limits.h>
const unsigned int STOP_THRESHOLD = 15; //cm

#if NO_PHONE_MODE
  int turn_direction = 0; // right
  const unsigned long TURN_DIRECTION_INTERVAL = 2000; // 转变方向的频率(ms).
  unsigned long turn_direction_timeout = 0;   // 延时(ms)后, 随机转向
#endif

#include <PinChangeInterrupt.h>

const float US_TO_CM = 0.01715; // cm/uS -> (343 * 100 / 1000000) / 2; 声波一微秒走过的距离
const unsigned long PING_INTERVAL = 100; // 超声波测量距离的频率(ms)
unsigned long ping_timeout = 0;   // 超时(ms)后, 距离被设置为无限大(65535)
volatile unsigned long start_time = 0; // 触发超声波的时间
volatile unsigned long echo_time = 0; // 发出到接收花费的时间
unsigned int distance = UINT_MAX; //cm 实际距离
unsigned int distance_estimate = UINT_MAX; //cm 预估距离

const unsigned int distance_array_sz = 3; // 距离数组长度
unsigned int distance_array[distance_array_sz]={}; // 距离数组
unsigned int distance_counter = 0; // 距离计数
  

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

const int OLED_RESET = -1; // not used
Adafruit_SSD1306 display(OLED_RESET);

// 使用SSD1306 OLED屏
const unsigned int SCREEN_WIDTH = 128; // 宽度(像素)
const unsigned int SCREEN_HEIGHT = 32; // 高度(像素)

const unsigned int ADC_MAX = 1023; 
const unsigned int VREF = 5; // 参考电压 voltage reference 
const float VOLTAGE_DIVIDER_FACTOR = 5; // 分压因子 (R1+R2)/R2

// 车辆控制
int ctrl_left = 0;
int ctrl_right = 0;

// 电压计算
const unsigned int VIN_ARR_SZ = 10;
unsigned int vin_counter = 0;
unsigned int vin_array[VIN_ARR_SZ]={0};

// 速度测算
const unsigned int DISK_HOLES = 20; // 码盘孔数
volatile int counter_left = 0; // 左电机满盘孔计数
volatile int counter_right = 0; // 右电机满盘孔计数

// 指示灯
const unsigned long INDICATOR_INTERVAL = 500; // 指示灯闪烁频率
unsigned long indicator_timeout = 0;
int indicator_val = 0;

// 串口通信
const unsigned long SEND_INTERVAL = 1000; // MCU向外传输数据频率(ms)
unsigned long send_timeout = 0; 
String inString = "";

//------------------------------------------------------//
// SETUP(仅执行一次)
//------------------------------------------------------//

void setup() {
  // 输出
  pinMode(PIN_PWM_L1,OUTPUT);
  pinMode(PIN_PWM_L2,OUTPUT);
  pinMode(PIN_PWM_R1,OUTPUT);
  pinMode(PIN_PWM_R2,OUTPUT);
  pinMode(PIN_LED_LB,OUTPUT);
  pinMode(PIN_LED_RB,OUTPUT);

  // 输入
  pinMode(PIN_VIN,INPUT);       
  pinMode(PIN_SPEED_L,INPUT);
  pinMode(PIN_SPEED_R,INPUT);

  Serial.begin(115200,SERIAL_8N1); // 8个数据位 1个停止位 无校验
  send_timeout = millis() + SEND_INTERVAL; // 读取周期

  // 都通过中断更新计数器
  attachPinChangeInterrupt(digitalPinToPinChangeInterrupt(PIN_SPEED_L), update_speed_left, FALLING);
  attachPinChangeInterrupt(digitalPinToPinChangeInterrupt(PIN_SPEED_R), update_speed_right, FALLING);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  
  digitalWrite(PIN_LED_LB,LOW);
  digitalWrite(PIN_LED_RB,LOW);
  delay(500);
  digitalWrite(PIN_LED_LB,HIGH);
  delay(500);
  digitalWrite(PIN_LED_LB,LOW);
  digitalWrite(PIN_LED_RB,HIGH);
  delay(500);
  digitalWrite(PIN_LED_RB,LOW);
}

//------------------------------------------------------//
// MAIN LOOP(一直执行)
//------------------------------------------------------//

void loop() {
  // 测量电压
  vin_array[vin_counter%VIN_ARR_SZ] = analogRead(PIN_VIN);
  vin_counter++;
  
  // 每一个PING_INTERVAL测量一次距离
  if (millis() >= ping_timeout) {
    ping_timeout = ping_timeout + PING_INTERVAL;
    if (echo_time == 0) { // 未收到echo 前方无障碍物
      distance = UINT_MAX;
    } else {
      distance = echo_time * US_TO_CM;
    }
    distance_array[distance_counter%distance_array_sz] = distance;
    distance_counter++;
    // 取中值
    distance_estimate = get_median(distance_array, distance_array_sz);
    send_ping();
  }
  
  // 每一个SEND_INTERVAL向外发送一次数据
  if (millis() >= send_timeout) {
    send_vehicle_data();
    send_timeout = millis() + SEND_INTERVAL;
  }


  // 每一个INDICATOR_INTERVAL更新一次指示灯
  if (millis() >= indicator_timeout) {
    update_indicators();
    indicator_timeout = millis() + INDICATOR_INTERVAL;
  }
  
  #if (NO_PHONE_MODE)
    if (millis() > turn_direction_timeout){
      turn_direction_timeout = millis() + TURN_DIRECTION_INTERVAL;
      turn_direction = random(2); //随机产生0(左转)或1(右转)
    }
    // 直行
    if (distance_estimate > 4*STOP_THRESHOLD) {
      Serial.println("forward");
      ctrl_left = distance_estimate;
      ctrl_right = ctrl_left;
      digitalWrite(PIN_LED_LB, LOW);
      digitalWrite(PIN_LED_RB, LOW);
    }
    // 轻微转弯
    else if (distance_estimate > 2*STOP_THRESHOLD) {
      Serial.println("turn slightly");
      ctrl_left = distance_estimate;
      ctrl_right = ctrl_left - 3*STOP_THRESHOLD;
    }
    // 急剧转弯
    else if (distance_estimate > STOP_THRESHOLD) {
      Serial.println("turn strongly");
      ctrl_left = 40;
      ctrl_right = - 40;
    }
    // 缓慢后退
    else {
      Serial.println("backward");
        ctrl_left = -96;
        ctrl_right = -96;
        digitalWrite(PIN_LED_LB, HIGH);
        digitalWrite(PIN_LED_RB, HIGH);
    }
    // 按需反转控制并设置指示灯
    if (ctrl_left != ctrl_right) {
      if (turn_direction > 0) {
        int temp = ctrl_left;
        ctrl_left = ctrl_right;
        ctrl_right = temp;
        digitalWrite(PIN_LED_LB, HIGH);
        digitalWrite(PIN_LED_RB, LOW);
      } else {
        digitalWrite(PIN_LED_LB, LOW);
        digitalWrite(PIN_LED_RB, HIGH);
      }
    }
    // 限定点击转速, 防止点击转速过快
    ctrl_left = ctrl_left > 0 ? max(64, min(ctrl_left, 192)) : min(-64, max(ctrl_left, -192));
    ctrl_right = ctrl_right > 0 ? max(64, min(ctrl_right, 192)) : min(-64, max(ctrl_right, -192));

  #else // 等待来自外部的指令
    if (Serial.available() > 0) {
      read_msg();
    }
    if (distance_estimate < STOP_THRESHOLD) {
      if (ctrl_left > 0) ctrl_left = 0;
      if (ctrl_right > 0) ctrl_right = 0;
    }
  #endif

  // 更新左右点击转速
  update_left_motors();
  update_right_motors();
}


//------------------------------------------------------//
// FUNCTIONS
//------------------------------------------------------//

void update_left_motors() {
    if (ctrl_left < 0) {
      analogWrite(PIN_PWM_L1,-ctrl_left);
      analogWrite(PIN_PWM_L2,0);
    } else if (ctrl_left > 0) {
      analogWrite(PIN_PWM_L1,0);
      analogWrite(PIN_PWM_L2,ctrl_left);
    } else { //Motor brake
      analogWrite(PIN_PWM_L1,255);
      analogWrite(PIN_PWM_L2,255);
    }
}

void update_right_motors() {
    if (ctrl_right < 0) {
      analogWrite(PIN_PWM_R1,-ctrl_right);
      analogWrite(PIN_PWM_R2,0);
    } else if (ctrl_right > 0) {
      analogWrite(PIN_PWM_R1,0);
      analogWrite(PIN_PWM_R2,ctrl_right);
    } else { //Motor brake
      analogWrite(PIN_PWM_R1,255);
      analogWrite(PIN_PWM_R2,255);
    }
}

void read_msg() {
  if (Serial.available()) {
    char inChar = Serial.read();
    switch (inChar) {
      case 'c':
        ctrl_left = Serial.readStringUntil(',').toInt();
        ctrl_right = Serial.readStringUntil('\n').toInt();
        break;
      case 'i':
        indicator_val = Serial.readStringUntil('\n').toInt();
        break;
      default:
        break;
    }
  }
}

void send_vehicle_data() {
  float voltage_value = get_voltage();
  // 一个发送周期内的孔数
  int ticks_left = counter_left;
  counter_left = 0;
  int ticks_right = counter_right;
  counter_right = 0;
  
  float rpm_factor = 60.0*(1000.0/SEND_INTERVAL)/(DISK_HOLES);
  float rpm_left = ticks_left*rpm_factor;
  float rpm_right = ticks_right*rpm_factor;
  
  #if (NO_PHONE_MODE)
    Serial.print("Voltage: "); Serial.println(voltage_value, 2);
    Serial.print("Left RPM: "); Serial.println(rpm_left, 0);
    Serial.print("Right RPM: "); Serial.println(rpm_right, 0);
    Serial.print("Distance: "); Serial.println(distance_estimate);
    Serial.println("------------------");
  #else
    Serial.print(voltage_value);
    Serial.print(",");
    Serial.print(ticks_left);
    Serial.print(",");
    Serial.print(ticks_right);
    Serial.print(",");
    Serial.print(distance_estimate);
    Serial.println();
  #endif 
  
    // 设定OLED信息
  drawString(
    "Voltage:    " + String(voltage_value,2), 
    "Left RPM:  " + String(rpm_left,0), 
    "Right RPM: " + String(rpm_right, 0), 
    "Distance:   " + String(distance_estimate));
}

// 计算均值
float get_voltage () {
  unsigned long array_sum = 0;
  unsigned int array_size = min(VIN_ARR_SZ,vin_counter);
  for(unsigned int index = 0; index < array_size; index++) { 
    array_sum += vin_array[index]; 
  }
  return float(array_sum)/array_size/ADC_MAX*VREF*VOLTAGE_DIVIDER_FACTOR;
}

void update_indicators() {
  if (indicator_val < 0) {
    digitalWrite(PIN_LED_LB, !digitalRead(PIN_LED_LB));
    digitalWrite(PIN_LED_RB, 0);
  } else if (indicator_val > 0) {
    digitalWrite(PIN_LED_LB, 0);
    digitalWrite(PIN_LED_RB, !digitalRead(PIN_LED_RB));
  } else {
    digitalWrite(PIN_LED_LB, 0);
    digitalWrite(PIN_LED_RB, 0);
  }
}

// 在OLED屏上输出字符串
void drawString(String line1, String line2, String line3, String line4) {
  display.clearDisplay();
  // 设置字体颜色
  display.setTextColor(WHITE);
  // 设置字体大小
  display.setTextSize(1);
  // 设置指针位置
  display.setCursor(1,0);
  // 显示文本
  display.println(line1);
  display.setCursor(1,8);
  // 显示文本
  display.println(line2);
  display.setCursor(1,16);
  // 显示文本
  display.println(line3);
  display.setCursor(1,24);
  // 显示文本
  display.println(line4);    
  display.display();
}

// 使用冒泡排序 对距离进行排序 返回中值
unsigned int get_median(unsigned int a[], unsigned int sz) {
  //bubble sort
  for(unsigned int i=0; i<(sz-1); i++) {
    for(unsigned int j=0; j<(sz-(i+1)); j++) {
      if(a[j] > a[j+1]) {
          unsigned int t = a[j];
          a[j] = a[j+1];
          a[j+1] = t;
      }
    }
  }
  return a[sz/2];
}

//------------------------------------------------------//
// INTERRUPT SERVICE ROUTINES (ISR)
// 中断服务程序
//------------------------------------------------------//

// 增加左计数器读数
void update_speed_left() {
  if (ctrl_left < 0) {
    counter_left--; 
  } else if (ctrl_left > 0) {
    counter_left++;
  }
}

// 增加右计数器读数
void update_speed_right() {
  if (ctrl_right < 0) {
    counter_right--; 
  } else if (ctrl_right > 0){
    counter_right++;
  }
}

// 触发trigger引脚
void send_ping() {
  detachPinChangeInterrupt(digitalPinToPinChangeInterrupt(PIN_ECHO));
  echo_time = 0;
  pinMode(PIN_TRIGGER,OUTPUT);
  digitalWrite(PIN_TRIGGER, LOW);
  delayMicroseconds(5);
  digitalWrite(PIN_TRIGGER, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIGGER, LOW);
  pinMode(PIN_ECHO,INPUT);
  attachPinChangeInterrupt(digitalPinToPinChangeInterrupt(PIN_ECHO), start_timer, RISING);
}

// 开启定时器记录echo引脚回复时间
void start_timer() {
  start_time = micros();
  attachPinChangeInterrupt(digitalPinToPinChangeInterrupt(PIN_ECHO), stop_timer, FALLING);
}

// 停止计数器, 并记录所用时间
void stop_timer() {
  echo_time = micros() - start_time;
}
