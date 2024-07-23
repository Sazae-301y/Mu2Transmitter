#include <Arduino.h>

class ButtonManager {

    public:
    ButtonManager(){
        
    }

    void add(int pin, int debouncetime){
        btn[count].pin = pin;
        btn[count].debouncetime = debouncetime;
        pinMode(pin,INPUT_PULLUP);
        count++;
    }

    void update(){
        for (int i = 0;  i < count; i++){ //ボタン個数分ループ
            if (millis() - btn[i].lasttime  > btn[i].debouncetime){   //チャタリング防止


                bool status = !digitalRead(btn[i].pin);


                if (status &&! btn[i].laststatus){
                    btn[i].rising = true;
                    btn[i].falling = false;
                    btn[i].lasttime = millis();

                }else if( !status && btn[i].laststatus){
                    btn[i].falling = true;
                    btn[i].rising = false;
                    btn[i].lasttime = millis();

                }


                btn[i].laststatus = status;
            }
        }
    }

    bool isPressed(int pin){
        return btn[pin].rising;
    }

    bool isReleased(int pin){
        return btn[pin].falling;
    }
    bool isHold(int pin){
        return btn[pin].laststatus;
    }



    void release(){
       for (int i = 0;  i < count; i++){ //ボタン個数分ループ

            btn[i].falling = false;
            btn[i].rising = false;
            
        }
    }

    private:
    

    int count = 0;
    struct Button {
        int pin = 0;
        int debouncetime = 0;
        uint32_t lasttime = 0;
        bool laststatus = false;
        bool rising = false;
        bool falling = false;

    };
    Button btn[6] = {};

    
};