#pragma once
#include <M5Unified.h>

// Drop-in shims for a few M5StickCPlus APIs that M5Unified renamed or
// removed. Used to keep the Plus2-era call sites (M5Axp.ScreenBreath,
// M5Beep.tone, ...) working on hardware where the underlying chip is
// different. On StickS3 there is no AXP192 — battery current, VBUS
// voltage, and chip temperature have no hardware source, so those shims
// return zeros.

struct M5AxpCompat {
  void  ScreenBreath(int v)   { M5.Display.setBrightness(v * 255 / 100); }
  void  PowerOff()            { M5.Power.powerOff(); }
  int   GetBtnPress()         { return M5.BtnPWR.wasPressed() ? 0x02 : 0x00; }
  void  SetLDO2(bool on)      { on ? M5.Display.wakeup() : M5.Display.sleep(); }
  float GetBatVoltage()       { return M5.Power.getBatteryVoltage() / 1000.0f; }
  float GetBatCurrent()       { return 0.0f; }
  float GetVBusVoltage()      { return M5.Power.isCharging() ? 5.0f : 0.0f; }
  float GetTempInAXP192()     { return 0.0f; }
};

struct M5BeepCompat {
  void begin()                              {}
  void tone(uint16_t freq, uint32_t dur)    { M5.Speaker.tone(freq, dur); }
  void update()                             {}
};

extern M5AxpCompat  M5Axp;
extern M5BeepCompat M5Beep;
