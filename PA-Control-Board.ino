// PA-Control-Board.ino
// RP2040 (rpipico) AT-command firmware with hardware SPI

/*
AT → replies OK

AT+SPI=0xABCD → sends 0xABCD to the SPI device, reads 16-bit reply and replies OK+SPI=0x1234

AT+LED=R,ON or AT+LED=B,OFF → control single LED

AT+LED=RGB,101 → R=1, G=0, B=1 (1=ON, 0=OFF)

AT+RELAY=3,ON → turn relay #3 (GPIO10) ON

AT+IO=20,HIGH → set GPIO20 HIGH (or LOW)

AT+TR=HIGH and AT+FH_CLK=LOW → shortcuts for IO20/21
*/

#include <Arduino.h>
#include <SPI.h>

// ---------- PIN CONFIG ----------
const uint8_t PIN_SPI_MISO = 16;   // MISO from target (matches your schematic)
const uint8_t PIN_SPI_CS   = 17;   // active-low CS
const uint8_t PIN_SPI_SCK  = 18;   // SPI clock
const uint8_t PIN_SPI_MOSI = 19;   // MOSI to target

const uint8_t PIN_LED_R = 2;  // change if needed
const uint8_t PIN_LED_G = 3;
const uint8_t PIN_LED_B = 4;

// Relays mapped to GPIO8..GPIO15 for relay 1..8
const uint8_t RELAY_PINS[8] = {8, 9, 10, 11, 12, 13, 14, 15};

const uint8_t PIN_TR      = 20; // IO20 (TR)
const uint8_t PIN_FH_CLK  = 21; // IO21 (FH_CLK)

String lineBuf = "";
bool toggleTR = false;
long lastToggleMillis;
const int TOGGLE_DELAY = 1000;

SPISettings spisettings(300000, MSBFIRST, SPI_MODE0);
// SPISettings spisettings(1000000, MSBFIRST, SPI_MODE1);
// SPISettings spisettings(1000000, MSBFIRST, SPI_MODE2);
// SPISettings spisettings(1000000, MSBFIRST, SPI_MODE3);

// SPISettings spisettings(1000000, LSBFIRST, SPI_MODE0);
// SPISettings spisettings(1000000, LSBFIRST, SPI_MODE1);
// SPISettings spisettings(1000000, LSBFIRST, SPI_MODE2);
// SPISettings spisettings(1000000, LSBFIRST, SPI_MODE3);

String trimStr(const String &s) {
  int i=0, j=s.length()-1;
  while (i<=j && isspace(s[i])) i++;
  while (j>=i && isspace(s[j])) j--;
  if (j < i) return "";
  return s.substring(i, j+1);
}

uint32_t parseNumber(const String &s, bool &ok) {
  ok = false;
  String t = trimStr(s);
  if (t.length() == 0) return 0;
  if (t.startsWith("0x") || t.startsWith("0X")) {
    char *endptr;
    unsigned long v = strtoul(t.c_str()+2, &endptr, 16);
    ok = true;
    return (uint32_t)v;
  } else {
    char *endptr;
    unsigned long v = strtoul(t.c_str(), &endptr, 10);
    ok = true;
    return (uint32_t)v;
  }
}

// ---------- Command handlers ----------
void handle_AT() {
  Serial.print("OK\r\n");
}

void handle_SPICMD(const String &arg)
{
    uint8_t tx[10];
    tx[0] = 'A';
    tx[1] = 'T';

    int index = 2;
    int start = 0;

    while (index < 10) {
        int comma = arg.indexOf(',', start);
        String token;

        if (comma == -1) {
            token = arg.substring(start);
        } else {
            token = arg.substring(start, comma);
            start = comma + 1;
        }

        token.trim();

        bool ok;
        uint32_t v = parseNumber(token, ok);
        if (!ok || v > 0xFF) {
            Serial.print("ERR:BADBYTE\r\n");
            return;
        }

        tx[index++] = (uint8_t)v;

        if (comma == -1) break;
    }

    if (index != 10) {
        Serial.print("ERR:NEED8BYTES\r\n");
        return;
    }

    SPI.beginTransaction(spisettings);
    digitalWrite(PIN_SPI_CS, LOW);

    for (int i = 0; i < 10; i++) {
        SPI.transfer(tx[i]);
    }

    digitalWrite(PIN_SPI_CS, HIGH);
    SPI.endTransaction();

    Serial.print("OK\r\n");
}

