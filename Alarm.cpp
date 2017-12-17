// Do not remove the include below
#include "Alarm.h"


#include "Timers.h"
#include <EEPROM.h>


#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal.h>


LiquidCrystal lcd(8,9,10,11,12,13);

#define buzzer A0

constexpr uint8_t RST_PIN = 5;     // Configurable, see typical pin layout above
constexpr uint8_t SS_1_PIN = 53;

MFRC522 mfrc522;
bool switched = true;
Timers <1> budzik;

struct code {
	byte one;
	byte two;
	byte three;
	byte four;
};

code read_card;



void reset_switch();
void bip();
void dump_byte_array(byte *buffer, byte bufferSize);
int compareCards(code card);
bool writeCard(code card);
void deleteCard(code card);

void setup() {
	Serial.begin(9600); // Initialize serial communications with the PC
	while (!Serial);    // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)

	SPI.begin();        // Init SPI bus


	mfrc522.PCD_Init(SS_1_PIN, RST_PIN); // Init each MFRC522 card
	Serial.print(F("Reader "));
	Serial.print(F(": "));
	mfrc522.PCD_DumpVersionToSerial();
	pinMode(buzzer, OUTPUT);
	digitalWrite(buzzer, 1);
	delay(50);
	digitalWrite(buzzer, 0);
	budzik.attach(0, 2000, reset_switch);
	lcd.begin(20,4);
	lcd.setCursor(0, 0);
	lcd.print("Test");
	lcd.setCursor(0, 1);
	lcd.print("hello World");


}

void loop() {
	budzik.process();
	if (mfrc522.PICC_IsNewCardPresent() && switched && mfrc522.PICC_ReadCardSerial() ) {
		bip();
		switched = false;
		dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
		int wynik = compareCards(read_card);
		Serial.println(wynik);
	}

}

void bip () {
	digitalWrite(buzzer, 1);
	delay(50);
	digitalWrite(buzzer, 0);
}

void reset_switch() {
	switched = true;
}

void dump_byte_array(byte *buffer, byte bufferSize) {
	for (byte i = 0; i < bufferSize; i++) {
		switch (i) {
		case 0: read_card.one = buffer[i];
		break;
		case 1: read_card.two = buffer[i];
		break;
		case 2: read_card.three = buffer[i];
		break;
		case 3: read_card.four = buffer[i];
		break;
		}
	}
}

int compareCards(code card) {
	int return_value = 0;
	int number_of_cards;
	EEPROM.get(0,number_of_cards);
	if (number_of_cards!=-1) {
		int i = 2;
		int z = 0;
		code cards[number_of_cards];
		for (int z=0; z<number_of_cards; z++) {
			for (int j=0; j<4; j++) {
				byte read_byte;
				read_byte = EEPROM.read(i);
				switch (j) {
				case 0: cards[z].one = read_byte;
				break;
				case 1: cards[z].two = read_byte;
				break;
				case 2: cards[z].three = read_byte;
				break;
				case 3: cards[z].four = read_byte;
				break;
				}
				i++;
			}
		}
		for (int j=0; j<number_of_cards; j++) {
			if (cards[j].one == card.one && cards[j].two == card.two && cards[j].three == card.three && cards[j].four == card.four) {
				return_value = j+1;
			}
		}
	}
	return return_value;
}

bool writeCard(code card) {
	if (compareCards(card)==0) {
		int number_of_cards;
		EEPROM.get(0,number_of_cards);
		if (number_of_cards == -1) {
			number_of_cards = 1;
		} else {
			number_of_cards++;
		}
		int address = (number_of_cards-1)*4+2;
		EEPROM.put(0,number_of_cards);
		EEPROM.write(address, card.one);
		EEPROM.write(address+1, card.two);
		EEPROM.write(address+2, card.three);
		EEPROM.write(address+3, card.four);
		Serial.println("card written");
		return true;
	} else {
		return false;
	}
}
