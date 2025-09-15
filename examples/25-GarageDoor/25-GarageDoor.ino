/*********************************************************************************
 *  MIT License
 *  
 *  Copyright (c) 2020-2024 Gregg E. Berman
 *  
 *  https://github.com/HomeSpan/HomeSpan
 *  
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *  
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *  
 ********************************************************************************/

////////////////////////////////////////////////////////////
//                                                        //
//    HomeSpan: A HomeKit implementation for the ESP32    //
//    ------------------------------------------------    //
//                                                        //
// 简单车库门示例                                          //
// Simple Garage Door Example                             //
//                                                        //
////////////////////////////////////////////////////////////

#include "HomeSpan.h"
#include <WiFi.h>
#include <nvs_flash.h>
#include <esp_wifi.h>

// 伪代码：实际项目中请用真实的传感器和继电器控制
#define GARAGE_DOOR_RELAY_PIN  12   // 控制继电器的GPIO
#define GARAGE_DOOR_SENSOR_PIN 14   // 检测门状态的GPIO
#define RESET_BUTTON_PIN       0    // 重置按钮GPIO (通常使用板载按钮)
#define WIFI_LED_PIN           2    // WiFi状态LED指示灯GPIO

struct GarageDoor : Service::GarageDoorOpener {
  SpanCharacteristic *currentState, *targetState, *obstruction;

  GarageDoor() : Service::GarageDoorOpener() {
    currentState = new Characteristic::CurrentDoorState(1); // 1=Closed
    targetState  = new Characteristic::TargetDoorState(1);  // 1=Close, 0=Open
    obstruction  = new Characteristic::ObstructionDetected(false);
    pinMode(GARAGE_DOOR_RELAY_PIN, OUTPUT);
    pinMode(GARAGE_DOOR_SENSOR_PIN, INPUT_PULLUP);
  }

  boolean update() override {
    int tgt = targetState->getNewVal();
    int cur = currentState->getVal();
    if (tgt != cur) {
      // 触发继电器，模拟开/关门
      digitalWrite(GARAGE_DOOR_RELAY_PIN, LOW);
      delay(500);
      digitalWrite(GARAGE_DOOR_RELAY_PIN, HIGH);

      // 简单模拟门移动
      currentState->setVal(2); // 2=Opening, 3=Closing
      delay(3000); // 假设3秒完成
      currentState->setVal(tgt == 0 ? 0 : 1); // 0=Open, 1=Closed
    }
    return true;
  }
};

// 彻底清除WiFi配置的函数
void clearWiFiCompletely() {
  Serial.println("🧹 开始彻底清除WiFi配置...");
  
  // 1. 断开当前WiFi连接
  WiFi.disconnect(true);
  delay(1000);
  
  // 2. 清除WiFi模式和配置
  WiFi.mode(WIFI_OFF);
  delay(1000);
  
  // 3. 清除ESP32存储的WiFi凭据
  esp_wifi_restore();
  delay(1000);
  
  // 4. 清除NVS分区中的WiFi相关数据
  nvs_flash_erase();
  nvs_flash_init();
  delay(1000);
  
  // 5. 使用HomeSpan的WiFi重置
  homeSpan.processSerialCommand("W");
  delay(1000);
  
  Serial.println("✅ WiFi配置已彻底清除!");
  Serial.println("🔄 设备即将重启...");
  delay(2000);
  
  // 重启设备
  ESP.restart();
}

