// Do not remove the include below

#include "Alarm.h"


#include "Timers.h"
#include <EEPROM.h>


#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal.h>

#define LED_R 22
#define LED_G 24
#define LED_B 23

//display variable
LiquidCrystal lcd(8,9,10,11,12,13);


//Buzzer PIN A0
#define buzzer A0

//RST5220 pins
constexpr uint8_t RST_PIN = 5;     // Configurable, see typical pin layout above
constexpr uint8_t SS_1_PIN = 53;

//RFID reader object
MFRC522 mfrc522;



//timer to reset RFID reading
Timers <2> budzik;


//RFID card structure
struct code {
	byte one;
	byte two;
	byte three;
	byte four;
};

//variable to stop reading RFID for 2 seconds
bool switched = true;
//SMS time control variable
bool sms_trigger = false;
//SMS sending state variable
int sms_state = 0;
//variable triggering display refresh
bool refresh_display = true;
//time to arm the alarm
int time_to_arm = 20;
//SMS message to send
String sms_message = "Wykryto karte";


//last read card
code read_card;

//windows states
bool ok1 = true;
bool ok2 = true;
bool ok3 = true;

/*
 *  Status of the alarm:
 *  1 - not armed, can be armed
 *  2 - not armed, cannot be armed (windows open)
 *  3 - arming
 *  4 - armed
 *  5 - armed, power lost
 *  6 - alert
 *  7 - door opened, waiting to disarm
 *  8 - add card
 *  9 - remove card
 *
 */

int state = 1;


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
//The procedure to display all data to the user
void display();
//SMS send
void SMS_send(String message);




void setup() {
	Serial.begin(115200); // Initialize serial communications with the PC
	while (!Serial);    // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)

	SPI.begin();        // Init SPI bus


	mfrc522.PCD_Init(SS_1_PIN, RST_PIN); // Init each MFRC522 card
	Serial.print(F("Reader "));
	Serial.print(F(": "));
	mfrc522.PCD_DumpVersionToSerial();


	pinMode(buzzer, OUTPUT);  //setup buzzer pin
	pinMode(LED_R, OUTPUT);
	pinMode(LED_G, OUTPUT);
	pinMode(LED_B, OUTPUT);


	bip();
	budzik.attach(0, 2000, reset_switch);  //setup the timer for 2 sec for resetting the RFID reader

	lcd.begin(20,4);  //setup LCD as 20x4
	lcd.setCursor(0, 0);
	lcd.print("Test");
	lcd.setCursor(0, 1);
	lcd.print("hello World");

	digitalWrite(LED_R, HIGH);
	digitalWrite(LED_G, HIGH);
	digitalWrite(LED_B, HIGH);

	display();

	Serial1.begin(115200);

}

void loop() {
	budzik.process();  //check budzik timer
//	SMS_tick();
		if (mfrc522.PICC_IsNewCardPresent() && switched && mfrc522.PICC_ReadCardSerial() ) { //read RFID card
		bip();  //bip if read
		switched = false;  //disable next read for 2 sec
		dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size); //write card to local variable
		//	deleteCard(read_card);
		int wynik = compareCards(read_card);  //check if card is valid
		//			writeCard(read_card);
	//	Serial.println(wynik);
		SMS_send("Wykryto karte");
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

void deleteCard(code card) {
	int position = compareCards(card);  // check if card is register and at witch position
	if (position != 0) {
		int number_of_cards;
		EEPROM.get(0,number_of_cards);
		if (position == number_of_cards) {  //if the last card - just zero last 4 bytes
			for (int i=((number_of_cards-1)*4+2); i< ((number_of_cards-1)*4+6); i++) {
				EEPROM.write(i,0);
				EEPROM.put(0, (number_of_cards-1));
			}
		} else {  //else - rewrite all cards down 4 bytes and delete last 4
			for (int i=((position)*4+2); i< ((number_of_cards-1)*4+6); i++) {
				byte read_byte;
				read_byte = EEPROM.read(i);
				EEPROM.write(i-4, read_byte);
			}
			for (int i=((number_of_cards-1)*4+2); i< ((number_of_cards-1)*4+6); i++) {
				EEPROM.write(i,0);
			}
			EEPROM.put(0, (number_of_cards-1));
		}
	}
}

void display() {
	if (refresh_display) {
		lcd.clear();
		lcd.setCursor(0, 0);
		switch (state) {
		case 1:
			lcd.print("Stan alarmu:");
			lcd.setCursor(0,1);
			lcd.print("Nie wlaczony");
			break;
		case 2:
			lcd.print("Stan alarmu:");
			lcd.setCursor(0,1);
			lcd.print("Nie wlaczony, blad");
			break;
		case 3:
			lcd.print("Stan alarmu:");
			lcd.setCursor(0,1);
			lcd.print("Wlaczanie, czas: ");
			lcd.print(time_to_arm);
			break;
		case 4:
			lcd.print("Stan alarmu:");
			lcd.setCursor(0,1);
			lcd.print("Monitorowanie");
			break;
		case 5:
			lcd.print("Stan alarmu:");
			lcd.setCursor(0,1);
			lcd.print("Monitorowanie, BP");
			break;
		case 6:
			lcd.print("Stan alarmu:");
			lcd.setCursor(0,1);
			lcd.print("Alarm!!!");
			break;
		case 7:
			lcd.print("Stan alarmu:");
			lcd.setCursor(0,1);
			lcd.print("Wylaczanie, czas: ");
			lcd.print(time_to_arm);
			break;
		}
		lcd.setCursor(0,2);
		lcd.print("OK1:");
		if (ok1) {
			lcd.print("Z ");
		} else {
			lcd.print("O ");
		}
		lcd.print("OK2:");
		if (ok2) {
			lcd.print("Z ");
		} else {
			lcd.print("O ");
		}

		lcd.print("OK3:");
		if (ok3) {
			lcd.print("Z ");
		} else {
			lcd.print("O ");
		}

	}
}

void SMS_send(String message) {
	String numbers[] = {"661880070"};
	Serial1.println("AT+CMGF=1");
	delay(500);
	String numer = "AT+CMGS=\""+numbers[0]+"\"";
	Serial1.println(numer);
	delay(500);
	Serial1.println(message);
	delay(500);
	Serial1.write(0x1a);

}


