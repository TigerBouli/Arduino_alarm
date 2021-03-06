// Do not remove the include below

#include "Alarm.h"


#include "Timers.h"
#include <EEPROM.h>


#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal.h>
#include <Keypad.h>


//Three color LED
#define LED_R 22
#define LED_G 24
#define LED_B 23
//PIR sensors
#define PIR_1 48
#define PIR_2 49
#define OK_1  46
#define OK_2  47
#define OK_3  44
#define DZW_1 45

//Keyboard connection
const byte ROWS = 4;
const byte COLS = 4;
byte rowPins[ROWS] = {36, 38, 40, 42};
byte colPins[COLS] = {37, 39, 41, 43};

char keys[ROWS][COLS] = { //Keyboard mapping
		{'1','2','3','A'},
		{'4','5','6','B'},
		{'7','8','9','C'},
		{'*','0','#','D'}
};

Keypad keyboard = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );

//display variable
LiquidCrystal lcd(8,9,10,11,12,13);


//Buzzer PIN A0
#define buzzer A0

//RST5220 pins
constexpr uint8_t RST_PIN = 5;     // Configurable, see typical pin layout above
constexpr uint8_t SS_1_PIN = 53;
constexpr uint8_t IRQ_PIN = 2;

//RFID reader object
MFRC522 mfrc522(SS_1_PIN, RST_PIN);

MFRC522::MIFARE_Key key;

//timer to reset RFID reading and sensor sweep
Timers <4> budzik;


//RFID card structure
struct code {
	byte one;
	byte two;
	byte three;
	byte four;
};

//interrupt variable for RFID
volatile boolean bNewInt = false;

//register for interval mode setting
byte regVal = 0x7F;



//variable to stop reading RFID for 2 seconds
bool switched = true;
//variable to stop sensor reading
bool sensors_check = true;
//control variable if valid card was detected
bool valid_card  = false;
//variable triggering display refresh
bool refresh_display = true;
//time to arm the alarm
int time_to_arm = 21;
//LED variables: color and state
int LED_color;
bool led_state = 0;





//last read card
code read_card;

//last keyboard pressed (X if not pressed after processing)
char character = 0;

//windows and door states
bool ok1 = true;
bool ok2 = true;
bool ok3 = true;
bool door = true;

//motion sensor states
bool m1 = false;
bool m2 = false;

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

int state = 2;


//Time interrupt funtion to reset RFID reader
void reset_switch();
//Time interrupt to reset sensor sweep
void reset_sensors_read();
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
//Funtion to check if card is was detected
void check_card();
//Funtion to check all the sensors
void set_card();
//check all sensors in one pass
void check_sensors();
//technical funtion for RFID reader - activate
void activateRec(MFRC522 mfrc522);
//clear RFID interrupt
void clearInt(MFRC522 mfrc522);
//timer funtion for arming alarm
void count_down();
//LED management
void start_LED_blink();
void stop_LED_blink();
void LED_change_state();
//keyboard read
void read_char_from_keyboard();


//state process funtion to separate code
void process_state_1();
void process_state_2();
void process_state_3();
void process_state_4();
void process_state_5();
void process_state_6();
void process_state_7();
void process_state_8();
void process_state_9();






