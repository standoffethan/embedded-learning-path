//Constants
#define STANDBY_DELAY 3 //Range 1-4 seconds
#define PRESCALER 1024 //TCCR1B & TCCR2B register configs
#define TICK_RATE (F_CPU / PRESCALER)
#define BAUD 9600
#define UBRR_VALUE ((F_CPU / 16 / BAUD) - 1)

// Timer1 Config (for timeout)
#define TIMER1_START (65536UL - ((F_CPU / PRESCALER) * STANDBY_DELAY))

#if TIMER1_START > 65536UL
#error "STANDBY_DELAY exceeds 16-bit uint limit. Must be within uint16_t TCNT1 register."
#endif

//Timer2 Config (for debounce)
#define TIMER2_LIMIT 255 //OCR2A Value & Timer1 8-bit limit
#define TIMER2_DURATION_US (((TIMER2_LIMIT + 1UL) * 1000000UL) / TICK_RATE) //Timer2 duration in microseconds

//Debounce Config
#define DEBOUNCE_MS 50
#define DEBOUNCE_US (DEBOUNCE_MS * 1000UL) //miliseconds -> microseconds
#define DEBOUNCE_COUNT ((DEBOUNCE_US + TIMER2_DURATION_US - 1) / TIMER2_DURATION_US)

#if DEBOUNCE_COUNT > 255
#error "DEBOUNCE_COUNT exceeds 8-bit uint limit. Would overflow uint8_t debounce_counter in the scope of INT0 ISR."
#elif DEBOUNCE_COUNT == 0
#error "DEBOUNCE_COUNT is zero.
#endif


//Global Variables
typedef enum { OFF, ON, STANDBY } State;

volatile uint8_t timer2_fire_count = 0;
volatile State current_state = OFF;
volatile bool button_event = false,
              timeout_event = false;
volatile uint8_t debounce_counter = 0;


void atomic_set_timer(void){
  uint8_t status = SREG;
  cli();
  
  //Stops timer.
  TCCR1B = 0x00;
  //Enables overflow interrupt
  TIMSK1 = (1 << TOIE1);
  //Sets Timer1 to 0.
  TCNT1 = TIMER1_START;
  //Starts timer with 1024 Prescaler.
  TCCR1B |= (1 << CS12) | (1 << CS10);
  
  SREG = status;
}


void enter_on_state(void){
  PORTD |= (1 << PD5);
  PORTD &= ~(1 << PD6);
  current_state = ON;
  atomic_set_timer();
}


void setup() {
  // Explicitly set DDRD = 0110'0000
  // PD5&6 as OUTPUT
  DDRD = (1 << PD5) | (1 << PD6);

  //Turn internal pullup resistor on PD2
  PORTD = (1 << PD2);

  //EICRA ISC01 Sets INT0 to trigger on falling edge.
  //EIMSK enables INT0.
  EICRA = (1 << ISC01);
  EIMSK = (1 << INT0);
  sei();

  TCCR1B = 0x00;
  TCCR2B = 0x00;
  TCCR1A = 0x00;
  TCCR2A = 0x00;
  OCR2A = 255; //Max time, ~16ms

  //Timer interrupt enabler
  TIMSK1 = 0x00;
  TIMSK2 = 0x00;

  //Timer1 Interrupt Flag Register
  TCNT1 = 0x00;
  TCNT2 = 0x00;
  TIFR1 = (1 << TOV1);
  TIFR2 = (1 << OCF2A);

  //Set Timer2 as off
  TCCR2A = (1 << WGM21); //Timer2 CTC mode
  TIMSK2 &= ~(1 << OCIE2A);
}


ISR(INT0_vect){
  if(debounce_counter == 0) {  

    //Clearing interrupt flags
    TIFR2 = (1 << OCF2A);

    //Enable interrupt
    TIMSK2 |= (1 << OCIE2A);

    //Configure timer2 for debounce
    TCNT2 = 0;
    TCCR2B |= (1 << CS22) | (1 << CS21) | (1 << CS20);

    button_event = true;
    debounce_counter = DEBOUNCE_COUNT;
  }
}


ISR(TIMER1_OVF_vect){
  timeout_event = true;
}

ISR(TIMER2_COMPA_vect){
  if(--debounce_counter == 0){
    TIMSK2 &= ~(1 << OCIE2A); //Disable timer2 comparison ISR
    TCCR2B = 0x00; //Disables timer2
  }
}


void loop() {
  // Atomically read and clear button event to prevent race conditions during polling in main.
  uint8_t sreg = SREG;
  cli();
  bool btn_evt = button_event;
  bool tmo_evt = timeout_event;
  button_event = false;
  timeout_event = false;
  SREG = sreg;

  // states: OFF / STANDBY / ON
  if(btn_evt){
    switch (current_state) {
      case STANDBY:
      case OFF:
        enter_on_state();
        break;
      case ON:
        //Stops timer.
        TCCR1B = 0x00;
        //Turns PD5 and overflow interrupts off.
        TIMSK1 &= ~(1 << TOIE1);
        PORTD  &= ~(1 << PD5);
        current_state = OFF;
        break;
    }
    btn_evt = false;
  }

  if(tmo_evt){
    //Stops timer.
    TCCR1B = 0x00;
    TIMSK1 &= ~(1 << TOIE1);
    PORTD &= ~(1 << PD5);
    PORTD |= (1 << PD6);
    
    current_state = STANDBY;
    tmo_evt = false;
  }
}
