#include <WiFi.h>
#include <PubSubClient.h>

// include file config ที่เราเก็บค่าการตั้งค่าต่างๆมาเก็บไว้
#include "config.h"


// include Gy32
#include <Wire.h>

uint8_t rht[4];
float humidity;
float temperatureC;
float temperatureF;

// include LED 
const int ledPin = 12;

//--- include segment
#include <Arduino.h>
#include <TM1637Display.h>

// Module connection pins (Digital Pins)
#define CLK 5
#define DIO 4
TM1637Display display(CLK, DIO);

// The amount of time (in milliseconds) between tests
// #define TEST_DELAY   2000

const uint8_t SEG_DONE[] = {
  SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,           // d
  SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,   // O
  SEG_C | SEG_E | SEG_G,                           // n
  SEG_A | SEG_D | SEG_E | SEG_F | SEG_G            // E
};

const uint8_t SEG_LUX[] = {
SEG_F | SEG_E | SEG_D, // L
SEG_F | SEG_E | SEG_D | SEG_C | SEG_B, // U
SEG_B | SEG_C | SEG_E | SEG_F | SEG_G // X
};


const uint8_t SEG_OK[] = {
  SEG_G,
  SEG_G, 
  SEG_G,
  SEG_G,
};

//--- end segment



// กำหนดค่า THRESHOLD

#define THRESHOLD_lux_min 8800
#define THRESHOLD_lux_max 20000

#define THRESHOLD_temp_min 23
#define THRESHOLD_temp__max 26

#define THRESHOLD_humid_min 60
#define THRESHOLD_humid__max 80



// include buzzer
int Buzzer = 15;



//servo

#include <ESP32Servo.h>

Servo myservo;  
int pos = 0;    

int servoPin = 18;



// include status ของ green house
String mystatus = "";

// include ambient
#include <BH1750.h>
BH1750 lightMeter(0x23);
float LUX;



/* MQTT Instance */
WiFiClient espClient;
PubSubClient client(espClient);

// ส่วนของการ connect wifi
bool wifiConnected = true;

/* Value Buffer */
char buf[100]; //Reserved for 100 bytes

void setup() {
  Serial.begin(115200); // ตั้งค่าการเชื่อมต่อ Serial
  Wire.begin();  // ตั้งค่าการเชื่อมต่อ Wire
  pinMode(ledPin, OUTPUT); // ตั้งค่าให้ port ledPin เป็น OUTPUT
  pinMode(Buzzer, OUTPUT); // ตั้งค่าให้ port Buzzer เป็น OUTPUT

  myservo.attach(servoPin); 
  
  
  /* (WiFi) Connection Setup */
  WiFi.mode(WIFI_STA);      // set to station mode
  WiFi.begin(ssid, pass);   // เชื่อมต่อ WiFi Access Point โดยใช้ชื่อ (ssid) และรหัสผ่าน (pass)
  delay(5000); //รอ 5 วิ

  /* loop until ESP32 can sucesfully connect to the WiFi */
  Serial.printf("Connecting to %s ", ssid);
  while (WiFi.status() != WL_CONNECTED) { // loop จนกว่า ESP32 จะเชื่อมต่อ WiFi สำเร็จ
      delay(500);
      Serial.print(".");
  }
  /* connection is successful */ 
  Serial.println(" CONNECTED");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());  // แสดง IP Address ที่เชื่อมต่อ

  //ambient
  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {  // เริ่มต้นการทำงานของ BH1750 ว่าสามารถใช้งานได้หรือเปล่า
    Serial.println(F("BH1750 Advanced begin"));
  } else {
    Serial.println(F("Error initialising BH1750"));
  }

  /* MQTT Server */
  client.setServer(mqttServer, mqttPort); // ตั้งค่า MQTT Server และ Port
  client.setCallback(callback); // ตั้งค่า Callback function สำหรับ MQTT client
}