void handle_SPIACK()
{
    uint8_t rx[10];
    
    SPI.beginTransaction(spisettings);
    digitalWrite(PIN_SPI_CS, LOW);
    for (int i = 0; i < 10; i++) {
        rx[i] = SPI.transfer(0x00);
    }
    digitalWrite(PIN_SPI_CS, HIGH);
    SPI.endTransaction();

    Serial.print("OK\r\nACK=");
    for (int i = 0; i < 10; i++) {
        if(i == 0) {
          if((rx[i] == 0x41) || (rx[i] == 0x61)) {
            Serial.print("A ");
            continue;
          }
        }
        else if (i == 1) {
          if((rx[i] == 0x74) || (rx[i] == 0x54)) {
            Serial.print("T ");
            continue;
          }
        }
        Serial.print("0x");
        if (rx[i] < 0x10) Serial.print('0');
        Serial.print(rx[i], HEX);
        Serial.print(' ');
    }
    Serial.print("\r\n");
}

void handle_LED(const String &arg) {
  int comma = arg.indexOf(',');
  if (comma < 0) { Serial.print("ERR:LED_SYNTAX\r\n"); return; }
  String a = trimStr(arg.substring(0, comma));
  String b = trimStr(arg.substring(comma+1));
  a.toUpperCase(); b.toUpperCase();

  auto setLed = [&](uint8_t pin, bool on){
    digitalWrite(pin, on ? HIGH : LOW);
  };

  if (a == "RGB") {
    if (b.length() < 3) { Serial.print("ERR:RGB_BAD\r\n"); return; }
    setLed(PIN_LED_R, b[0] == '1');
    setLed(PIN_LED_G, b[1] == '1');
    setLed(PIN_LED_B, b[2] == '1');
    Serial.print("OK\r\n");
    return;
  }

  bool on;
  if (b == "ON" || b == "1" || b == "HIGH") on = true;
  else if (b == "OFF" || b == "0" || b == "LOW") on = false;
  else { Serial.print("ERR:LED_ARG\r\n"); return; }

  if (a == "R") setLed(PIN_LED_R, on);
  else if (a == "G") setLed(PIN_LED_G, on);
  else if (a == "B") setLed(PIN_LED_B, on);
  else { Serial.print("ERR:LED_NAME\r\n"); return; }

  Serial.print("OK\r\n");
}

void handle_RELAY(const String &arg) {
  int comma = arg.indexOf(',');
  if (comma < 0) {
    Serial.print("ERR:RELAY_SYNTAX\r\n");
    return;
  }
  String sIndex = trimStr(arg.substring(0, comma));
  String sState = trimStr(arg.substring(comma+1));
  bool ok; uint32_t idx = parseNumber(sIndex, ok);
  if (!ok || idx < 1 || idx > 8) {
    Serial.print("ERR:RELAY_INDEX\r\n");
    return;
  }
  sState.toUpperCase();
  bool on = false;
  
  if (sState == "ON" || sState == "1" || sState=="HIGH") on = true;
  else if (sState == "OFF" || sState=="0" || sState=="LOW") on = false;
  else {
    Serial.print("ERR:RELAY_ARG\r\n");
    return;
  }

  uint8_t pin = RELAY_PINS[idx - 1];
  digitalWrite(pin, on ? HIGH : LOW);
  Serial.print("OK\r\n");
}

void handle_IO(const String &arg) {
  int comma = arg.indexOf(',');
  if (comma < 0) {
    Serial.print("ERR:IO_SYNTAX\r\n");
    return;
  }
  String sPin = trimStr(arg.substring(0, comma));
  String sState = trimStr(arg.substring(comma+1));
  bool ok; uint32_t pinNum = parseNumber(sPin, ok);
  if (!ok) {
    Serial.print("ERR:IO_PIN\r\n");
    return;
  }
  sState.toUpperCase();
  bool on;
  if (sState=="HIGH"||sState=="1"||sState=="ON") on = true;
  else if (sState=="LOW"||sState=="0"||sState=="OFF") on = false;
  else { Serial.print("ERR:IO_STATE\r\n"); return; }
  if (pinNum == PIN_TR) digitalWrite(PIN_TR, on ? HIGH : LOW);
  else if (pinNum == PIN_FH_CLK) digitalWrite(PIN_FH_CLK, on ? HIGH : LOW);
  else {
    Serial.print("ERR:IO_NOT_ALLOWED\r\n");
    return;
  }
  Serial.print("OK\r\n");
}

