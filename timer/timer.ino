
#include <LiquidCrystal.h>
#include <Wire.h>

const byte RTC_ADDRESS = 0x68; // Real Time Clock address
const byte OUT_FAST = 5;
const byte OUT_SLOW = 6;

const byte KEY_OK = 1;
const byte KEY_NEXT = 2;
const byte KEY_PREV = 4;
const byte KEY_CHANGE = 8;

const byte STATE_MENU = 0;
const byte STATE_SETTINGS = 1;
const byte STATE_TIMER = 2;

const byte FUNCTION_TIME = 0;
const byte FUNCTION_DATE = 1;
const byte FUNCTION_TYPE = 2;
const byte FUNCTION_DELAY = 3;
const byte FUNCTION_START = 4;

const byte TYPE_ONE = 0;
const byte TYPE_MULTI = 1;
const byte TYPE_TRIGGER = 2;

const byte LCD_WIDTH = 16;
const byte LCD_HEIGHT = 2;
const char* BLANK_STRING = "                ";
const char* OK_MESSAGE   = "OK              ";

bool needToRedraw = true;
bool needToSetTime = false;
bool needToSetDate = false;

byte upArrow[8] = {
  0b00100,
  0b00100,
  0b01110,
  0b01110,
  0b01110,
  0b11111,
  0b11111,
  0b11111
};

