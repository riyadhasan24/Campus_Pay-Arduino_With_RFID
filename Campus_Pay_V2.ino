#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <SPI.h>
#include <MFRC522.h>

// ================= LCD =================
LiquidCrystal_I2C lcd(0x25, 16, 2);   // change to 0x27 / 0x3F if needed

// ================= Keypad (4x4) =================
// A = Recharge (Top-up) mode
// B = Show Balance (tap card)
// C = Reset balance to 0 (tap card)
// D = Pay mode

const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {A0, A1, A2, A3};
byte colPins[COLS] = {2, 3, 4, 5};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

#define SS_PIN 10
#define RST_PIN 9
MFRC522 rfid(SS_PIN, RST_PIN);

#define BUZZER_PIN 6
#define GREEN_LED_PIN 8
#define RED_LED_PIN 7

const byte BALANCE_BLOCK = 4;

// ================= Authorized Cards (ONLY these will work) =================
String student1_uid = "29C83903"; 
String student2_uid = "63A2840F";  


enum Action { ACT_PAY, ACT_RECHARGE, ACT_SHOW_BAL, ACT_RESET };
Action currentAction = ACT_PAY;

String amountStr = "";
bool waitingForCard = false;

// Auth key (default: FF FF FF FF FF FF)
MFRC522::MIFARE_Key key;

// Blink control
unsigned long blinkTimer = 0;
bool blinkState = false;

// ---------- LED helpers ----------
void setLEDs(bool greenOn, bool redOn) 
{
  digitalWrite(GREEN_LED_PIN, greenOn ? HIGH : LOW);
  digitalWrite(RED_LED_PIN,   redOn   ? HIGH : LOW);
}

void turnOffLEDs() 
{ 
  setLEDs(false, false); 
}


void playSuccessSound() 
{
  tone(BUZZER_PIN, 1000, 120); delay(140);
  tone(BUZZER_PIN, 1400, 120); delay(140);
  tone(BUZZER_PIN, 1800, 120); delay(140);
  noTone(BUZZER_PIN);
}

void playFailureSound() 
{
  tone(BUZZER_PIN, 300, 400); delay(420);
  noTone(BUZZER_PIN);
}


void blinkBothLEDsStep() 
{
  if (millis() - blinkTimer >= 150) 
  {
    blinkTimer = millis();
    blinkState = !blinkState;
    setLEDs(blinkState, blinkState);
  }
}


unsigned long parseAmount(const String &s) 
{
  if (s.length() == 0) return 0;
  return (unsigned long)s.toInt();
}

void showHome() 
{
  lcd.clear();
  lcd.setCursor(0, 0);

  if (currentAction == ACT_PAY)             lcd.print("PAY: Enter Amt");
  else if (currentAction == ACT_RECHARGE)   lcd.print("TOPUP: Enter Amt");
  else if (currentAction == ACT_SHOW_BAL)   lcd.print("SHOW BALANCE");
  else if (currentAction == ACT_RESET)      lcd.print("RESET BAL=0");

  lcd.setCursor(0, 1);
  if (currentAction == ACT_PAY || currentAction == ACT_RECHARGE) 
  {
    lcd.print("Amt: ");
  } 
  
  else 
  {
    lcd.print("Tap card...");
  }

  amountStr = "";
  waitingForCard = false;
  turnOffLEDs();
}


void stopRFIDSession() 
{
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}


String getUIDString() 
{
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) 
  {
    if (rfid.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  return uid;
}

bool isAuthorizedUID(const String &uid) 
{
  return (uid == student1_uid) || (uid == student2_uid);
}


bool readBalanceFromCard(uint32_t &balance) 
{
  balance = 0;

  MFRC522::StatusCode status = (MFRC522::StatusCode)rfid.PCD_Authenticate(
    MFRC522::PICC_CMD_MF_AUTH_KEY_A,
    BALANCE_BLOCK,
    &key,
    &(rfid.uid)
  );
  if (status != MFRC522::STATUS_OK) return false;

  byte buffer[18];
  byte size = sizeof(buffer);

  status = (MFRC522::StatusCode)rfid.MIFARE_Read(BALANCE_BLOCK, buffer, &size);
  if (status != MFRC522::STATUS_OK) return false;

  // first 4 bytes = balance (big-endian)
  balance = ((uint32_t)buffer[0] << 24) |
            ((uint32_t)buffer[1] << 16) |
            ((uint32_t)buffer[2] << 8)  |
            ((uint32_t)buffer[3]);

  return true;
}

bool writeBalanceToCard(uint32_t balance) 
{
  MFRC522::StatusCode status = (MFRC522::StatusCode)rfid.PCD_Authenticate(
    MFRC522::PICC_CMD_MF_AUTH_KEY_A,
    BALANCE_BLOCK,
    &key,
    &(rfid.uid)
  );
  if (status != MFRC522::STATUS_OK) return false;

  byte dataBlock[16] = {0};

  dataBlock[0] = (balance >> 24) & 0xFF;
  dataBlock[1] = (balance >> 16) & 0xFF;
  dataBlock[2] = (balance >> 8)  & 0xFF;
  dataBlock[3] = (balance)       & 0xFF;

  status = (MFRC522::StatusCode)rfid.MIFARE_Write(BALANCE_BLOCK, dataBlock, 16);
  return (status == MFRC522::STATUS_OK);
}


void setup() 
{
  Serial.begin(9600);

  SPI.begin();
  rfid.PCD_Init();

  lcd.init();
  lcd.backlight();

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);

  // Default Key A = FF FF FF FF FF FF
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;

  showHome();
}


