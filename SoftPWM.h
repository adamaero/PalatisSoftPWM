// arduino-softpwm software PWM library for Arduino: https://github.com/per1234/arduino-softpwm
#ifndef SoftPWM_h
#define SoftPWM_h

#include <Arduino.h>
// uncomment the following line to add a 1 PWM clock cycle delay between outputs to prevent large in-rush currents
//#define SOFTPWM_OUTPUT_DELAY

// helper macros
#define SOFTPWM_DEFINE_PINMODE(CHANNEL, PMODE, PORT, BIT) \
  template <> \
  inline void pinModeStatic<CHANNEL>(uint8_t const mode) { \
    if (mode == INPUT) { \
      bitClear(PMODE, BIT); bitClear(PORT, BIT); \
    } \
    else if (mode == INPUT_PULLUP) { \
      bitClear(PMODE, BIT); bitSet(PORT, BIT); \
    } \
    else { \
      bitSet(PMODE, BIT); \
    } \
  }

#define SOFTPWM_DEFINE_CHANNEL(CHANNEL, PMODE, PORT, BIT) \
  template <> \
  inline void bitWriteStatic<CHANNEL>(bool const value) { \
    bitWrite( PORT, BIT, value ); \
  } \
  SOFTPWM_DEFINE_PINMODE( CHANNEL, PMODE, PORT, BIT )

#define SOFTPWM_DEFINE_CHANNEL_INVERT( CHANNEL, PMODE, PORT, BIT ) \
  template <> \
  inline void bitWriteStatic<CHANNEL>(bool const value) { \
    bitWrite(PORT, BIT, !value); \
  } \
  SOFTPWM_DEFINE_PINMODE(CHANNEL, PMODE, PORT, BIT)

#if defined(__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
#define SOFTPWM_DEFINE_OBJECT_WITH_BRIGHTNESS_LEVELS(CHANNEL_CNT, BRIGHTNESS_LEVELS) \
  CSoftPWM<CHANNEL_CNT, BRIGHTNESS_LEVELS> SoftPWM; \
  ISR(TIM1_COMPA_vect) { \
    interrupts(); \
    SoftPWM.update(); \
  }
#else  //defined(__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
#define SOFTPWM_DEFINE_OBJECT_WITH_BRIGHTNESS_LEVELS(CHANNEL_CNT, BRIGHTNESS_LEVELS) \
  CSoftPWM<CHANNEL_CNT, BRIGHTNESS_LEVELS> SoftPWM; \
  ISR(TIMER1_COMPA_vect) { \
    interrupts(); SoftPWM.update(); \
  }
#endif  //defined(__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)

#define SOFTPWM_DEFINE_OBJECT(CHANNEL_CNT) \
  SOFTPWM_DEFINE_OBJECT_WITH_BRIGHTNESS_LEVELS(CHANNEL_CNT, 0)

#define SOFTPWM_DEFINE_EXTERN_OBJECT_WITH_BRIGHTNESS_LEVELS(CHANNEL_CNT, BRIGHTNESS_LEVELS) \
  extern CSoftPWM<CHANNEL_CNT, BRIGHTNESS_LEVELS> SoftPWM;

#define SOFTPWM_DEFINE_EXTERN_OBJECT(CHANNEL_CNT) \
  SOFTPWM_DEFINE_EXTERN_OBJECT_WITH_BRIGHTNESS_LEVELS(CHANNEL_CNT, 0)

// here comes the magic @o@
template <int channel> inline void bitWriteStatic(bool value) {}
template <int channel> inline void pinModeStatic(uint8_t mode) {}

template <int channel>
struct bitWriteStaticExpander {
  void operator() (bool value) const {
    bitWriteStatic<channel>(value);
    bitWriteStaticExpander < channel - 1 > ()(value);
  }

  void operator() (uint8_t const &count, uint8_t const * const &channels) const {
#ifdef SOFTPWM_OUTPUT_DELAY
    bitWriteStatic<channel>((count + channel) < channels[channel]);
#else  //SOFTPWM_OUTPUT_DELAY
    bitWriteStatic<channel>(count < channels[channel]);
#endif  //SOFTPWM_OUTPUT_DELAY
    bitWriteStaticExpander < channel - 1 > ()(count, channels);
  }
};

