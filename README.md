# Arduino_alarm
Arduino Mega based alarm station
This is work in progress, should be finished until 02/2018. Commit's comments will show the progress.

The program works on Arduino Mega with 20x4 LCD display as a custom alarm station. The main interface is a keyboard and RFID
reader/writer RC522, 1.56 Mhz with a few cards/dongles. The station works with 2 motion sensors (PIR) and 4 contactrons (3 for windows, 
1 for doors). 

You can add new cards RFID cards to the system, remove cards from it, use keyboard to turn on/off the system if you will loose/forget the 
the card/dongle. Maximum number of cards: 20. 

The triggered alert will not rise any sound indication - the system is using A6 GSM module on the serial port to send SMS messages to the 
specified phone number (also stored in EEPROM, up to 20 numbers can be stored, the message is send to all of them). The numbers on the
can request the status report from the system using SMS message, the status will be send only to that number. 
