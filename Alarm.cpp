// Do not remove the include below
#include "Alarm.h"


#include "Timers.h"
#include <EEPROM.h>


#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal.h>

//display variable
LiquidCrystal lcd(8,9,10,11,12,13);


//Buzzer PIN A0
#define buzzer A0

//RST5220 pins
constexpr uint8_t RST_PIN = 5;     // Configurable, see typical pin layout above
constexpr uint8_t SS_1_PIN = 53;

//RFID reader object
MFRC522 mfrc522;

//variable to stop reading RFID for 2 seconds
bool switched = true;

//timer to reset RFID reading
Timers <1> budzik;

//RFID card structure
struct code {
	byte one;
	byte two;
	byte three;
	byte four;
};

//last read card
code read_card;


//Time interrupt funtion to reset RFID reader
void reset_switch();
//Simple buzzer bip function
void bip();
//Split read RFID card id into code structure
void dump_byte_array(byte *buffer, byte bufferSize);
//check if the card is in memory (is valid), return the position of the card number in memory
int compareCards(code card);
//Write the card to the momory at last position, return true if success
bool writeCard(code card);
//Remove card from memory
void deleteCard(code card);

void setup() {
	Serial.begin(9600); // Initialize serial communications with the PC
	while (!Serial);    // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)

	SPI.begin();        // Init SPI bus


	mfrc522.PCD_Init(SS_1_PIN, RST_PIN); // Init each MFRC522 card
	Serial.print(F("Reader "));
	Serial.print(F(": "));
	mfrc522.PCD_DumpVersionToSerial();
	pinMode(buzzer, OUTPUT);  //setup buzzer pin
	bip();
	budzik.attach(0, 2000, reset_switch);  //setup the timer for 2 sec for resetting the RFID reader

	lcd.begin(20,4);  //setup LCD as 20x4
	lcd.setCursor(0, 0);
	lcd.print("Test");
	lcd.setCursor(0, 1);
	lcd.print("hello World");


}

void loop() {
	budzik.process();  //check budzik timer
	if (mfrc522.PICC_IsNewCardPresent() && switched && mfrc522.PICC_ReadCardSerial() ) { //read RFID card
		bip();  //bip if read
		switched = false;  //disable next read for 2 sec
		dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size); //write card to local variable
		int wynik = compareCards(read_card);  //check if card is valid
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

	//write all the values to the scruct one by one
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
	EEPROM.get(0,number_of_cards);  //get the number of card stored from address 0 - int 2 bytes
	if (number_of_cards!=-1) { //  if vaulue == 255 (-1), there are no data written, abort procedure
		int i = 2;  //start from the second address (0+2 bytes for number_of_cards)
		code cards[number_of_cards];  //create an array of cards stored in EEPROM
		for (int z=0; z<number_of_cards; z++) {  //read each card from EEPROM into the array of cards
			for (int j=0; j<4; j++) {  //each card has 4 bytes
				byte read_byte;
				read_byte = EEPROM.read(i);
				switch (j) {  //decide witch byte to write
				case 0: cards[z].one = read_byte;
				break;
				case 1: cards[z].two = read_byte;
				break;
				case 2: cards[z].three = read_byte;
				break;
				case 3: cards[z].four = read_byte;
				break;
				}
				i++;  //next address
			}
		}
		for (int j=0; j<number_of_cards; j++) {  //for each element of cards table compare it with the current card, return the card position if any card is found
			if (cards[j].one == card.one && cards[j].two == card.two && cards[j].three == card.three && cards[j].four == card.four) {
				return_value = j+1;
			}
		}
	}
	return return_value;  //return the value
}

bool writeCard(code card) {
	if (compareCards(card)==0) {  //write the new card only if not already written
		int number_of_cards;
		EEPROM.get(0,number_of_cards);  //get the current number of cards in memory
		if (number_of_cards == -1) {
			number_of_cards = 1;  //if no card is stored, this is the first card, so number of cards will be 1 after the operation.
		} else {
			number_of_cards++;  //else increase the number of cards by 1
		}
		int address = (number_of_cards-1)*4+2; //set the next free adress for the new card
		EEPROM.put(0,number_of_cards);  //change the number of cards from now on
		EEPROM.write(address, card.one);  //write all bytes of new card
		EEPROM.write(address+1, card.two);
		EEPROM.write(address+2, card.three);
		EEPROM.write(address+3, card.four);
		Serial.println("card written");
		return true;  //card was written successfully
	} else {
		return false;  //card was not written (card already existed).
	}
}