void loop() {

  if (!client.connected()) { //ถ้าหลุดการ connect ให้เชื่อมใหม่
    reconnect();
  }
  client.loop();
  

  // ส่วนของการอ่านค่าจากChipcap และรับค่าจากChipcap
  // ตัวแปร rht เก็บข้อมูลอุณหภูมิและความชื้นจาก Chipcap
  // ตัวแปร humidity และ temperatureC เก็บค่าอุณหภูมิและความชื้นหลังจากที่คำนวณเสร็จสมบูรณ์
  Wire.requestFrom((uint8_t)0x28, (uint8_t)4, (uint8_t) true);
  rht[0] = Wire.read();  
  rht[1] = Wire.read(); 
  rht[2] = Wire.read();  
  rht[3] = Wire.read(); 
  humidity = (((rht[0] & 63) << 8) + rht[1]) / 163.84;
  temperatureC = (((rht[2] << 6) + (rht[3] / 4)) / 99.29) - 40;
    
  // check ว่า humidity และ temperatureC failed หรือเปล่า
  if (isnan(humidity) || isnan(temperatureC)) {
    Serial.println(F("Failed to read from Chipcap sensor!"));
    return;
  }  

  // อ่านค่าแสงจากเซ็นเซอร์วัดแสง Ambient light 
  if (lightMeter.measurementReady()) {
    float lux = lightMeter.readLightLevel();
    LUX = lux;
  }


  /* mqtt-server Transmission */
  //ส่งค่า temperatureC แบบ float ที่ได้ไปเก็บในตัวแปร buf ส่งไปที่topic esp32/temperature
  sprintf(buf, "%.2f", temperatureC); //Make it text in the buffer (buf)
  Serial.println("temp"+ String(buf));
  client.publish("esp32/temperature", buf);

  //ส่งค่า humidity แบบ float ที่ได้ไปเก็บในตัวแปร buf ส่งไปที่topic esp32/temperature
  sprintf(buf, "%.2f", humidity);
  Serial.println("humid"+String(buf));
  client.publish("esp32/humidity", buf);

  //ส่งค่า LUX แบบ float ที่ได้ไปเก็บในตัวแปร buf ส่งไปที่topic esp32/temperature
  sprintf(buf, "%.2f", LUX); //Make it text in the buffer (buf)
  Serial.println("lux"+String(buf));
  client.publish("esp32/lux", buf);



  //ส่วนของการ display ค่า เมื่อมีค่า LUX, temperatureC, humidity มาเปรียบเทียบกับค่า THRESHOLD ของตัวต่างๆที่ได้ตั้งไว้
  // ถ้าค่า lux ไม่ได้อยู่ในrange ที่กำหนด Segmentsจะแสดง คำว่า LUX ออกมา และ จะแสดง mystatus ออกมาที่ serail monitor
  
  
  if (LUX > THRESHOLD_lux_max) {
    display.setBrightness(0x0f);
    display.setSegments(SEG_LUX);
    mystatus = "ค่าความเข้มแสง (lux) มากเกินไป"; //Make it text in the buffer (buf)
    Serial.println(mystatus);

  }
  else if (LUX < THRESHOLD_lux_min) {
    display.setBrightness(0x0f);
    display.setSegments(SEG_LUX);
    mystatus = " ค่าความเข้มแสง (lux) น้อยเกินไป"; //Make it text in the buffer (buf)
    Serial.println(mystatus);

  }

  // ถ้าค่า temperatureC ไม่ได้อยู่ในrange ที่กำหนด Segmentsจะแสดงค่าอุณหภูมิออกมา และ จะแสดง mystatus ออกมาที่ serail monitor
  // และจะมี buzzer แจ้งเตือนด้วย
  else if  (temperatureC >= THRESHOLD_temp__max)  {
    display.setBrightness(0x0f);
    display.showNumberDec(temperatureC, false, 4, 0);
    mystatus =  "ค่าอุณหภูมิ (temperture) มากเกินไป"; //Make it text in the buffer (buf)
    Serial.println(mystatus);
    buzzer_warnning();
    

  }
  else if (temperatureC <= THRESHOLD_temp_min)  {
    display.setBrightness(0x0f);
    display.showNumberDec(temperatureC, false, 4, 0);
    mystatus = "ค่าอุณหภูมิ (temperture) น้อยเกินไป"; //Make it text in the buffer (buf)
    Serial.println(mystatus);
    buzzer_warnning();

  }
  // ถ้าค่า humidity ไม่ได้อยู่ในrange ที่กำหนด Segmentsจะแสดงค่า humid  และ จะแสดง mystatus ออกมาที่ serail monitor 
  // และจะต่างจาก temp ที่ไม่มีเสียง buzzer
   else if  (humidity >= THRESHOLD_humid__max)  {
    display.setBrightness(0x0f);
    display.showNumberDec(humidity, false, 4, 0);
    Serial.println("humidity มากเกินไป ");
    mystatus = "ค่าความชื้น (humidity) มากเกินไป";
    Serial.println(mystatus);

  }
  else if (humidity <= THRESHOLD_humid_min)  {
    display.setBrightness(0x0f);
    display.showNumberDec(humidity, false, 4, 0);
    mystatus = "ค่าความชื้น (humidity) น้อยเกินไป";
    Serial.println(mystatus);
  }
  // ถ้าทุกอย่างปกติ ให้แสเง status ok คือ (----) บน 7-segment
  else  {
    display.setBrightness(0x0f);
    display.setSegments(SEG_OK);
    mystatus = "ทุกอย่างอยู่ในเกณฑ์ปกติ";
    Serial.println(mystatus);

  }


  // sprintf(buf, "%s", mystatus.c_str()); //Make it text in the buffer (buf)
  // client.publish("esp32/status", buf);

  delay(2000); // delay การ display ค่าต่างๆ ของจอ 7 segment 
  display.clear(); // clear จอ 7 segment 


  delay(1000); // delay การอ่านค่าต่างๆ

}


