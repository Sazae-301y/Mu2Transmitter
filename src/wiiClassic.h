#include "controller.h"
#include <Arduino.h>
#include <Wire.h>

const uint8_t CLASSIC_ADDR = 0x52;

class WiiClassic {
public:
  WiiClassic(TwoWire &wire) : wire_(wire){};
  void init() {
    connect();
    controller::ControllerData c;

    update(c);
  }
  bool update(controller::ControllerData &c) {
    if (millis() - lastUpdate_ < 15) {
      return false;
    }
    wire_.requestFrom(CLASSIC_ADDR, 8);
    uint8_t count = 0;
    memset(buffer_, 0, 6);
    while (wire_.available()) {
      if (count < 6) {
        buffer_[count++] = wire_.read();
      } else {
        wire_.read();
      }
    }
    if ((buffer_[4] == 0 && buffer_[5] == 0) ||
        (buffer_[0] == 0xff && buffer_[1] == 0xff)) {
      err++;
      if (err > 20) {
        // wire_.end();
        connect();
        err = 0;
        c = controller::ControllerData(); // clear
        connected=false;
        return true;
      }
      return false;
    }
    connected = true;
    mapButton(c);
    send(0, 0);
    lastUpdate_ = millis();

    return true;
  }
  bool isConnected() { return connected; }

private:
  controller::ControllerData c;
  TwoWire &wire_;
  bool connected = false;
  uint8_t buffer_[6];
  uint32_t lastUpdate_ = 0;
  uint8_t err = 0;
  void connect() {
    wire_.begin();
    send(0xF0, 0x55);
    send(0xFB, 0x00);
  }
  void send(uint8_t addr, uint8_t data) {

    wire_.beginTransmission(CLASSIC_ADDR);
    wire_.write(addr);
    wire_.write(data);
    wire_.endTransmission();
    delay(5);
  }
  bool b(int i, int j) { return bitRead(buffer_[i], j); }
  void mapButton(controller::ControllerData &c) {
   
    buffer_[4] = ~buffer_[4];
    buffer_[5] = ~buffer_[5];
    c.setButton(controller::X, b(5, 5));
    c.setButton(controller::Y, b(5, 3));
    c.setButton(controller::A, b(5, 6));
    c.setButton(controller::B, b(5, 4));
    c.setButton(controller::UP, b(5, 0));
    c.setButton(controller::DOWN, b(4, 6));
    c.setButton(controller::LEFT, b(5, 1));
    c.setButton(controller::RIGHT, b(4, 7));
    c.setButton(controller::L, b(4, 5));
    c.setButton(controller::R, b(4, 1));
    c.setButton(controller::BACK, b(4, 4));
    c.setButton(controller::START, b(4, 2));
    c.setButton(controller::XBOX, b(4, 3));
    c.setButton(controller::SL, 0);
    c.setButton(controller::SR, 0);
    c.setButton(controller::FLAG_STICK_POLAR, 0);
    c.setAnalog(controller::TriggerL, b(5, 7) * 7);
    c.setAnalog(controller::TriggerR, b(5, 2) * 7);
    c.setAnalog(controller::LstickX, buffer_[0] >> 2 & 0x0f); // 6bit to 4bit
    c.setAnalog(controller::LstickY, buffer_[1] >> 2 & 0x0f); // 6bit to 4bit
    c.setAnalog(controller::RstickX,
                ((buffer_[0] & 0b11000000) >> 4) |
                    ((buffer_[1] & 0b11000000) >> 6)); // 5bit to 4bit
    c.setAnalog(controller::RstickY, buffer_[2] >> 1 & 0x0f);
    if (c.analograw(controller::LstickX) == 7)
      c.setAnalog(controller::LstickX, 8);
    if (c.analograw(controller::LstickY) == 7)
      c.setAnalog(controller::LstickY, 8);
    if (c.analograw(controller::RstickX) == 7)
      c.setAnalog(controller::RstickX, 8);
    if (c.analograw(controller::RstickY) == 7)
      c.setAnalog(controller::RstickY, 8);
  }
};