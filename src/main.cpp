#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_task_wdt.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ButtonManager.h>
#include "pin.h"
#include <MUwrapper.hpp>
#include <wiiClassic.h>
#include <controller.h>

TaskHandle_t Main_Handle = NULL;
TaskHandle_t Mu_Handle = NULL;
TaskHandle_t Display_Handle = NULL;

QueueHandle_t controller_TO_mainQueue = NULL;
QueueHandle_t controller1_TO_mainQueue = NULL;
QueueHandle_t config_TO_mainQueue = NULL;
QueueHandle_t main_TO_MuQueue = NULL;



controller::ControllerData controller_main[2];
Packetizer packetizer;

struct QueueData{
  uint8_t Mudata[12];
  uint8_t len;
  int config[5];

};
enum mu_config_items{
  userid,
  groupid,
  deviceid,
  targetid,
  channel ,
  mode
};

struct ConfigData{
  int configdata[5];
};

uint8_t generate_mudata(uint8_t *buf, bool emergency){//最大１２バイト

  if(emergency){  //非常停止aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
    buf[0] = 'E';
    return 1;
  }else{
    packet_t p;
    packetizer.init(p);
    p=controller_main[0].packetize(packetizer);
    memcpy(buf,p.data,5);
    buf[5] = 0x0;
    // packetizer.init(p);
    // p=controller_main[1].packetize(packetizer);
    // memcpy(buf+6,p.data,5);
    // buf[11] = 0x0;

    return 5;
  }
}


//Mu2にシリアルで文字を送る関数Muwrapperのコールバックを受けて実行される
void SendData(MUEvent event, uint8_t *data, uint8_t len){
  if (event == MU_EVENT_ERROR){
    Serial.write("MU_EVENT_ERROR");
  }
  if (event == MU_EVENT_SEND_REQUEST){
      Serial1.write(data,len);
        
  }
}



//↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓タスク↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓

//mainタスク
void main_task(void *pvParameters) {
  Serial.begin(9600);

  ButtonManager btn2;
  btn2.add(Emergency, 15);

  uint8_t Mudata[12];
  
  QueueData queue_send_data;
  ConfigData result_config;
  char inchar;

  int lasttime = 0;
    
  while (1){
    if (millis() - lasttime > 15){
      btn2.update();

      xQueueReceive(controller_TO_mainQueue, &controller_main[0],1);
      xQueueReceive(controller1_TO_mainQueue, &controller_main[1],1);
      xQueueReceive(config_TO_mainQueue, &result_config,1);

      queue_send_data.len = generate_mudata(Mudata, !btn2.isHold(0));
      memcpy(&queue_send_data.config, &result_config.configdata, sizeof(result_config.configdata));
      memcpy(&queue_send_data.Mudata, &Mudata, sizeof(Mudata));
      

      //出力
      xQueueSend(main_TO_MuQueue,&queue_send_data,0);
      lasttime = millis();
    }
    

    

  if (Serial1.available()) {

    inchar = Serial1.read();
    // Serial.printf("%c",inchar);

  }
  vTaskDelay(1);
  }
}





//Mu2のタスク
void Mu(void *pvParameters){
  Serial1.begin(19200,SERIAL_8N1,Mu_TXD,Mu_RXD);

  int lasttime = 0;
  int lastconfig[5];

  MUWrapper mu(SendData);

  mu.init(8);

  QueueData queue_data;

  while (1){

    if (millis() - lasttime > 20){
      
      xQueueReceive(main_TO_MuQueue,&queue_data,0);

      for (int i = 0; i < queue_data.len; i++){
        Serial.printf("%d ",queue_data.Mudata[i]);
      }
      Serial.print("\n");

      if (lastconfig != queue_data.config){
        // mu.setParams(queue_data.config[groupid],queue_data.config[channel],queue_data.config[targetid],queue_data.config[deviceid]);
      }

      //送信
      mu.send(queue_data.Mudata,queue_data.len);
      lasttime = millis();
    }
    
    vTaskDelay(1);

  }

}