// เป็นการรับ payload ของ topic ที่ได้ subscibe มา คือ esp32/led  
void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();


  // หากได้รับข้อความในหัวข้อ esp32/led ให้ตรวจสอบว่าข้อความนั้น "on" หรือ "off"
  //เปลี่ยนสถานะเอาต์พุตตามข้อความ
  if (String(topic) == "esp32/led") {
    Serial.print("Changing output to ");
    if(messageTemp == "on"){
      Serial.println("on");
      digitalWrite(ledPin, HIGH);
    }
    else if(messageTemp == "off"){
      Serial.println("off");
      digitalWrite(ledPin, LOW);
    }
  }

  if (String(topic) == "esp32/servo") {
    Serial.print("servo ");
    if(messageTemp == "on"){
      Serial.println("on");

      for (pos = 0; pos <= 180; pos += 1) { 
      // in steps of 1 degree
      myservo.write(pos);    
      delay(15);             
    }
    for (pos = 180; pos >= 0; pos -= 1) { 
      myservo.write(pos);    
      delay(15);             
    }
    }
    else if(messageTemp == "off"){
      Serial.println("off");
      myservo.detach();
    }
  }



}

// function ของ buzzer 
void buzzer_warnning(){

  digitalWrite(Buzzer,HIGH);
  delay(500);
  digitalWrite(Buzzer,LOW);
  delay(500);
}

 // เป็น fn ที่ทำการเชื่อมต่อกับ mqtt server ที่ได้ตั้งค่าไว้ หรือถ้าขาดการเชื่อมต่อจะทำการ reconnect อีกครั้ง
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("esp32 Client")) {
      Serial.println("connected");
      // Subscribe
      client.subscribe("esp32/led");
      client.subscribe("esp32/servo");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