void handle_TR(const String &arg) {
  int comma = arg.indexOf(',');
  if (comma < 0) {
    Serial.print("ERR:IO_SYNTAX\r\n");
    return;
  }
  String sPin = trimStr(arg.substring(0, comma));
  String sState = trimStr(arg.substring(comma+1));
  bool ok; uint32_t pinNum = parseNumber(sPin, ok);
  if (!ok) {
    Serial.print("ERR:IO_PIN\r\n");
    return;
  }
  sState.toUpperCase();
  bool on;
  if (sState=="HIGH"||sState=="1"||sState=="ON") on = true;
  else if (sState=="LOW"||sState=="0"||sState=="OFF") {
    on = false;
    toggleTR = false;
  }
  else if (sState == "TOGGLE") {
    toggleTR = true;
    on = true;
  }
  else {
    Serial.print("ERR:IO_STATE\r\n");
    return;
  }
  
  if (pinNum == PIN_TR) {
    digitalWrite(PIN_TR, on ? HIGH : LOW);
    if(on) {
      digitalWrite(PIN_LED_G, LOW);
      digitalWrite(PIN_LED_R, HIGH);
    }
    else {
      digitalWrite(PIN_LED_G, HIGH);
      digitalWrite(PIN_LED_R, LOW);
    }
    lastToggleMillis = millis();
  }
  else {
    Serial.print("ERR:IO_NOT_ALLOWED\r\n");
    return;
  }
  Serial.print("OK\r\n");
}

void setup_pins() {
  // Initialize hardware SPI (use default pins for SPI0 on rp2040 core)
  SPI.setRX(PIN_SPI_MISO);
  SPI.setCS(PIN_SPI_CS);
  SPI.setSCK(PIN_SPI_SCK);
  SPI.setTX(PIN_SPI_MOSI);
  SPI.begin(true);

  pinMode(PIN_LED_R, OUTPUT); digitalWrite(PIN_LED_R, LOW);
  pinMode(PIN_LED_G, OUTPUT); digitalWrite(PIN_LED_G, LOW);
  pinMode(PIN_LED_B, OUTPUT); digitalWrite(PIN_LED_B, LOW);

  for (int i=0;i<8;i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], LOW); // default off
  }

  pinMode(PIN_TR, OUTPUT); digitalWrite(PIN_TR, LOW);
  pinMode(PIN_FH_CLK, OUTPUT); digitalWrite(PIN_FH_CLK, LOW);
}

void processLine(const String &ln) {
  String s = trimStr(ln);
  if (s.length() == 0) return;
  String s_up = s;
  s_up.toUpperCase();

  if (s_up == "AT") { handle_AT(); return; }

  if (!s_up.startsWith("AT+")) { Serial.print("ERR:UNKNOWN\r\n"); return; }
  String cmdRest = s.substring(3); // preserve original case for args
  int eq = cmdRest.indexOf('=');
  String cmd = (eq >= 0) ? cmdRest.substring(0, eq) : cmdRest;
  String arg = (eq >= 0) ? cmdRest.substring(eq+1) : "";

  cmd.trim(); arg = trimStr(arg);
  cmd.toUpperCase();

  if (cmd == "SPICMD") handle_SPICMD(arg);
  else if (cmd == "SPIACK") handle_SPIACK();
  else if (cmd == "LED") handle_LED(arg);
  else if (cmd == "RELAY") handle_RELAY(arg);
  else if (cmd == "IO") handle_IO(arg);
  else if (cmd == "TR") handle_TR(String(PIN_TR) + "," + arg);
  else if (cmd == "FH_CLK") handle_IO(String(PIN_FH_CLK) + "," + arg);
  else Serial.print("ERR:CMD\r\n");
}

void rgbTest() {
  digitalWrite(PIN_LED_R, HIGH); delay(1000);
  digitalWrite(PIN_LED_R, LOW);
  digitalWrite(PIN_LED_G, HIGH); delay(1000);
  digitalWrite(PIN_LED_G, LOW);
  digitalWrite(PIN_LED_B, HIGH); delay(1000);
  digitalWrite(PIN_LED_B, LOW);
}

void handle_TRToggle(bool t) {
  if(t) {
    long now = millis();
    if(now >= (lastToggleMillis + TOGGLE_DELAY)) {
      bool state = digitalRead(PIN_TR);
      digitalWrite(PIN_TR, !state);
      if(state) {
        digitalWrite(PIN_LED_G, LOW);
        digitalWrite(PIN_LED_R, HIGH);
      }
      else {
        digitalWrite(PIN_LED_G, HIGH);
        digitalWrite(PIN_LED_R, LOW);
      }
      lastToggleMillis = millis();
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(50);
  setup_pins();
  rgbTest();
  digitalWrite(PIN_LED_G, HIGH);
}

void loop() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if ((c == '\n') || (c == '\r')) {
      processLine(lineBuf);
      lineBuf = "";
    } else {
      lineBuf += c;
      if (lineBuf.length() > 200) {
        lineBuf = "";
        Serial.print("ERR:LINE_TOO_LONG\r\n");
      }
    }
  }
  handle_TRToggle(toggleTR);
  delay(1);
}
