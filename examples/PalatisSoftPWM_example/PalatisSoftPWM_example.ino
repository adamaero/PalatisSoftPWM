/* Demonstrates usage of the PalatisSoftPWM library.
   Fades Arduino pin 13 from PWM value 0 to the highest level and back to 0 again.
*/

#include <PalatisSoftPWM.h>

SOFTPWM_DEFINE_PINA8_CHANNEL(0);  //Configure Arduino pin A8 as PWM channel 0
SOFTPWM_DEFINE_PINA9_CHANNEL(1);
SOFTPWM_DEFINE_PINA10_CHANNEL(2);
SOFTPWM_DEFINE_PINA11_CHANNEL(3);
SOFTPWM_DEFINE_PINA12_CHANNEL(4);
SOFTPWM_DEFINE_PINA13_CHANNEL(5);
SOFTPWM_DEFINE_PINA14_CHANNEL(6);
SOFTPWM_DEFINE_PINA15_CHANNEL(7);

SOFTPWM_DEFINE_OBJECT(1);        // may need to change DO first!

void setup() {
  PalatisSoftPWM.begin(60);  // begin with 60 Hz PWM frequency
}

void loop() {
   for(int pin = 0; pin < 8; pin++)
        for(int s = 63; s <= 255; s += 64)
          {          // PWM: 255 = 5V
            PalatisSoftPWM.set(pin, s);     // sweeps 1.24, 2.5, 3.75 & 5 V
            delay(500);
          }
}
