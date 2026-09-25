#ifndef BLUETOOTH_KEY_CODEC_H
#define BLUETOOTH_KEY_CODEC_H
#include "NoodoeBluetooth.h"
#define BLUETOOTH_KEY_BYTES 152U
void Bluetooth_EncodeKeys(uint8_t out[BLUETOOTH_KEY_BYTES],const Bluetooth_KeyStore *keys);
uint32_t Bluetooth_DecodeKeys(Bluetooth_KeyStore *keys,const uint8_t *data,uint32_t bytes);
#endif
