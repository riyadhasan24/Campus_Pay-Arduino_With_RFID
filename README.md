# Campus_Pay-Arduino_With_RFID

This project is a Smart RFID payment and recharge system built using Arduino, an MFRC522 RFID reader, a Keypad, and an I2C LCD.
Each authorized RFID card stores its own balance inside the card memory (MIFARE Classic 1K).
The system supports payment, recharge (top-up), balance checking, and balance reset, with LED and buzzer feedback for user interaction.

Only authorized cards are accepted; any unknown card is automatically rejected for security.

▶️ How to Use
A → Recharge (Top-Up) Mode
B → Show Balance
C → Reset Balance
D → Payment Mode
# → Enter Button
* → Clear Button

🔌 Pin Configuration
RFID (MFRC522)
MFRC522 Pin	Arduino Pin
SDA (SS)	D10
RST	D9
MOSI	D11
MISO	D12
SCK	D13

LCD (I2C)
LCD Pin	Arduino Pin
SDA	A4
SCL	A5
Address	0x25

Other Components
Component	Arduino Pin
Buzzer	D6
Green LED	D7
Red LED	D8

Keypad Rows	A0–A3
Keypad Columns	D2–D5



📩 Contact, 
For help, suggestions, or collaboration, feel free to reach out. 
📧 Email: riyadhasan24a@gmail.com 
📱 WhatsApp: +88 01730 288553
