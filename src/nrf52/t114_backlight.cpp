// Heltec T114: backlight dimming before the display goes off. See t114_backlight.h.

#if defined(BOARD_HELTEC_T114)

#include <Arduino.h>
#include "HardwarePWM.h"

#include "configuration.h"
#include "loop_functions_extern.h"
#include "t114_backlight.h"

#define DIM_TOKEN   0x54424C44u   // owner token of the PWM module ("TBLD")
#define DIM_LEVEL   64            // of 255: 25 % on-time (the pin is active low)
#define DIM_AT_MS   15000         // dim when this much of the off timer is left

static bool g_dim = false;

static void dimOn()
{
    if(g_dim)
        return;

    if(!HwPWM3.takeOwnership(DIM_TOKEN))
        return;   // module busy: no dimming, the display just goes off as before

    HwPWM3.setResolution(8);
    HwPWM3.setClockDiv(PWM_PRESCALER_PRESCALER_DIV_64);   // 16 MHz / 64 / 256 = about 1 kHz
    HwPWM3.addPin(PIN_TFT_LEDA_CTL);
    HwPWM3.writePin(PIN_TFT_LEDA_CTL, DIM_LEVEL, true);   // inverted: the value is the low (= lit) time
    g_dim = true;
}

void t114BacklightRelease(bool restoreFull)
{
    if(!g_dim)
        return;

    HwPWM3.removePin(PIN_TFT_LEDA_CTL);
    HwPWM3.stop();
    HwPWM3.releaseOwnership(DIM_TOKEN);

    pinMode(PIN_TFT_LEDA_CTL, OUTPUT);
    if(restoreFull)
        digitalWrite(PIN_TFT_LEDA_CTL, LOW);   // TFT ON, full brightness

    g_dim = false;
}

void t114BacklightTick()
{
    bool want = false;

    if(DisplayOffWait > 0 && bDisplayOff && !bDisplayIsOff)
    {
        int32_t left = (int32_t)(DisplayOffWait - millis());
        want = (left > 0 && left <= DIM_AT_MS);
    }

    if(want)
        dimOn();
    else if(g_dim)
        t114BacklightRelease(!bDisplayIsOff);
}

#endif