//画面タスク
void Display(void *pvParameters){

  #define SCREEN_I2C_ADDR 0x3c // or 0x3C
  #define OLED_RST_PIN -1      // Reset pin (-1 if not available)
  
  bool menu = false;
  int lasttime = 0;
  int wiilasttime = 0;
  int page = 0;

  
  String menu_items[6] = {
    "userid",
    "groupid",
    "deviceid",
    "targetid",
    "channel" ,
    "mode"
  };
  int config_items[6][4] ={
    {1, 2, 3, 4},
    {1, 2, 3, 4},
    {0, 1, 2, 4},
    {0, 1, 2, 4},
    {8, 14, 31, 46},
    {5,12,5,12}
  };

  ConfigData result_config;


  int config[6] ={0,3,0,1,0,0};
  int select_menu_count = 0;
  
  //コントローラー
  controller::ControllerData controllerdata[2] ;
  WiiClassic wii(Wire);
  WiiClassic wii1(Wire1);
  
  Adafruit_SSD1306 display(128, 64, &Wire, OLED_RST_PIN);
  Wire.setPins(OLED_SDA, OLED_SCL); 
  Wire1.setPins(P2_SDA,P2_SCL);
  Wire.begin();
  Wire1.begin();
  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_I2C_ADDR);
  display.setRotation(2);
  

  wii.init();
  wii1.init();

  ButtonManager btn;
  btn.add(SW7,20);
  btn.add(SW6,20);
  btn.add(SW5,20);
  btn.add(SW4,20);

  enum btn_name{
    FRONT_BTNA = 0,//SW7
    FRONT_BTNB = 1,//SW6
    FRONT_BTNC = 2,//SW5
    FRONT_BTND = 3//SW4
  };

  

  while (1){
    btn.update();
    
    if (millis() - lasttime  > 100){
      display.clearDisplay();
      if (menu == false){
        display.setTextSize(2);               //フォントサイズは2(番目に小さい)
        display.setTextColor(SSD1306_WHITE);  //色指定はできないが必要
        display.setCursor(0, 0);            //テキストの表示開始位置
        display.print("CH");         //表示文字列
        display.setCursor(0, 25);
        display.printf("%02x",config_items[channel][config[4]]);

      }else if (menu == true){


        display.setTextSize(1);               //フォントサイズは2(番目に小さい)
        display.setTextColor(SSD1306_WHITE);  //色指定はできないが必要
        display.setCursor(0, 0); 
        display.print("MENU");
        display.drawLine(0,10,128,10,WHITE);

        for (int i =page*5; i < (page+1)*5; i++){

          display.setCursor(2, 10*(i+1)+1);

          if (i == select_menu_count){
            display.fillRect(0, 10*(i+1), 52, 10, WHITE);
            display.setTextColor(INVERSE);  //色指定はできないが必要
            display.print(menu_items[i]);

          }else{
            display.setTextColor(SSD1306_WHITE);
            display.print(menu_items[i]);
            
          }

          display.fillRect(98, 10*(i+1), 14, 10, WHITE);
          display.setTextColor(SSD1306_INVERSE);
          display.setCursor(100, 10*(i+1)+1);
          display.printf("%02x",config_items[i][config[i]]);

          display.drawLine(0,10*(i+2),128,10*(i+2),WHITE);
        }
      }

      

      lasttime = millis();
    }

    if (millis() - wiilasttime  > 15){
      // if (wii.update(controllerdata[0]) == true){
      //   xQueueOverwrite(controller_TO_mainQueue,&controllerdata[0]);
      // }

      // if (wii1.update(controllerdata[1]) == true){
      //   xQueueOverwrite(controller1_TO_mainQueue,&controllerdata[1]);
      // }

      wiilasttime = millis();
    }
    

    if (btn.isPressed(FRONT_BTNA)){
      menu = !menu;
      if (menu == false){

        Serial.printf("UI = %02x\nGI = %02x\nEI = %02x\nDI = %02x\nCH = %02x\nMODE = %02x\n",config_items[userid][config[0]]
        ,config_items[groupid][config[1]],config_items[deviceid][config[2]],config_items[targetid][config[3]],config_items[channel][config[4]],config_items[mode][config[5]]);


        for (int i = 0;i < 5;i++){
          result_config.configdata[i] = config_items[i][config[i]];
        }
        
        xQueueOverwrite(config_TO_mainQueue,&result_config);
      }


    }
    if (menu == true){
      if (btn.isPressed(FRONT_BTNB)){
        select_menu_count++;
        page = select_menu_count/5;
        select_menu_count = select_menu_count % 6;

      }
      if (btn.isPressed(FRONT_BTNC)){
        config[select_menu_count]--;
        if (config[select_menu_count] < 0){
          config[select_menu_count] = 3;
        }

      }
      if (btn.isPressed(FRONT_BTND)){
        config[select_menu_count]++;
        config[select_menu_count] = config[select_menu_count] % 4;
      }
    }

    

    display.display();
    btn.release();
    vTaskDelay(1);

  }
}

void setup() {
  
//Queueを作ってからタスクを召喚する
  controller_TO_mainQueue = xQueueCreate(1,sizeof(controller::ControllerData));
  controller1_TO_mainQueue = xQueueCreate(1,sizeof(controller::ControllerData));
  config_TO_mainQueue = xQueueCreate(1,sizeof(ConfigData));
  main_TO_MuQueue = xQueueCreate(1,sizeof(QueueData));


  xTaskCreateUniversal(main_task,"main", 8192, NULL, 2, &Main_Handle, CONFIG_ARDUINO_RUNNING_CORE);
  xTaskCreateUniversal(Mu,"Mu", 8192, NULL, 2, &Mu_Handle, CONFIG_ARDUINO_RUNNING_CORE);
  xTaskCreateUniversal(Display,"Display", 8192, NULL, 2, &Display_Handle, CONFIG_ARDUINO_RUNNING_CORE);

}

void loop() {
}