void setup() {
	Serial.begin(115200); // Initialize serial communications with the PC
	while (!Serial);    // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)

	SPI.begin();        // Init SPI bus


	mfrc522.PCD_Init(); // Init each MFRC522 card
	Serial.print(F("Reader "));
	Serial.print(F(": "));
	mfrc522.PCD_DumpVersionToSerial();


	pinMode(buzzer, OUTPUT);  //setup buzzer pin
	//Setup LED's
	pinMode(LED_R, OUTPUT);
	pinMode(LED_G, OUTPUT);
	pinMode(LED_B, OUTPUT);
	//Setup PIR sensors
	pinMode(PIR_1, INPUT);
	pinMode(PIR_2, INPUT);

	// TODO - Setup contactrons
	pinMode(OK_1, INPUT_PULLUP);
	pinMode(OK_2, INPUT_PULLUP);
	pinMode(OK_3, INPUT_PULLUP);
	pinMode(DZW_1, INPUT_PULLUP);

	// bip for main setup startup
	bip();
	budzik.attach(0, 2000, reset_switch);  //setup the timer for 2 sec for resetting the RFID reader
	budzik.attach(1, 50, reset_sensors_read); //setup timer for 50 ms sensor read
	budzik.attach(2, 0, count_down);
	budzik.attach(3, 0, LED_change_state);

	lcd.begin(20,4);  //setup LCD as 20x4

	//Set led status at start - all are off - HIGH state turns LED off
	digitalWrite(LED_R, HIGH);
	digitalWrite(LED_G, HIGH);
	digitalWrite(LED_B, HIGH);
	//Setup interrupt PIN for RFID
	pinMode(IRQ_PIN, INPUT_PULLUP);
	//attach interrupt to this pin
	attachInterrupt(digitalPinToInterrupt(IRQ_PIN), set_card, FALLING);

	//initial display
	display();

	//Initial setup of GPRS modem: TEXT mode for SMS
	Serial1.begin(115200);
	delay(500);
	Serial1.println("AT+CMGF=1");

	// setup RFID sensor to work with interrupts
	regVal = 0xA0; //rx irq
	mfrc522.PCD_WriteRegister(mfrc522.ComIEnReg, regVal);

	bNewInt = false; //interrupt flag

	//setup finished, alarm ready
	bip();
}

void loop() {
	budzik.process();  //check budzik timers
	check_card();
	check_sensors();
	read_char_from_keyboard();
	switch (state) {  //main loop for states
	case 1:
		process_state_1();
		display();
		break;
	case 2:
		process_state_2();
		display();
		break;
	case 3:
		process_state_3();
		display();
		break;
	case 4:
		process_state_4();
		display();
		break;
	case 5:
		process_state_5();
		display();
		break;
	case 6:
		process_state_6();
		display();
		break;
	case 7:
		process_state_7();
		display();
		break;
	case 8:
		process_state_8();
		display();
		break;
	case 9:
		process_state_9();
		display();
		break;
	}


}

void read_char_from_keyboard() {
	//get next key if last key is processed.
	if (!character) {
		character = keyboard.getKey();
	}
	if (character) {
		Serial.println(character);
		character = 0;
	}
}

void LED_change_state() {
	switch (LED_color) {  //decide witch color to blink
	case 1:
		if (led_state) {  //if was off: turn it on
			digitalWrite(LED_R, LOW);
			led_state = false;  //write previos state
		} else {  //else turn it off
			digitalWrite(LED_R, HIGH);
			led_state = true;
		}
		break;
	case 2:
		if (led_state) {
			digitalWrite(LED_G, LOW);
			led_state = false;
		} else {
			digitalWrite(LED_R, HIGH);
			led_state = true;
		}
		break;
	case 3:
		if (led_state) {
			digitalWrite(LED_B, LOW);
			led_state = false;
		} else {
			digitalWrite(LED_R, HIGH);
			led_state = true;
		}
		break;
	}
}

void start_LED_blink() {
	budzik.updateInterval(3,500);  //turn on timer
}

void stop_LED_blink() {
	budzik.updateInterval(3,0);  //turn off timer and all the LED's
	digitalWrite(LED_R, HIGH);
	digitalWrite(LED_G, HIGH);
	digitalWrite(LED_B, HIGH);
}

void process_state_1() {
	if (!ok1 || !ok2 || !ok3 ) {  //if some windows are opened, the alarm cannot be armed, state 2
		state = 2;
		refresh_display = true;
	}
	if (valid_card) {  //if card is read, start arming if possible
		state = 3;
		refresh_display = true;
		valid_card=false;
	}
}

void process_state_2() {
	if (ok1 && ok2 && ok3 ) {  //switch to state 1 if all windows are closed.
		state = 1;
		valid_card = false;
		refresh_display = true;
	}
}

void process_state_3() {
	if (time_to_arm == 21) {  //starting state: just came to tahe state, setup everything, 20 seconds timer, 1 second interval, start blinking RED LED.
		time_to_arm=20;
		budzik.updateInterval(2, 1000);
		LED_color = 1;
		start_LED_blink();
	} else if (time_to_arm == 0) { //timer expired, go to state 4, reset state 3 settings to initial values
		state = 4;
		refresh_display = 1;
		time_to_arm =21;
		stop_LED_blink();
		digitalWrite(LED_R, LOW);
		bip();
	} else {    //when arming watch for card - if present, abort arming and restore state 3 initial values
		if (valid_card) {
			state = 1;
			refresh_display = 1;
			valid_card = false;
			budzik.updateInterval(2, 0);
			time_to_arm = 21;
		}
	}
}

