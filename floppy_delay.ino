// a 3.5" floppy nominally rotates at 300rpm, which is a period of 200 mS

// the floppy control lines
#define _STEP 7
#define _DIR 6
#define _MOTORA_EN 4
#define _DRIVEA_SEL 5
#define _HEAD_SEL A0 // (aka SIDE) analog input used as a digital output
#define _INDEX 3
#define _READ_DATA 12
#define _WRITE_DATA 8
#define _WRITE_EN 9
#define _TRACK0 10


// the floppy bus uses open collector drivers
// for details, see https://github.com/dhansel/ArduinoFDC/blob/cd3b1c0417d39498b1bb882b5a2bcb5ef93bb41c/ArduinoFDC.cpp#L238
void openCollectorPinMode( byte pin) {
  digitalWrite(pin, LOW);
  pinMode(pin, INPUT);
}

// again, see dhansel's code linked above.
void digitalWriteOC(byte pin, byte state) {
  if( state==LOW )
    pinMode(pin, OUTPUT); // the pin is set to output LOW, so setting it to OUTPUT mode drives the pin to zero volts
  else 
    pinMode(pin, INPUT); // setting the pin HIGH leaves it floating/hi-Z
}

void selectHead( byte head) {
  if( head == 1)
    digitalWriteOC(_HEAD_SEL, LOW);
  else
    digitalWriteOC(_HEAD_SEL, HIGH);
}

void startMotor() {
  digitalWriteOC(_MOTORA_EN, LOW);
}

void stopMotor() {
  // We might want to stop the motor to reduce wear when idling
  digitalWriteOC(_MOTORA_EN, HIGH);
}

void enableWrite() {
  digitalWriteOC(_WRITE_EN, LOW);
}

void disableWrite() {
  digitalWriteOC(_WRITE_EN, HIGH);
}

void enableDrive() {
  digitalWriteOC(_DRIVEA_SEL, LOW);
}

void disableDrive() {
  digitalWriteOC(_DRIVEA_SEL, HIGH);
}

void setup() {
  // outputs from Arduino to Floppy
  openCollectorPinMode(_STEP);
  openCollectorPinMode(_DIR);
  openCollectorPinMode(_MOTORA_EN);
  openCollectorPinMode(_DRIVEA_SEL);
  openCollectorPinMode(_HEAD_SEL); // aka SIDE
  openCollectorPinMode(_WRITE_EN);

  // I'm unsure as to why dhansel sets up this one pin that way
  // I think it's because they're using a the timer/output-compare system
  //  to drive this pin directly, which can't do the "open collector"
  //  trick. Since I'm doing everything in software, I feel okay using
  //  the open collector trick. 
  //digitalWrite(_WRITE_DATA, HIGH);
  //pinMode(_WRITE_DATA, OUTPUT); 
  openCollectorPinMode(_WRITE_DATA);

  // inputs to Arduino from Floppy
  pinMode(_READ_DATA, INPUT_PULLUP); // dhansel recommends an additional external pullup of 1K
  pinMode(_INDEX, INPUT_PULLUP);
  pinMode(_TRACK0, INPUT_PULLUP);

  Serial.begin(115200);

  startMotor();
  enableDrive();
  selectHead(0);
}

void bitDelayFM() {
  // we have 200mS per revolution
  // in FM, we write two bits per input bit
  // if we delay 10uS per bit, that's 20uS per byte, and 10,000 bytes per track
  // With 2 sides and 80 tracks each, that's 160*10k = 1.56Mbytes/disk! not likely with FM!
  // ten times slower works out to 156k/disk, which puts us at a lower density than any 3.5" floppy
  // that's 1000 bytes a track
  delayMicroseconds(100);
}

void writeOneFM() {
  // see https://en.wikipedia.org/wiki/Run-length_limited#FM:_(0,1)_RLL
  // data is active low, and we want to write two ones, so we write two zeroes
  digitalWrite(_WRITE_DATA, LOW);
  bitDelayFM();
  digitalWrite(_WRITE_DATA, LOW); // redundant but we want timing to match
  bitDelayFM();
}

void writeZeroFM() {
  // see https://en.wikipedia.org/wiki/Run-length_limited#FM:_(0,1)_RLL
  // data is active low, and we want to write a one and a zero, so we write a zero and a one
  digitalWrite(_WRITE_DATA, LOW);
  bitDelayFM();
  digitalWrite(_WRITE_DATA, HIGH);
  bitDelayFM();
}

byte readSymbolFM() {
  byte val;
  if( digitalRead(_READ_DATA))
    val = 0x00;
  else
    val = 0x01;
  bitDelayFM();
  if( digitalRead(_READ_DATA))
    asm("nop");
  else
    val |= 0x02;
  bitDelayFM();
  if(val == 0x03)
    return HIGH;
  else
    return LOW;
}

byte buffer[25];

void loop() {
  while(digitalRead(_INDEX) == HIGH) {
    // wait until the _INDEX signal is asserted
    asm("nop");
  }

  Serial.print('W');
  // let's write some numbers
  enableWrite();
  for( byte d = 0; d < 25; d += 1) {
    for( byte mask = 0x01; mask != 0; mask = mask << 1) {
      if( d & mask)
        writeOneFM();
      else
        writeZeroFM();
    }
  }
  disableWrite();


  while(digitalRead(_INDEX) == HIGH) {
    // wait until the _INDEX signal is asserted
    asm("nop");
  }

  Serial.print('R');
  byte current_byte;
  // now let's try to read them back into a buffer
  for( byte i = 0; i < 25; i += 1) {
    current_byte = 0x00;
    for( byte mask = 0x01; mask != 0; mask = mask << 1) {
      if( readSymbolFM())
        current_byte |= mask;
    }
    buffer[i] = current_byte;
  }

  // print out what we found
  Serial.print("\nbuf>>>");
  for( byte i = 0; i < 25; i += 1)
    Serial.write('A' + buffer[i]);
  Serial.print("<<<\n");
}
