
#include "ACDimmer.h"

const uint8_t interruptPin = 8;
static uint8_t outPin = 6;

static uint8_t dim;// Dimming level (0-100)  100 = on, 0 = 0ff
volatile static uint8_t critical, timeout, rising, edge_detect, si;

#define NUM_SAMPLES 5

static uint16_t lowpulse, highpulse;
static uint8_t l[NUM_SAMPLES], h[NUM_SAMPLES];
static uint8_t csec;
static uint32_t sec; // Clock from 50Hz

static void acdimmer_setup();
static void acdimmer_clearSync() ;
static void acdimmer_timer2_init();
static void acdimmer_timer2_start(uint8_t timeout, uint8_t timeout2);

static void acdimmer_timer1_init();
static void acdimmer_clearSync();
static uint16_t acdimmer_calcAverage(uint8_t *l);
static void acdimmer_calcTimer();

static uint8_t acdimmer_poll();

#define MAX_PULSE_LENGTH 250

/*
      Timer 2 is used for starting and ending the command pulse
*/

ISR(TIMER2_COMPB_vect) {
  digitalWrite(outPin, HIGH);
}

ISR(TIMER2_COMPA_vect)
{
  digitalWrite(outPin, LOW);
  TCCR2B = 0; // stop timer
}

static void acdimmer_timer2_init()
{
  TCCR2A = 0;
  TCCR2B = 0;
  TCNT2 = 0;

  OCR2A = 200; // FIXME
  OCR2B = 100;
  TCCR2A = _BV(WGM21);  // Set to CTC Mode
  TCCR2B = _BV(CS22) | _BV(CS21) | _BV(CS20);  // set prescaler to 1024
  TIMSK2 = _BV(OCIE2A) | _BV(OCIE2B);  //Set interrupt on compare match
  TIFR2 |= _BV(OCF2A) | _BV(OCF2B);
}

static void acdimmer_timer2_start(uint8_t timeout, uint8_t timeout2) {
  TCNT2 = 0;
  OCR2A = timeout2;
  OCR2B = timeout;
  TCCR2B = _BV(CS22) | _BV(CS21) | _BV(CS20);  // set prescaler to 1024
}


/*
   Timer 1 is used for measuring pulse widths
*/

/* Zero cross detect

      /\        /\
     /  \      /  \
   -/----\----/----\
   /      \  /      \
           \/
    __   __   __
    ||   ||   ||
   _||___||___||

   lowpulse = 135
   highpulse = 21
*/

ISR(TIMER1_CAPT_vect) {
  volatile uint16_t capture = ICR1;
  TCNT1 = 0;
  if (rising) {
    if (dim != DIM_FORCE_ON) {
      digitalWrite(outPin, LOW);
      if (timeout > 0) {
        acdimmer_timer2_start(timeout, timeout + 10);
      }
    }
  }
  if (capture > MAX_PULSE_LENGTH) {
    capture = 0;
  }
  if (rising) {
    if (!critical) {
      // We may miss an edge, but there are plenty of them
      l[si] = capture;
    }
    TCCR1B &= ~(1 << ICES1); // next falling edge
  } else { // falling
    if (!critical) {
      h[si] = capture;
    }
    TCCR1B |= (1 << ICES1); // next rising edge
  }
  rising = !rising;
  edge_detect = 1;

  if (rising) {
    csec++;
    if (csec == 100) { // TODO consider 60Hz
      csec = 0;
      sec++;
    }
    si = (si + 1) % NUM_SAMPLES;
#ifndef TOO_LONG_INTERRUPT_METHOD
    acdimmer_calcTimer();
#endif
  }
}


ISR(TIMER1_OVF_vect)
{
  // Overflow. Zero all detected pulse lengths
  //  TIFR1 |= 0x01;
  acdimmer_clearSync();
}