const byte NDAYS[2][12] = { {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
  {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
};

byte date_year_leap( int year ) {
  if ((((year) % 4) == 0 && ((year) % 100) != 0) || ((year) % 400) == 0)
    return 1;
  else
    return 0;
}


unsigned long date_2unixtime(int DD, int MM, int YY, int hh, int mm, int ss) {
  unsigned long days = ((YY - 1970) * (365 + 365 + 366 + 365) + 1) / 4 + DD - 1;

  switch (MM) {
    case 2:
      days += 31;
      break;
    case 3:
      days += 31 + 28;
      break;
    case 4:
      days += 31 + 28 + 31;
      break;
    case 5:
      days += 31 + 28 + 31 + 30;
      break;
    case 6:
      days += 31 + 28 + 31 + 30 + 31;
      break;
    case 7:
      days += 31 + 28 + 31 + 30 + 31 + 30;
      break;
    case 8:
      days += 31 + 28 + 31 + 30 + 31 + 30 + 31;
      break;
    case 9:
      days += 31 + 28 + 31 + 30 + 31 + 30 + 31 + 31;
      break;
    case 10:
      days += 31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30;
      break;
    case 11:
      days += 31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31;
      break;
    case 12:
      days += 31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30;
      break;
  }

  if ( (MM >= 3) && (YY % 4 == 0) )
    days ++;

  return days * 24 * 3600 + hh * 3600 + mm * 60 + ss;
}

LiquidCrystal lcd(0, 1, A0, A1, A2, A3);

class Timer {
  public:
    unsigned long value = 0;
    unsigned long t = 0;
    bool finished = false;
    bool started = false;
} timer;

class Menu {
  public:
    byte state = STATE_MENU; // STATE_MENU = 0 - menu, STATE_SETTINGS = 1 - settings, STATE_TIMER = 2 - timer
    byte function = 0; // FUNCTION_TIME = 0 - time, FUNCTION_DATE = 1 - date, FUNCTION_TYPE = 2 - type, FUNCTION_DELAY = 3 - delay, FUNCTION_START = 4 - start
    byte pos = 0;
    const char elements[5][LCD_WIDTH + 1] =    {"TIME            ", "DATE            ", "TYPE            ", "DELAY           ", "START           "};
    byte type = 0;
    const char types[3][LCD_WIDTH + 1] =       {"ONE             ", "MULTI           ", "TRIGGER         "};
    byte multiplier = 0;
    const char multipliers[4][LCD_WIDTH + 1] = {" x0.1s          ", " x1.0s          ", " x1.0m          ", " x1.0h          "};
    byte value = 0;
    byte time[3];
    byte date[3];
} menu;

byte bcd2dec(byte c) {
  return c / 16 * 10 + c % 16;
}

byte dec2bcd(byte c) {
  return (c / 10 << 4) | (c % 10);
}

void getTime(byte *hh, byte *mm, byte *ss) {
  Wire.beginTransmission(RTC_ADDRESS);
  Wire.write(0);
  Wire.endTransmission();

  Wire.requestFrom(RTC_ADDRESS, 3); // request 3 bytes
  *ss = Wire.read() & 0x7f;
  *mm = Wire.read();
  *hh = Wire.read() & 0x3f;
}

void getDate(byte *year, byte *month, byte *day) {

  Wire.beginTransmission(RTC_ADDRESS);
  Wire.write(0x04); // start from day
  Wire.endTransmission();

  Wire.requestFrom(RTC_ADDRESS, 3); // request 3 bytes

  *day = Wire.read(); // day
  *month = Wire.read(); // month
  *year = Wire.read(); // year

}

void setTime(byte hh, byte mm, byte ss) {
  Wire.beginTransmission(RTC_ADDRESS);
  Wire.write(0);
  Wire.write(ss);
  Wire.write(mm);
  Wire.write(hh);
  Wire.endTransmission();
}

void setDate(byte day, byte month, byte year) {
  Wire.beginTransmission(RTC_ADDRESS);
  Wire.write(0x04);
  Wire.write(day);
  Wire.write(month);
  Wire.write(year);
  Wire.endTransmission();
}

void time2str(byte hh, byte mm, byte ss, char *str) {
  str[0] = (hh >> 4) + '0';
  str[1] = (hh & 0x0F) + '0';
  str[2] = ' ';
  str[3] = ':';
  str[4] = ' ';
  str[5] = (mm >> 4) + '0';
  str[6] = (mm & 0x0F) + '0';
  str[7] = '\0';
}

void date2str(byte year, byte month, byte day, char *str) {
  str[0] = (year >> 4) + '0';
  str[1] = (year & 0x0F) + '0';
  str[2] = ' ';
  str[3] = '-';
  str[4] = ' ';
  str[5] = (month >> 4) + '0';
  str[6] = (month & 0x0F) + '0';
  str[7] = ' ';
  str[8] = '-';
  str[9] = ' ';
  str[10] = (day >> 4) + '0';
  str[11] = (day & 0x0F) + '0';
  str[12] = '\0';
}

void val2str(byte v, char *str) {
  str[0] = ((v & 0xF0) >> 4) + '0';
  str[1] = (v & 0x0F) + '0';
  str[2] = '\0';
}


void startInt() {
  timer.finished = false;
  timer.started = true;
  if (menu.multiplier < 2) {
    timer.value = 0;
    timer.t = millis();
  } else {
    timer.value = 0;
    timer.t = 0;
  }
}

void keysProcessing() {

  byte ss, mm, hh;

  needToRedraw = true;

  switch (PINB >> 2) {

    case KEY_OK: // OK (UP)
      switch (menu.state) {

        case STATE_TIMER:
          digitalWrite(OUT_SLOW, LOW);
          digitalWrite(OUT_FAST, LOW);
          menu.state = STATE_MENU; // menu
          timer.started = false;
          break;

        case STATE_MENU: // menu
          switch (menu.function) {

            case FUNCTION_START: // start
              menu.state = STATE_TIMER; // timer
              timer.finished = false;
              timer.started = false;
              if (menu.multiplier < 2) {
                timer.value = 0;
                timer.t = millis();
              } else {
                timer.value = 0;
                timer.t = 0;
              }

              break;

            case FUNCTION_DATE: // date
              menu.pos = 2;
            case FUNCTION_TIME: // time
            case FUNCTION_TYPE: // type
            case FUNCTION_DELAY: // delay
              menu.state = STATE_SETTINGS;
              break;

          }
          break;

        case STATE_SETTINGS: // settings
          menu.pos = 0;
          switch (menu.function) {
            case FUNCTION_TIME: // time
            case FUNCTION_DATE: // date
            case FUNCTION_TYPE: // type
            case FUNCTION_DELAY: // delay
              menu.state = STATE_MENU; // go to main menu
              break;
          }
          break;
      }
      break;

    case KEY_NEXT: // NEXT (RIGHT)
      switch (menu.state) {
        case STATE_SETTINGS: // settings

          switch (menu.function) {
            case FUNCTION_TIME: // time
              if (menu.pos < 6)
                if (menu.pos == 1)
                  menu.pos += 4;
                else
                  menu.pos++;
              break;

            case FUNCTION_DATE: // date
              if (menu.pos < 13)
                if ((menu.pos == 3) || (menu.pos == 8))
                  menu.pos += 4;
                else
                  menu.pos++;
              break;

            case FUNCTION_DELAY: // delay
              if (menu.pos < 3)
                if (menu.pos == 1)
                  menu.pos += 2;
                else
                  menu.pos++;
              break;

          } // case function
          break;
      } // case state
      break;

    case KEY_PREV: // PREV (LEFT)
      switch (menu.state) {
        case STATE_SETTINGS: // settings
          switch (menu.function)
          {
            case FUNCTION_TIME: // time
              if (menu.pos > 0)
                if (menu.pos == 5)
                  menu.pos -= 4;
                else
                  menu.pos--;
              break;

            case FUNCTION_DATE: // date
              if (menu.pos > 0)
                if ((menu.pos == 7) || (menu.pos == 12))
                  menu.pos -= 4;
                else
                  menu.pos--;
              break;

            case FUNCTION_DELAY: // delay
              if (menu.pos > 0)
                if (menu.pos == 3)
                  menu.pos -= 2;
                else
                  menu.pos--;
              break;

          } // case function
      } // case state
      break;

    case KEY_CHANGE: // CHANGE (DOWN)
      switch (menu.state) {

        case STATE_MENU: // menu
          if (menu.function < 4)
            menu.function++;
          else
            menu.function = 0;
          break;

        case STATE_SETTINGS: // settings
          switch (menu.function) {

            case FUNCTION_TIME: // time
              needToSetTime = true;
              switch (menu.pos) {
                  byte temp;
                case 0:
                  temp = (menu.time[0] >> 4) + 1;
                  if ((temp == 3) || (temp == 2) && ((menu.time[0] & 0x0F) > 3))
                    temp = 0;
                  menu.time[0] &= 0x0F;
                  menu.time[0] |= temp << 4;
                  break;
                case 1:
                  temp = (menu.time[0] & 0x0F) + 1;
                  if (( (temp > 9)) || (((menu.time[0] >> 4) == 2) && (temp > 3)))
                    temp = 0;
                  menu.time[0] &= 0xF0;
                  menu.time[0] |= temp;
                  break;
                case 5:
                  temp = (menu.time[1] >> 4) + 1;
                  if (temp > 5)
                    temp = 0;
                  menu.time[1] &= 0x0F;
                  menu.time[1] |= temp << 4;
                  break;
                case 6:
                  temp = (menu.time[1] & 0x0F) + 1;
                  if (temp > 9)
                    temp = 0;
                  menu.time[1] &= 0xF0;
                  menu.time[1] |= temp;
                  break;

              }
              break;

            case FUNCTION_DATE: // date
              needToSetDate = true;
              switch (menu.pos) {
                  byte temp;
                  byte nDays;
                case 2:
                  temp = (menu.date[2] >> 4) + 1;
                  if (temp > 9)
                    temp = 0;
                  menu.date[2] &= 0x0F;
                  menu.date[2] |= temp << 4;
                  menu.date[1] = 1;
                  menu.date[0] = 1;
                  break;
                case 3:
                  temp = (menu.date[2] & 0x0F) + 1;
                  if (temp > 9)
                    temp = 0;
                  menu.date[2] &= 0xF0;
                  menu.date[2] |= temp;
                  menu.date[1] = 1;
                  menu.date[0] = 1;
                  break;
                case 7:
                  temp = (menu.date[1] >> 4) + 1;
                  if ((((menu.date[1] & 0x0F) > 2) && (temp > 0)) || (((menu.date[1] & 0x0F) <= 2) && (temp > 1)))
                    temp = 0;
                  menu.date[1] &= 0x0F;
                  menu.date[1] |= temp << 4;
                  if (menu.date[1] == 0)
                    menu.date[1] = 1;
                  menu.date[0] = 1;
                  break;
                case 8:
                  temp = (menu.date[1] & 0x0F) + 1;
                  if ((((menu.date[1] >> 4) == 0) && (temp > 9)) || (((menu.date[1] >> 4 == 1) && (temp > 2))))
                    temp = 0;
                  menu.date[1] &= 0xF0;
                  menu.date[1] |= temp;
                  if (menu.date[1] == 0)
                    menu.date[1] = 1;
                  menu.date[0] = 1;
                  break;
                case 12:
                  nDays = NDAYS[date_year_leap(bcd2dec(menu.date[2]) + 2000)][bcd2dec(menu.date[1]) - 1];
                  temp = (menu.date[0] >> 4) + 1;
                  if (((temp >= dec2bcd(nDays) >> 4) && ( (menu.date[0] & 0x0F) > (dec2bcd(nDays) & 0x0F) )) || (bcd2dec(menu.date[0] + 10) > nDays) )
                    temp = 0;
                  menu.date[0] &= 0x0F;
                  menu.date[0] |= temp << 4;
                  if (menu.date[0] == 0)
                    menu.date[0] = 1;
                  break;
                case 13:
                  nDays = NDAYS[date_year_leap(bcd2dec(menu.date[2]) + 2000)][bcd2dec(menu.date[1]) - 1];
                  temp = (menu.date[0] & 0x0F) + 1;
                  if ((((menu.date[0] >> 4) >= dec2bcd(nDays) >> 4) && ( temp > (dec2bcd(nDays) & 0x0F) )) || (temp > 9) )
                    temp = 0;
                  menu.date[0] &= 0xF0;
                  menu.date[0] |= temp;
                  if (menu.date[0] == 0)
                    menu.date[0] = 1;
                  break;
              } // case pos
              break;

            case FUNCTION_TYPE: // type
              if (menu.type < 2)
                menu.type++;
              else
                menu.type = 0;
              break;

            case FUNCTION_DELAY: // delay
              switch (menu.pos) {
                  byte next;
                case 0:
                  next = (menu.value >> 4) + 1;
                  if (next > 9)
                    next = 0;
                  menu.value &= 0x0F;
                  menu.value |= next << 4;
                  break;
                case 1:
                  next = (menu.value & 0x0F) + 1;
                  if (next > 9)
                    next = 0;
                  menu.value &= 0xF0;
                  menu.value |= next;
                  break;
                case 3:
                  if (menu.multiplier < 3)
                    menu.multiplier++;
                  else
                    menu.multiplier = 0;
                  break;
              }
              break;

          } // case function
          break;
      } // case state

      break;

  }

}

void setup() {

  // 1 Hz output
  Wire.beginTransmission(RTC_ADDRESS);
  Wire.write(0x07);
  Wire.write(0x10);
  Wire.endTransmission();

  lcd.begin(LCD_WIDTH, LCD_HEIGHT);
  lcd.createChar(0, upArrow);

  // Enable output
  pinMode(OUT_FAST, OUTPUT);
  digitalWrite(OUT_FAST, LOW);
  pinMode(OUT_SLOW, OUTPUT);
  digitalWrite(OUT_SLOW, LOW);

  DDRB = 0;
  attachInterrupt(1, keysProcessing, RISING);
  attachInterrupt(0, startInt, RISING);

}

int CNT = 0;
void loop() {

  if (menu.multiplier < 2)
    delayMicroseconds(50);
  else {
    CNT++;
    delay(50);
  }

  if (menu.state == STATE_TIMER) {

    switch (menu.type) {

      case TYPE_ONE:

        switch (menu.multiplier) {

          case 0: // x0.1s

            if (millis() - timer.t >= 10) {
              timer.value++;
              timer.t = millis();
            }

            if ((timer.value / 10) == bcd2dec(menu.value)) {
              digitalWrite(OUT_FAST, HIGH);
              lcd.setCursor(0, 1);
              lcd.print(OK_MESSAGE);
              timer.finished = true;
            }

            break;

          case 1: // x1.0s

            if (millis() - timer.t >= 100) {
              timer.value++;
              timer.t = millis();
            }

            if ((timer.value / 10) == bcd2dec(menu.value)) {
              timer.value = 0;
              timer.t = millis();
              digitalWrite(OUT_FAST, HIGH);
              digitalWrite(OUT_SLOW, HIGH);
              lcd.setCursor(0, 1);
              lcd.print(OK_MESSAGE);
              timer.finished = true;
            }

            break;

          case 2: // x1.0m
          case 3: // x1.0h

            byte day, month, year, hour, minute, second;
            getDate(&year, &month, &day);
            getTime(&hour, &minute, &second);

            int DD, MM, YY, hh, mm, ss;

            DD = bcd2dec(day);
            MM = bcd2dec(month);
            YY = bcd2dec(year) + 2000;
            hh = bcd2dec(hour);
            mm = bcd2dec(minute);
            ss = bcd2dec(second);

            if (timer.t == 0) {
              timer.t = date_2unixtime(DD, MM, YY, hh, mm, ss);
            }

            timer.value = date_2unixtime(DD, MM, YY, hh, mm, ss);

            unsigned long multiplier = 60;
            if (menu.multiplier == 3)
              multiplier *= 60;

            if ( (unsigned long) timer.value >= (unsigned long) bcd2dec(menu.value) * multiplier + timer.t ) {
              timer.value = 0;
              timer.t = 0;
              digitalWrite(OUT_FAST, HIGH);
              digitalWrite(OUT_SLOW, HIGH);
              lcd.setCursor(0, 1);
              lcd.print(OK_MESSAGE);
              timer.finished = true;
            }

            if ((CNT % 10 == 0) && !timer.finished)
              needToRedraw = true;
            break;

        }

        break;

      case TYPE_MULTI:

        switch (menu.multiplier) {

          case 0: // x0.1s

            if (millis() - timer.t >= 10) {
              timer.value++;
              timer.t = millis();
            }

            if ((timer.value / 10) == bcd2dec(menu.value)) {
              timer.value = 0;
              timer.t = millis();
              digitalWrite(OUT_FAST, HIGH);
              delayMicroseconds(1000);
              digitalWrite(OUT_FAST, LOW);
            }

            break;

          case 1: // x1.0s

            if (millis() - timer.t >= 100) {
              timer.value++;
              timer.t = millis();
            }

            if ((timer.value / 10) == bcd2dec(menu.value)) {
              timer.value = 0;
              timer.t = millis();
              digitalWrite(OUT_FAST, HIGH);
              digitalWrite(OUT_SLOW, HIGH);
              delay(100);
              digitalWrite(OUT_FAST, LOW);
              digitalWrite(OUT_SLOW, LOW);
            }

            break;

          case 2: // x1.0m
          case 3: // x1.0h
            byte day, month, year, hour, minute, second;
            getDate(&year, &month, &day);
            getTime(&hour, &minute, &second);

            int DD, MM, YY, hh, mm, ss;

            DD = bcd2dec(day);
            MM = bcd2dec(month);
            YY = bcd2dec(year) + 2000;
            hh = bcd2dec(hour);
            mm = bcd2dec(minute);
            ss = bcd2dec(second);

            if (timer.t == 0) {
              timer.t = date_2unixtime(DD, MM, YY, hh, mm, ss);
            }

            timer.value = date_2unixtime(DD, MM, YY, hh, mm, ss);

            unsigned long multiplier = 60;
            if (menu.multiplier == 3)
              multiplier *= 60;

            if ( (unsigned long) timer.value >= (unsigned long) bcd2dec(menu.value) * multiplier + timer.t ) {
              timer.value = 0;
              timer.t = 0;
              digitalWrite(OUT_FAST, HIGH);
              digitalWrite(OUT_SLOW, HIGH);
              delay(100);
              digitalWrite(OUT_FAST, LOW);
              digitalWrite(OUT_SLOW, LOW);
            }

            if ( CNT % 10 == 0 )
              needToRedraw = true;
            break;

        }

        break;


      case TYPE_TRIGGER:

        if (timer.started) {

          switch (menu.multiplier) {

            case 0: // x0.1s

              if (millis() - timer.t >= 10) {
                timer.value++;
                timer.t = millis();
              }

              if ((timer.value / 10) == bcd2dec(menu.value)) {
                digitalWrite(OUT_FAST, HIGH);
                lcd.setCursor(0, 1);
                lcd.print(OK_MESSAGE);
                timer.finished = true;
              }



              break;

            case 1: // x1.0s

              if (millis() - timer.t >= 100) {
                timer.value++;
                timer.t = millis();
              }

              if ((timer.value / 10) == bcd2dec(menu.value)) {
                timer.value = 0;
                timer.t = millis();
                digitalWrite(OUT_FAST, HIGH);
                digitalWrite(OUT_SLOW, HIGH);
                lcd.setCursor(0, 1);
                lcd.print(OK_MESSAGE);
                timer.finished = true;
              }

              break;

            case 2: // x1.0m
            case 3: // x1.0h

              byte day, month, year, hour, minute, second;
              getDate(&year, &month, &day);
              getTime(&hour, &minute, &second);

              int DD, MM, YY, hh, mm, ss;

              DD = bcd2dec(day);
              MM = bcd2dec(month);
              YY = bcd2dec(year) + 2000;
              hh = bcd2dec(hour);
              mm = bcd2dec(minute);
              ss = bcd2dec(second);

              if (timer.t == 0) {
                timer.t = date_2unixtime(DD, MM, YY, hh, mm, ss);
              }

              timer.value = date_2unixtime(DD, MM, YY, hh, mm, ss);

              unsigned long multiplier = 60;
              if (menu.multiplier == 3)
                multiplier *= 60;

              if ( (unsigned long) timer.value >= (unsigned long) bcd2dec(menu.value) * multiplier + timer.t ) {
                timer.value = 0;
                timer.t = 0;
                digitalWrite(OUT_FAST, HIGH);
                digitalWrite(OUT_SLOW, HIGH);
                lcd.setCursor(0, 1);
                lcd.print(OK_MESSAGE);
                timer.finished = true;
              }

              if ((CNT % 10 == 0) && !timer.finished)
                needToRedraw = true;
              break;

          }
        }
        break;


    }

    if (needToRedraw) {

      lcd.setCursor(0, 0);

      lcd.print(menu.types[menu.type]);

      lcd.setCursor(8, 0);

      char value[LCD_WIDTH];
      val2str(menu.value, value);
      lcd.print(value);

      lcd.print(menu.multipliers[menu.multiplier]);

      if ((menu.multiplier >= 2)) {
        lcd.setCursor(0, 1);
        lcd.print(BLANK_STRING);
        lcd.setCursor(0, 1);

        unsigned long multiplier = 60;
        if (menu.multiplier == 3)
          multiplier *= 60;

        unsigned long diff = (unsigned long) bcd2dec(menu.value) * multiplier + timer.t - timer.value;
        int s = diff % 60;
        int m = ( ( diff - s ) / 60 ) % 60;
        int h = (( (( diff - s ) / 60) - m ) / 60 ) % 60;
        if (h < 10)
          lcd.print('0');
        lcd.print(h);
        lcd.print("h ");
        if (m < 10)
          lcd.print('0');
        lcd.print(m);
        lcd.print("m ");
        if (s < 10)
          lcd.print('0');
        lcd.print(s);
        lcd.print("s ");
      }


      needToRedraw = false;

    }

  }



  if (needToSetTime) {
    setTime(menu.time[0], menu.time[1], menu.time[2]);
    needToSetTime = false;
  }

  if (needToSetDate) {
    setDate(menu.date[0], menu.date[1], menu.date[2]);
    needToSetDate = false;
  }

  if (needToRedraw) {
    switch (menu.state) {

      case STATE_MENU: // menu
        lcd.setCursor(0, 0);
        lcd.print(menu.elements[menu.function]);
        lcd.setCursor(0, 1);
        lcd.print(BLANK_STRING);
        break;

      case STATE_SETTINGS: // settings
        switch (menu.function) {
          case FUNCTION_TIME: // time

            byte hh, mm, ss;
            char time[LCD_WIDTH];
            getTime(&hh, &mm, &ss);
            time2str(hh, mm, ss, time);

            menu.time[0] = hh;
            menu.time[1] = mm;
            menu.time[2] = ss;

            lcd.setCursor(0, 0);
            lcd.print(time);

            lcd.setCursor(0, 1);
            lcd.write(BLANK_STRING);

            lcd.setCursor(menu.pos, 1);
            lcd.write((byte)0);

            break;

          case FUNCTION_DATE: // date
            byte year, month, day;
            char date[LCD_WIDTH];
            getDate(&year, &month, &day);

            menu.date[0] = day;
            menu.date[1] = month;
            menu.date[2] = year;

            date2str(year, month, day, date);

            lcd.setCursor(0, 0);
            lcd.print("20");
            lcd.print(date);

            lcd.setCursor(0, 1);
            lcd.write(BLANK_STRING);

            lcd.setCursor(menu.pos, 1);
            lcd.write((byte)0);

            break;

          case FUNCTION_TYPE: // type
            lcd.setCursor(0, 0);
            lcd.print(menu.types[menu.type]);
            break;

          case FUNCTION_DELAY: // delay
            lcd.setCursor(0, 0);
            char value[LCD_WIDTH];
            val2str(menu.value, value);
            lcd.print(value);
            lcd.print(menu.multipliers[menu.multiplier]);

            lcd.setCursor(0, 1);
            lcd.write(BLANK_STRING);

            lcd.setCursor(menu.pos, 1);
            lcd.write((byte)0);

            break;

        } // switch function
        break;
    } // switch state
    needToRedraw = false;
  }


}

