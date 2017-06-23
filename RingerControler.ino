#include <SoftwareSerial.h>

#define PIN_TX                          -1     // Disables transmision serial data
#define PIN_RX                          0      // PB0
#define PIN_BELL_A                      3      // PB4
#define PIN_BELL_B                      4      // PB5
#define PIN_VOLTSUPPLY                  1      // PB1
#define PIN_RING                        2      // PB2

#define MAX_DURATION                    60000  // Max duration of ringing (ms) to keep people from demolishing the phone
#define PULSE_WIDTH                     15     // Duration of single pulse to a bell (in ms)

#define RINGER_DELAY                    250    // ms

volatile boolean ringerActive         = false;
volatile boolean isRinging            = false;

volatile boolean timerStarted         = false;
volatile boolean doStartRinger        = false;
volatile boolean doStopRinger         = false;

volatile unsigned short ticks         = 0;
volatile unsigned long  mSec          = 0;

// Assemble ring signal
volatile unsigned short signalLength  = 4000;  // Length of sequence of rings + pause = 4000 ticks = 4 s 
         unsigned short signalLength1 = 4000;  // Length of sequence of rings + pause = 4000 ticks = 4 s 
         unsigned short signalLength2 = 1000;  // Length of sequence of rings + pause = 1000 ticks = 1 s 
volatile unsigned short period        = 40;    // period cycle in ms. 40 ms ~ 25 Hz
         unsigned short period1       = 40;    // period cycle in ms. 40 ms ~ 25 Hz
         unsigned short period2       = 1000;  // period cycle in ms. One "ding dong" takes 1 s
volatile unsigned short nPeriod       = 50;    // Nr of periods = 50 * 40 = 2000 ms
         unsigned short nPeriod1      = 50;    // Nr of periods = 50 * 40 = 2000 ms
         unsigned short nPeriod2      = 2;     // Nr of periods

volatile boolean repeat               = false; // True = repeat signal after duration
volatile boolean inRingSeq            = false;

boolean dingDong                      = false; // Indicatie ringtone is "dingdong" or "tring"

char inputString[3]                   = "";    // a string to hold incoming data
boolean stringComplete                = false; // whether the string is complete
boolean inputAvailable                = false; //

boolean ringPinHigh                   = false;
boolean prevRingPinHigh               = false;

SoftwareSerial TinySerial(PIN_RX, PIN_TX); // RX, TX

void setup()
{
  // set the data rate for the SoftwareSerial port
  TinySerial.begin(2400);

  pinMode(PIN_BELL_A, OUTPUT);
  pinMode(PIN_BELL_B, OUTPUT);
  pinMode(PIN_VOLTSUPPLY, OUTPUT);

  pinMode(PIN_RING, INPUT);

  // Set Timer to interrupt every 1 ms
  TCCR0A = (1 << WGM01);             // Clear Timer on Compare (CTC) mode
  TCCR0B = (1 << CS01);              // div8
  OCR0A  = F_CPU/8 * 0.001 - 1;      // 1000us compare value
  TIMSK |= (1<<OCIE0A);              // if you want interrupt
}


void loop ()
{

  // Check Ringer pin
  ringPinHigh = (digitalRead(PIN_RING) == HIGH);
  if (ringPinHigh != prevRingPinHigh)
  { // Reading changed since last loop cycle
    if (ringPinHigh)
    {
      doStartRinger = true;
      setTring();
    }
    else
    {
      doStopRinger  = true;
      if (doStartRinger)
      {
        setDingDong();
      }
    }
  }
  prevRingPinHigh = ringPinHigh;

  if (inputAvailable)
    doStopRinger  = true; // Always stop ringer in case of any input

  if (stringComplete)
  {
    // Process request

    if (strcmp(inputString, "S1") == 0) // SET SIGNAL DINGDONG
    {
      setDingDong();
    }
    else if (strcmp(inputString, "S2") == 0) // SET SIGNAL TRING
    {
      setTring();
    }

    stringComplete = false;
    inputAvailable = false;
    strcpy(inputString, "");

  } // End if

  inputEvent();

} // End loop ()

void setTring()
{
  dingDong      = false;
  repeat        = true;
  signalLength  = signalLength1;
  period        = period1;
  nPeriod       = nPeriod1;
}

void setDingDong()
{
  dingDong      = true;
  repeat        = false;
  signalLength  = signalLength2;
  period        = period2;
  nPeriod       = nPeriod2;
}

/*
  SerialEvent occurs whenever a new data comes in the
 hardware serial RX.  This routine is run between each
 time loop() runs, so using delay inside loop can delay
 response.  Multiple bytes of data may be available.
 */
void inputEvent()
{
  while (TinySerial.available())
  {
    inputAvailable = true;

    // get the new byte:
    char inChar[1];
    inChar[0] = (char)TinySerial.read(); 

    // add it to the inputString:
    // if the incoming character is a newline, set a flag
    // so the main loop can do something about it:
    if (inChar[0] == '\n' || strlen(inputString) > 1)
    {
      stringComplete = true;
    }
    else
    {
      strcat(inputString, inChar);
    }
  }
}


// Interrupt is called once a millisecond,
ISR(TIMER0_COMPA_vect)
{

  mSec++;

  if (doStartRinger)
  {
    if(!timerStarted)
    {
      mSec = 0;
      timerStarted = true;
    }
    if (mSec == RINGER_DELAY)
    {
      ringerActive  = true;
      doStartRinger = false;
    }
  }
 
  if (doStopRinger && (!dingDong || mSec >= RINGER_DELAY + signalLength))
  {
    ringerActive = false;
    doStopRinger = false;

    timerStarted = false;
  }

  if (ringerActive)
  {

    if (repeat || !isRinging)
    {
      if (!isRinging)
      {
        isRinging = true;
        ticks     = 0;
      }

      if (ticks == 0)
      {
        digitalWrite(PIN_VOLTSUPPLY,HIGH); // Start voltage supply
        inRingSeq = true;
      }
      else if (ticks == nPeriod * period)
      {
        digitalWrite(PIN_VOLTSUPPLY,LOW); // Stop voltage supply
        inRingSeq = false;
      }
      else if (ticks == signalLength -1)
      {
        if (mSec > MAX_DURATION + RINGER_DELAY)
        { // Max duration reached, stop ringing in next cycle
          repeat = false;
        }
      }

      if (inRingSeq)
      { // within sequence of ring cycles

        unsigned int cycleTicks = ticks % period;
        if (cycleTicks == 0)
          digitalWrite(PIN_BELL_A,HIGH);// Bell A on
        else if (cycleTicks == PULSE_WIDTH)
          digitalWrite(PIN_BELL_A,LOW);// Bell A off
        else if (cycleTicks == period / 2)
          digitalWrite(PIN_BELL_B,HIGH);// Bell B on
        else if (cycleTicks == PULSE_WIDTH + period / 2)
          digitalWrite(PIN_BELL_B,LOW);// Bell B off

      }

      ticks = ++ticks % signalLength;

    }
    else
    {
      // Stop ringer
      ringerActive = false;
    }

  } // if (ringerActive)
  else if (isRinging)
  {
    digitalWrite(PIN_BELL_A,LOW);     // Bell A off
    digitalWrite(PIN_BELL_B,LOW);     // Bell B off
    digitalWrite(PIN_VOLTSUPPLY,LOW); // Stop voltage supply

    isRinging = false;
  }

}