static void acdimmer_timer1_init()
{
  rising = 1;
  TCCR1A = 0;
  TCNT1 = 0;                       //SETTING INTIAL TIMER VALUE
  TCCR1B = 0;
  TCCR1B |= (1 << ICES1);          //SETTING FIRST CAPTURE ON RISING EDGE ,(TCCR1B = TCCR1B | (1<<ICES1)
  TCCR1B |= (1 << ICNC1); //Enable input capture noise canceller
  TIMSK1 |= (1 << ICIE1) | (1 << TOIE1); //ENABLING INPUT CAPTURE AND OVERFLOW INTERRUPTS
  // TCCR1B |= (1 << CS10);          //STARTING TIMER WITH PRESCALER 1
  // TCCR1B|=(1<<CS11)|(1<<CS10);    //STARTING TIMER WITH PRESCALER 64
  // TCCR1B|=(1<<CS12);              //STARTING TIMER WITH PRESCALER 256
  TCCR1B |= (1 << CS12) | (1 << CS10); //STARTING TIMER WITH PRESCALER 1024
}


static void acdimmer_clearSync() {
  for (uint8_t i = 0; i < NUM_SAMPLES; i++) {
    l[i] = h[i] = 0;
  }
  timeout = highpulse = lowpulse = 0;
}


static uint16_t acdimmer_calcAverage(uint8_t *l) {
  uint16_t sum = 0;
  for (uint8_t i = 0 ; i < NUM_SAMPLES; i++) {
    sum += l[i];
  }

  uint16_t avg = sum / NUM_SAMPLES;
  uint16_t tol = avg / 10;
  for (uint8_t i = 0; i < NUM_SAMPLES; i++) {
    if (l[i] < avg - tol || l[i] > avg + tol) {
      // no good
      avg = 0;
      break;
    }
  }
  return avg;
}

static void acdimmer_calcTimer() {
  highpulse = acdimmer_calcAverage(h);
  lowpulse = acdimmer_calcAverage(l);

  if (dim == 0 || lowpulse == 0 || highpulse == 0 || lowpulse + highpulse > MAX_PULSE_LENGTH) {
    // Switch off
    timeout = 0;
    return;
  }

  uint16_t halfhigh = highpulse / 2;
  if (dim >= DIM_MAX) {
    // Full phase, start immediately
    //timeout = 0;
    timeout = halfhigh;
    return;
  }
  // We have an average pulse length

  uint16_t timeout2 = halfhigh + (DIM_MAX - dim) * (lowpulse + halfhigh - 10) / DIM_MAX;

  if (timeout2 >= lowpulse) { // There might be room for a slightly more dimming..
    // if timeout is bigger, output is disabled
    timeout = 0;
    return;
  }
  timeout = timeout2;
}


uint8_t acdimmer_poll() {
  critical = 1;
  uint8_t tmp = edge_detect;
  if (edge_detect) {
    edge_detect = 0;
    acdimmer_calcTimer();
  }
  critical = 0;
  return tmp;
}

static void acdimmer_setup() {
  lowpulse = highpulse = 0;
  si = edge_detect = 0;

  acdimmer_timer2_init();

  pinMode(outPin, OUTPUT);
  digitalWrite(outPin, LOW);
  pinMode(interruptPin, INPUT_PULLUP);

  acdimmer_timer1_init();
}

// class ACDimmer

ACDimmer::ACDimmer(uint8_t cmdPin) {
  outPin = cmdPin;
  dim = DIM_OFF;
}

void ACDimmer::begin() {
  acdimmer_setup();
}

uint8_t ACDimmer::poll() {
  return acdimmer_poll();
}

void ACDimmer::setDimming(uint8_t level) {
  dim = level;
}

void ACDimmer::setOn() {
  dim = DIM_FORCE_ON;
  digitalWrite(outPin, HIGH);
}

void ACDimmer::setOff() {
  dim = DIM_OFF;
  digitalWrite(outPin, LOW);
}

uint8_t ACDimmer::isEdgeDetected() {
  uint8_t tmp = edge_detect;
  // race here, who cares
  edge_detect = 0;
  return tmp;
}

uint32_t ACDimmer::getSec() {
  return sec;
}

uint8_t ACDimmer::getPulseWidth() {
  return lowpulse + highpulse;
}
