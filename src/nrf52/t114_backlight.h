// Heltec T114: dim the TFT backlight in the second half of the display off timer (display stored off),
// so the user sees that the display is about to go dark. The backlight pin is active low; for the
// dimmed state it is driven by a PWM module of the nRF52, in every other state it is a plain GPIO
// (all digitalWrite() calls on it stay valid). Whole file is compiled for BOARD_HELTEC_T114 only.
#pragma once

#if defined(BOARD_HELTEC_T114)

void t114BacklightTick();                    // once per main loop pass
void t114BacklightRelease(bool restoreFull); // PWM off, pin back to GPIO; full brightness if restoreFull,
                                             // else the pin keeps the level the caller wrote last

#endif
