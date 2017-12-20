#include <Bounce2.h>

#include <Servo.h>

#define STATE_STANDBY 1
#define STATE_COMPUTER_STARTING 2
#define STATE_HW_STARTING 3
#define STATE_RUNNING 4
#define STATE_COMPUTER_STOPPRING 5

#define EVENT_NOTHING 1
#define EVENT_AUDIO_ON 2
#define EVENT_AUDIO_OFF 3

//Controls servo to start Mac
const int servoPin = 23;

//Single momentory push-button for on and off
const int offSwitchPin = 2;
const int onSwitchPin = 1;

//Uses USB bus power to detect when Mac has actually started and shutdown
//Logic is inverted by opto-isolator
const int USBBusPowerPin = 5;

const int redLedPin = 21;
const int greenLedPin = 22;

//Controls audio - probably via a contactor or relay
const int audioPowerPin = 16;

//Shuts down Mac by sending a MIDI Program Change 127 on Channel 16.
const int midiChannel = 16;
const int midiShutdownPC = 127;
//Receieves MIDI PC to indicate if HW has started and stopped
const int midiAudioOnPC = 255;
const int midiAudioOffPC = 254;

Bounce onSwitch = Bounce();
Bounce offSwitch = Bounce();

Servo servo;

volatile byte state;
volatile byte event;

bool justTransitioned = false;

void setup() {
  Serial.begin(9600);

  initServo();

  usbMIDI.setHandleProgramChange(onProgramChange);

  pinMode(onSwitchPin, INPUT_PULLUP);
  onSwitch.attach(onSwitchPin);

  pinMode(offSwitchPin, INPUT_PULLUP);
  offSwitch.attach(offSwitchPin);

  pinMode(audioPowerPin, OUTPUT);

  pinMode(redLedPin, OUTPUT);
  pinMode(greenLedPin, OUTPUT);

  pinMode(USBBusPowerPin, INPUT);
  transitionTo(STATE_STANDBY);
}

void loop() {
  readMIDI();
  doStateMachine();
}

//The Main State Machine
void doStateMachine() {
  switch (state) {
    case STATE_STANDBY: {
        if (justTransitioned) {
          Serial.print("Standby\n");

          digitalWrite(redLedPin, HIGH);
          digitalWrite(greenLedPin, LOW);

          switchOffAudio();

          justTransitioned = false;
        }

        onSwitch.update();
        if (onSwitch.fell()) {
          Serial.print("On Button pressed\n");

          transitionTo(STATE_COMPUTER_STARTING);
        }
        break;
      }

    case STATE_COMPUTER_STARTING: {
        if (justTransitioned) {
          Serial.print("Starting Computer\n");

          switchOnMac();

          justTransitioned = false;
        }

        if (digitalRead(USBBusPowerPin) == LOW) {
          Serial.print("USB Bus Power ON\n");

          transitionTo(STATE_HW_STARTING);
        }
        break;
      }

    case STATE_HW_STARTING: {
        if (justTransitioned) {
          Serial.print("Waiting for Hauptwerk to Start\n");

          digitalWrite(redLedPin, HIGH);
          digitalWrite(greenLedPin, HIGH);

          justTransitioned = false;
        }

        if (event == EVENT_AUDIO_ON) {
          transitionTo(STATE_RUNNING);
        }
        break;
      }

    case STATE_RUNNING: {
        if (justTransitioned) {
          Serial.print("Hauptwerk Started\n");

          digitalWrite(redLedPin, LOW);
          digitalWrite(greenLedPin, HIGH);

          switchOnAudio();

          justTransitioned = false;
        }

        offSwitch.update();
        if (offSwitch.fell()) {
          Serial.print("Off Button pressed\n");

          transitionTo(STATE_COMPUTER_STOPPRING);
        }
        break;
      }

    case STATE_COMPUTER_STOPPRING: {
        if (justTransitioned) {
          Serial.print("Computer Stopping\n");

          digitalWrite(redLedPin, HIGH);
          digitalWrite(greenLedPin, HIGH);

          switchOffAudio();
          switchOffMac();

          justTransitioned = false;
        }

        if (digitalRead(USBBusPowerPin) == HIGH) {
          Serial.print("USB Bus Power OFF\n");

          transitionTo(STATE_STANDBY);
        }
        break;
      }
  }
}


void transitionTo(byte newState) {
  justTransitioned = true;
  state = newState;
  event = EVENT_NOTHING;
}

void readMIDI() {
  //Event handlers will be triggered by read().
  while (usbMIDI.read()) {
  }
}


//Hauptwerk needs to be programmed to send CF FF when audio engine starts and CF FE when it stops
void onProgramChange(byte channel, byte program) {
  if (channel == midiChannel) {
    if (program == midiAudioOnPC) {
      Serial.print("Audio Engine Started\n");
      event = EVENT_AUDIO_ON;
    }
  }
}

void switchOnAudio() {
  digitalWrite(audioPowerPin, HIGH);
}

void switchOffAudio() {
  digitalWrite(audioPowerPin, LOW);
}

void switchOnMac() {
  //Moves the servo to mechanically switch on Mac
  servo.attach(servoPin);
  servo.write(70);
  delay(200);
  servo.writeMicroseconds(1500);
  delay(200);
  servo.detach();
}

void switchOffMac() {
  usbMIDI.sendProgramChange(midiShutdownPC, midiChannel);
  delay(200);
  usbMIDI.sendProgramChange(midiShutdownPC, midiChannel);
  delay(200);
}

void initServo() {
  servo.attach(servoPin);
  servo.writeMicroseconds(1500);
  delay(200);
  servo.detach();
}