void loop() 
{
  if (!waitingForCard) 
  {
    char k = keypad.getKey();
    if (k) 
    {
      // Switch actions
      if (k == 'D') { currentAction = ACT_PAY;      showHome(); return; }
      if (k == 'A') { currentAction = ACT_RECHARGE; showHome(); return; }
      if (k == 'B') { currentAction = ACT_SHOW_BAL; showHome(); waitingForCard = true; return; }
      if (k == 'C') { currentAction = ACT_RESET;    showHome(); waitingForCard = true; return; }

      if (currentAction == ACT_PAY || currentAction == ACT_RECHARGE) 
      {
        if (k >= '0' && k <= '9') 
        {
          if (amountStr.length() < 6) 
          {
            amountStr += k;
            lcd.setCursor(5, 1);
            lcd.print(amountStr + "     ");
          }
        } 
        else if (k == '*') 
        {
          amountStr = "";
          lcd.setCursor(5, 1);
          lcd.print("      ");
        } 
        else if (k == '#') 
        {
          if (amountStr.length() > 0) 
          {
            waitingForCard = true;
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print(currentAction == ACT_PAY ? "Tap Card to PAY" : "Tap Card TOPUP");
            lcd.setCursor(0, 1);
            lcd.print("Amt: " + amountStr);
          }
        }
      }
    }
    return;
  }

  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) 
  {
    String scannedUID = getUIDString();
    if (!isAuthorizedUID(scannedUID)) 
    {
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print("Unauthorized");
      lcd.setCursor(0, 1); lcd.print("Card Denied");
      setLEDs(false, true);   // RED only
      playFailureSound();
      delay(2000);
      stopRFIDSession();
      showHome();
      return;
    }

    uint32_t bal = 0;
    if (!readBalanceFromCard(bal)) 
    {
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print("Auth/Read Fail");
      lcd.setCursor(0, 1); lcd.print("Use MIFARE 1K");
      setLEDs(false, true);   // RED only
      playFailureSound();
      delay(1500);
      stopRFIDSession();
      showHome();
      return;
    }

    uint32_t amt = (uint32_t)parseAmount(amountStr);

    if (currentAction == ACT_SHOW_BAL) 
    {
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print("Balance:");
      lcd.setCursor(0, 1); lcd.print(String(bal));
      setLEDs(true, false);    // green only
      delay(2000);
      stopRFIDSession();
      showHome();
      return;
    }

    if (currentAction == ACT_RESET) 
    {
      blinkTimer = millis(); blinkState = false;

      unsigned long t0 = millis();
      while (millis() - t0 < 600) { blinkBothLEDsStep(); } // blink during reset

      bool ok = writeBalanceToCard(0);

      if (ok) 
      {
        lcd.clear();
        lcd.setCursor(0, 0); lcd.print("Reset Done");
        lcd.setCursor(0, 1); lcd.print("Bal: 0");
        setLEDs(true, true);   // both ON
        playSuccessSound();
      } 
      else 
      {
        lcd.clear();
        lcd.setCursor(0, 0); lcd.print("Reset Failed");
        lcd.setCursor(0, 1); lcd.print("Try again");
        setLEDs(false, true);  // red only
        playFailureSound();
      }

      delay(2000);
      stopRFIDSession();
      showHome();
      return;
    }

    if (currentAction == ACT_PAY) 
    {
      if (bal >= amt) 
      {
        uint32_t newBal = bal - amt;

        bool ok = writeBalanceToCard(newBal);
        if (ok) 
        {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print(scannedUID == student1_uid ? "Student-1 Pay" : "Student-2 Pay");
          lcd.setCursor(0, 1);
          lcd.print("Bal:" + String(newBal));

          setLEDs(true, false);     // GREEN only
          playSuccessSound();
        } 
        else 
        {
          lcd.clear();
          lcd.setCursor(0, 0); lcd.print("Write Failed");
          lcd.setCursor(0, 1); lcd.print("No Deduction");
          setLEDs(false, true);     // RED only
          playFailureSound();
        }

      } 
      else 
      {
        lcd.clear();
        lcd.setCursor(0, 0); lcd.print("Insufficient!");
        lcd.setCursor(0, 1); lcd.print("Bal:" + String(bal));
        setLEDs(false, true);       // RED only
        playFailureSound();
      }

      delay(2000);
      stopRFIDSession();
      showHome();
      return;
    }

    if (currentAction == ACT_RECHARGE) 
    {
      uint32_t newBal = bal + amt;

      blinkTimer = millis(); blinkState = false;

      unsigned long t0 = millis();
      while (millis() - t0 < 400) { blinkBothLEDsStep(); }

      bool ok = writeBalanceToCard(newBal);

      t0 = millis();
      while (millis() - t0 < 400) { blinkBothLEDsStep(); }

      if (ok) 
      {
        lcd.clear();
        lcd.setCursor(0, 0); lcd.print("Recharge Done");
        lcd.setCursor(0, 1); lcd.print("Bal:" + String(newBal));

        // recharge success => BOTH LEDs ON (as you wanted)
        setLEDs(true, true);
        playSuccessSound();
      } 
      else 
      {
        lcd.clear();
        lcd.setCursor(0, 0); lcd.print("Topup Failed");
        lcd.setCursor(0, 1); lcd.print("Try again");
        setLEDs(false, true);   // RED only
        playFailureSound();
      }

      delay(2000);
      stopRFIDSession();
      showHome();
      return;
    }

    // fallback
    stopRFIDSession();
    showHome();
  }
}