// 彻底工厂重置的函数
void factoryResetComplete() {
  Serial.println("🏭 开始彻底工厂重置...");
  
  // 1. 清除WiFi配置
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  esp_wifi_restore();
  delay(1000);
  
  // 2. 清除所有NVS数据
  nvs_flash_erase();
  nvs_flash_init();
  delay(1000);
  
  // 3. 使用HomeSpan的工厂重置
  homeSpan.processSerialCommand("R");
  delay(1000);
  
  Serial.println("✅ 工厂重置已完成!");
  Serial.println("🔄 设备即将重启...");
  delay(2000);
  
  // 重启设备
  ESP.restart();
}
// 检查重置按钮的函数
void checkResetButton() {
  static unsigned long pressStart = 0;
  static bool lastState = HIGH;
  
  bool currentState = digitalRead(RESET_BUTTON_PIN);
  
  // 检测按钮按下
  if (lastState == HIGH && currentState == LOW) {
    pressStart = millis();
    Serial.println("🔘 检测到按钮按下...");
  }
  
  // 检测按钮释放
  else if (lastState == LOW && currentState == HIGH) {
    unsigned long pressDuration = millis() - pressStart;
    
    if (pressDuration >= 5000 && pressDuration < 10000) {
      // 5-10秒：彻底重置WiFi配置
      Serial.println("📶 执行彻底WiFi重置...");
      clearWiFiCompletely();  // 调用彻底清除函数
    }
    else if (pressDuration >= 10000) {
      // 10秒以上：彻底工厂重置
      Serial.println("🏭 执行彻底工厂重置...");
      factoryResetComplete();  // 调用彻底工厂重置函数
    }
    else {
      Serial.printf("⏱️  按钮按下时间: %lu ms (需要5秒以上重置WiFi)\n", pressDuration);
    }
    pressStart = 0;
  }
  
  lastState = currentState;
}

void setup() {
  Serial.begin(115200);
  
  // 配置GPIO引脚
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
  pinMode(WIFI_LED_PIN, OUTPUT);
  digitalWrite(WIFI_LED_PIN, LOW);  // 初始状态LED关闭
  
  Serial.println("🚀 HomeSpan车库门控制器启动中...");
  Serial.println("📡 默认启动AP配网模式");
  Serial.println("💡 WiFi状态LED: GPIO2");
  
  // 设置HomeSpan默认启动AP模式
  // homeSpan.setApSSID("HomeSpan-GarageDoor");           // 自定义AP热点名称
  // homeSpan.setApPassword("");                          // 无密码
  // homeSpan.setApTimeout(300);                          // AP模式超时5分钟
  homeSpan.enableAutoStartAP();                        // 启用自动AP模式
  
  
  homeSpan.begin(Category::GarageDoorOpeners, "车库门");

  new SpanAccessory();
    new Service::AccessoryInformation();
      new Characteristic::Identify();
      new Characteristic::Name("Garage Door");
      new Characteristic::Manufacturer("HomeSpan");           // 制造商
      new Characteristic::SerialNumber("HS-GD-001");         // 序列号
      new Characteristic::Model("ESP32 Garage Door");        // 型号
      new Characteristic::FirmwareRevision("1.0.0");        // 固件版本
      new Characteristic::HardwareRevision("1.0");          // 硬件版本
    new GarageDoor();
    
}

void loop() {
  homeSpan.poll();
  checkResetButton();  // 检查重置按钮状态
  
  // 检查WiFi连接状态并控制LED
  static unsigned long lastCheck = 0;
  static bool wasConnected = false;
  static unsigned long blinkTimer = 0;
  static bool ledState = false;
  
  if (millis() - lastCheck > 1000) {  // 每1秒检查一次WiFi状态
    lastCheck = millis();
    
    bool isConnected = (WiFi.status() == WL_CONNECTED);
    
    if (isConnected && !wasConnected) {
      // WiFi刚连接成功
      Serial.println("✅ WiFi连接成功!");
      Serial.printf("📶 已连接到: %s\n", WiFi.SSID().c_str());
      Serial.printf("🌐 IP地址: %s\n", WiFi.localIP().toString().c_str());
      digitalWrite(WIFI_LED_PIN, HIGH);  // 点亮LED
    }
    else if (!isConnected && wasConnected) {
      // WiFi连接丢失
      Serial.println("⚠️  WiFi连接丢失！");
      Serial.println("📡 自动启动AP配网模式...");
      digitalWrite(WIFI_LED_PIN, LOW);   // 关闭LED
      delay(2000);
      homeSpan.processSerialCommand("A");
    }
    
    wasConnected = isConnected;
  }
  
  // WiFi连接中时LED闪烁效果
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - blinkTimer > 500) {  // 每500ms闪烁一次
      blinkTimer = millis();
      ledState = !ledState;
      digitalWrite(WIFI_LED_PIN, ledState);
    }
  }
}