template <>
struct bitWriteStaticExpander < -1 > {
  void operator() (bool) const {}
  void operator() (uint8_t const &, uint8_t const * const &) const {}
};

template <int channel>
struct pinModeStaticExpander {
  void operator() (uint8_t const mode) const
  {
    pinModeStatic<channel>(mode);
    pinModeStaticExpander < channel - 1 > ()(mode);
  }
};

template <>
struct pinModeStaticExpander < -1 > {
  void operator() (uint8_t const mode) const {}
};

template <int num_channels, int num_brightness_levels>
class CSoftPWM {
  public:
    void begin(long hertz) {
      asm volatile ("/************ pinModeStaticExpander begin ************/");
      uint8_t oldSREG = SREG;
      noInterrupts();
      pinModeStaticExpander < num_channels - 1 > ()( OUTPUT );
      SREG = oldSREG;
      asm volatile ("/************ pinModeStaticExpander end ************/");

      /* the setup of timer1 is stolen from ShiftPWM :-P
         http://www.elcojacobs.com/shiftpwm/ */
      asm volatile ("/************ timer setup begin ************/");
      TCCR1A = 0b00000000;
      TCCR1B = 0b00001001;
      OCR1A = (F_CPU - hertz * brightnessLevels() / 2) / (hertz * brightnessLevels());
      bitSet(TIMSK1, OCIE1A);
      asm volatile ("/************ timer setup end ************/");

      _count = 0;
    }

    void set(int channel_idx, uint8_t value) {
      _channels[channel_idx] = value;
    }

    size_t size() const {
      return num_channels;
    }

    unsigned int brightnessLevels() const {
      return num_brightness_levels ? num_brightness_levels : 256;
    }

    void allOff() {
      asm volatile ("/********** CSoftPWM::allOff() begin **********/");
      uint8_t oldSREG = SREG;
      noInterrupts();
      for (int i = 0; i < num_channels; ++i)
        _channels[i] = 0;
      bitWriteStaticExpander < num_channels - 1 > ()(false);
      SREG = oldSREG;
      asm volatile ("/********** CSoftPWM::allOff() end **********/");
    }

    /* This function cannot be private because the ISR uses it, and I have
       no idea about how to make friends with ISR. :-( */
    void update() __attribute__((always_inline)) {
      asm volatile ("/********** CSoftPWM::update() begin **********/");
      uint8_t count = _count;
      bitWriteStaticExpander < num_channels - 1 > ()(count, _channels);
      ++_count;
      if (_count == brightnessLevels())
        _count = 0;
      asm volatile ("/********** CSoftPWM::update() end **********/");
    }

    /* this function is stolen from ShiftPWM :-P
       http://www.elcojacobs.com/shiftpwm/ */
    void printInterruptLoad() {
      unsigned long time1, time2;

      bitSet(TIMSK1, OCIE1A); // enable interrupt
      time1 = micros();
      delayMicroseconds(5000);
      time1 = micros() - time1;

      bitClear(TIMSK1, OCIE1A); // disable interrupt
      time2 = micros();
      delayMicroseconds(5000);
      time2 = micros() - time2;

      double load = static_cast<double>(time1 - time2) / time1;
      double interrupt_frequency = static_cast<double>(F_CPU) / (OCR1A + 1);
      double cycles_per_interrupt = load * F_CPU / interrupt_frequency;

      Serial.println(F("SoftPWM::printInterruptLoad():"));
      Serial.print(F("  Load of interrupt: "));
      Serial.println(load, 10);
      Serial.print(F("  Clock cycles per interrupt: "));
      Serial.println( cycles_per_interrupt );
      Serial.print(F("  Interrupt frequency: "));
      Serial.print(interrupt_frequency);
      Serial.println(F(" Hz"));
      Serial.print(F("  PWM frequency: "));
      Serial.print(interrupt_frequency / brightnessLevels());
      Serial.println(F(" Hz"));
      Serial.print(F("  Brightness levels: "));
      Serial.println(brightnessLevels());

      bitSet(TIMSK1, OCIE1A);  // enable interrupt again
    }

  private:
    static void _timerCallback();

    uint8_t _channels[num_channels];
    uint8_t _count;
};

#endif  //SoftPWM_h
