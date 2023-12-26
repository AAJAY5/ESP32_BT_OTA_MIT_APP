#include <Arduino.h>
#include "BluetoothSerial.h"
#include <Update.h>

//#define USE_PIN // Uncomment this to use PIN during pairing. The pin is specified on the line below
const char *pin = "1234";  // Change this to more secure PIN.

String device_name = "ESP32-BT-Slave";

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

#if !defined(CONFIG_BT_SPP_ENABLED)
#error Serial Bluetooth not available or not enabled. It is only available for the ESP32 chip.
#endif

BluetoothSerial SerialBT;

void OTAProgress(size_t a, size_t b){
  Serial.printf("OTA Progress: %d, %d\r\n",a,b);
}

void setup() {
  Serial.begin(115200);
  Serial.println("APP_VER: 1.0.0");
  SerialBT.begin(device_name);  //Bluetooth device name
  Serial.printf("The device with name \"%s\" is started.\nNow you can pair it with Bluetooth!\n", device_name.c_str());
//Serial.printf("The device with name \"%s\" and MAC address %s is started.\nNow you can pair it with Bluetooth!\n", device_name.c_str(), SerialBT.getMacString()); // Use this after the MAC method is implemented
#ifdef USE_PIN
  SerialBT.setPin(pin);
  Serial.println("Using PIN");
#endif
  Update.onProgress(OTAProgress);
}

bool cmdFound = false;
bool headFound = false;
uint8_t cmd[300];
uint16_t idx = 0;

uint8_t computeCRC(uint8_t *b, size_t n) {
  uint8_t crc = 0;
  for (size_t i = 0; i < n; i++) {
    crc += b[i];
  }
  return ((~crc) + 1);
}

void printBuffer(uint8_t *buffer, size_t n){
  Serial.println();
  for(size_t i  = 0; i< n; i++){
    Serial.printf("%02X ", buffer[i]);
    if((i+1)%16 == 0){
      Serial.println();
    }
  }
  Serial.println();
}
void processCmdTask() {
  switch (cmd[1]) {
    case 0x01: /* OTA_BEGIN */
      {
        uint32_t updateSize = 0xFFFFFFFF;
        uint8_t type = U_FLASH;
        bool status = false;
        status = Update.begin(updateSize, type);
        Serial.printf("OTA Begin: %u\r\n",status);
        SerialBT.write(status ? 0x55 : 0xAA);
      }
      break;
    case 0x02: /* OTA_WRITE */
      {
        uint8_t *bin = &cmd[4];
        uint8_t len = cmd[3];
        size_t w = Update.write(bin, len);
        // Serial.printf("OTA Wrute: %u\r\n",w==len);
        // printBuffer(bin, len);
        SerialBT.write((w == len) ? 0x55 : 0xAA);
      }
      break;
    case 0x03: /* OTA_END */
      {
        Serial.println("OTA End");
        bool sts = Update.end(true);
        Serial.printf("OTA End: %u\r\n",sts);
        SerialBT.write(sts ? 0x55 : 0xAA);
        if(sts){
          Serial.println("Applying Updates...");
          delay(1000);
          ESP.restart();
        }
      }
      break;
    default:
      Serial.println("OTA Unknown Command");
      SerialBT.write(0xAA);
      break;
  }
}

void parseCmdTask() {
  if (!SerialBT.available())
    return;

  int d = SerialBT.read();

  if (!headFound) {
    if (d == 0x55) {
      idx = 0;
      cmd[idx++] = 0x55;
      headFound = true;
      return;
    }
  }

  if (!headFound) return;

  cmd[idx++] = d;

  if (!cmdFound && idx >= 3) {
    if (0xFF != (uint8_t)(cmd[1] + cmd[2])) {
      headFound = false;
      cmdFound = false;
      idx = 0;
    } else {
      cmdFound = true;
    }
  }

  if (idx > 3) {
    if (idx >= (4 + cmd[3] + 1)) {
      // proces
      if (computeCRC(cmd, idx - 1) == cmd[idx - 1]) {
        processCmdTask();
        // Serial.println("CMD Received");
        // for (int i = 0; i < idx; i++) {
        //   Serial.printf("%02X ", cmd[i]);
        //   if ((i + 1) % 16 == 0) Serial.println();
        // }
        // Serial.println();
      } else {
        Serial.println("CMD Invalid CRC");
      }
      cmdFound = false;
      headFound = false;
      idx = 0;
    }
  }
}

void loop() {
  parseCmdTask();
}
