// a 3.5" floppy nominally rotates at 300rpm, which is a period of 200 mS

// the floppy control lines
#define _DENSITY_SELECT 2
#define _INDEX 3
#define _MOTORA_EN 4
#define _DRIVEA_SEL 5
#define _DIR 6
#define _STEP 7
#define _WRITE_DATA 8
#define _WRITE_EN 9
#define _TRACK0 10
#define _WRITE_PROTECT 11
#define _READ_DATA 12
#define _HEAD_SEL A0 // analog input used as a digital output
#define _DISK_CHANGE A1 // analog input used as a digital input

void selectHead( byte head) {
  if( head == 1)
    digitalWrite(_HEAD_SEL, LOW);
  else
    digitalWrite(_HEAD_SEL, HIGH);
}

void startMotor() {
  digitalWrite(_MOTORA_EN, LOW);
}

void stopMotor() {
  // We might want to stop the motor to reduce wear when idling
  digitalWrite(_MOTORA_EN, HIGH);
}

void enableWrite() {
  digitalWrite(_WRITE_EN, LOW);
}

void disableWrite() {
  digitalWrite(_WRITE_EN, HIGH);
}

void setup() {
  pinMode(_DENSITY_SELECT, INPUT);
  pinMode(_INDEX, INPUT);
  pinMode(_MOTORA_EN, OUTPUT);
  pinMode(_DRIVEA_SEL, OUTPUT);
  pinMode(_DIR, OUTPUT);
  pinMode(_STEP, OUTPUT);
  pinMode(_WRITE_DATA, OUTPUT);
  pinMode(_WRITE_EN, OUTPUT);
  pinMode(_TRACK0, INPUT);
  pinMode(_WRITE_PROTECT, INPUT);
  pinMode(_READ_DATA, INPUT);
  pinMode(_HEAD_SEL, OUTPUT);
  pinMode(_DISK_CHANGE, INPUT);
  Serial.begin(115200);

  startMotor();
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
  while(digitalRead(_TRACK0) == HIGH) {
    // wait until the track0 signal is asserted
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


  while(digitalRead(_TRACK0) == HIGH) {
    // wait until the track0 signal is asserted
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
    Serial.print('A' + buffer[i]);
  Serial.print("<<<\n");
}