void process_state_4() {

}

void process_state_5() {

}

void process_state_6() {

}

void process_state_7() {

}

void process_state_8() {

}

void process_state_9() {

}

void count_down() {
	if (time_to_arm > 0) {
		time_to_arm--;
		refresh_display = 1;
	} else {
		budzik.updateInterval(2, 0);
	}
}


void check_sensors() {
	if (sensors_check) {
		//check PIR sensor 1
		if (digitalRead(PIR_1) == HIGH) {
			if (!m1) {
				refresh_display=true;
			}
			m1 = true;
		} else {
			if (m1) {
				refresh_display=true;
			}
			m1 = false;
		}
		//check PIR sensor 2
		if (digitalRead(PIR_2) == HIGH) {
			if (!m2) {
				refresh_display=true;
			}
			m2 = true;
		} else {
			if (m2) {
				refresh_display=true;
			}
			m2 = false;
		}
		//Windows 1
		if (digitalRead(OK_1) == LOW) {
			if (!ok1) {
				refresh_display=true;
			}
			ok1 = true;
		} else {
			if (ok1) {
				refresh_display=true;
			}
			ok1 = false;
		}
		//Window 2
		if (digitalRead(OK_2) == LOW) {
			if (!ok2) {
				refresh_display=true;
			}
			ok2 = true;
		} else {
			if (ok2) {
				refresh_display=true;
			}
			ok2 = false;
		}
		//Windows 3
		if (digitalRead(OK_3) == LOW) {
			if (!ok3) {
				refresh_display=true;
			}
			ok3 = true;
		} else {
			if (ok3) {
				refresh_display=true;
			}
			ok3 = false;
		}
		// Door
		if (digitalRead(DZW_1) == LOW) {
			if (!door) {
				refresh_display=true;
			}
			door = true;
		} else {
			if (door) {
				refresh_display=true;
			}
			door = false;
		}
		//turn off sensor checking for 50ms
		sensors_check = false;
	}
}

void set_card() {
	//if the timout passed: set the card present tag
	if (switched) {
		bNewInt = true;
		switched = false;
		mfrc522.PICC_ReadCardSerial();
	}
	//Always reset interrupt and RFID reader to receive the next one
	clearInt(mfrc522);
	mfrc522.PICC_HaltA();
}



void check_card() {
	if (bNewInt) {
		//read RFID card
		bip();  //bip if read
		switched = false;  //disable next read for 2 sec
		dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size); //write card to local variable
		if (compareCards(read_card) != 0) {
			valid_card = true;  //setup the variable if card is found
		}
		bNewInt = false;  //turn off card reading
	}
}

void reset_sensors_read() {
	sensors_check = true;  //just reset control variable
}

void bip () {
	digitalWrite(buzzer, 1);
	delay(50);
	digitalWrite(buzzer, 0);
}

void reset_switch() {
	switched = true;
	activateRec(mfrc522);  //restrigger the reader, important, does not work otherwise
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

		lcd.setCursor(0,3);
		lcd.print("CR1:");
		if (m1) {
			lcd.print("1 ");
		} else {
			lcd.print("0 ");
		}
		lcd.print("CR2:");
		if (m2) {
			lcd.print("1 ");
		} else {
			lcd.print("0 ");
		}
		lcd.print("DZ:");
		if (door) {
			lcd.print("Z ");
		} else {
			lcd.print("O ");
		}
	}
	refresh_display=false;

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

void activateRec(MFRC522 mfrc522) {
	mfrc522.PCD_WriteRegister(mfrc522.FIFODataReg, mfrc522.PICC_CMD_REQA);
	mfrc522.PCD_WriteRegister(mfrc522.CommandReg, mfrc522.PCD_Transceive);
	mfrc522.PCD_WriteRegister(mfrc522.BitFramingReg, 0x87);
}

void clearInt(MFRC522 mfrc522) {
	mfrc522.PCD_WriteRegister(mfrc522.ComIrqReg, 0x7F);
}

