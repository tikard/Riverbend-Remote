//#include <Arduino.h>
/*
 * HelTec Automation(TM) WIFI_LoRa_32 factory test code, witch includ
 * follow functions:
 * 
 * - Basic OLED function test;
 * 
 * - Basic serial port test(in baud rate 115200);
 * 
 * - LED blink test;
 * 
 * - WIFI connect and scan test;
 * 
 * - LoRa Ping-Pong test (DIO0 -- GPIO26 interrup check the new incoming messages);
 * 
 * - Timer test and some other Arduino basic functions.
 *
 * by Aaron.Lee from HelTec AutoMation, ChengDu, China
 * 
 * https://heltec.org
 *
 * this project also realess in GitHub:
 * https://github.com/HelTecAutomation/Heltec_ESP32
*/

#include "Arduino.h"
#include "heltec.h"
#include "WiFi.h"
#include "images.h"

#define BAND    915E6  //you can set band here directly,e.g. 868E6,915E6

String rssi = "RSSI --";
String packSize = "--";
String packet;

unsigned int counter = 0;

bool receiveflag = false; // software flag for LoRa receiver, received data makes it true.

long lastSendTime = 0;        // last send time
int interval = 1000;          // interval between sends
uint64_t chipid;

//************************************ Well Stuff **************************************************  
const int RELAY_ID = 99;
const int WELL_ID = 3;
const int RADIO_ID = 1;

const long HEARTBEAT_DELAY = 3;  // Heartbeat delay in seconds

long interval1 = 1000 * HEARTBEAT_DELAY;
long previousMillis = 0;        // will store last time LED was updated

enum msgType { MT_OK=0, MT_FAULTED=1};
enum mode { AUTO=0, MANUAL=1}; 

enum WELL_MSG_TYPE { WMT_STATUS=0, CMD=1, WELL_STATE=2, WELL_STATUS=3,
                     START_FILL=4, STOP_FILL=5, RESTART_WELL=6,
                     WELL_ERRORS=7, HEARTBEAT=8, INVALID_MSG};

enum states { IDLE=0, START=1, PWRWAIT= 2, FILLING=3,
              STOPPED=4, KILLPOWER=5, FILLCOMPLETED =6, FAULT=7 };

enum WELL_STATUS_MSGS { TANK_EMPTY=0, TANK_FULL=1, WELL_ERROR=2};  // only on Server

enum WELL_ERROR_MSGS { ERR_START_FAIL=0, ERR_FILL_PWR_FAIL=1, ERR_FILL_FAIL=2,
                       ERR_STOP_FAIL=3, ERR_NONE };  // Text lookup for error msgs

String Well_Error_Msg_Strings[] = {"START Failure", "Power Failure duing FILL",
                                   "Max Fill Time Exceeded","Unable to Stop Generator",
                                   "NO ERRORS"};

states testState = states::FILLCOMPLETED;
WELL_STATUS_MSGS testWellStatus = WELL_STATUS_MSGS::TANK_FULL;

struct well_msg  // Defines msgs sent to/from Wells
{
  int Radio_ID;
  int Well_ID;
  WELL_MSG_TYPE Msg_Type;
  int Msg_Value;
};

struct well_msg wellCMD;
struct well_msg wellMSG;

String RadioMsgRespone = "";

WELL_ERROR_MSGS well_error = WELL_ERROR_MSGS::ERR_FILL_FAIL; 

//*******************  Well Functions
// Helpers to send output to right port
void debugPrint(String s){ Serial.print(s);}
void debugPrint(int s){ Serial.print(s);}
void debugPrintln(String s){ Serial.println(s);}
void debugPrintln(int s){ Serial.println(s);}

void radioPrint(String s){ Serial1.print(s);}  // change to serial[x]

String printWellErrorMsgs(int m){
  return Well_Error_Msg_Strings[m];
}

String printWellStatusCodes(int c){
  // TANK_EMPTY=0, TANK_FULL=1, WELL_ERROR=2
  switch (c)
  {
  case TANK_EMPTY:
    return "TANK_EMPTY";
    break;
  case TANK_FULL:
    return "TANK_EMPTY";
    break;
  case WELL_ERROR:
    return "WELL_ERROR";
    break;
  default:
    return "";
    break;
  }
}

String printStates(int s){
 // IDLE=0, START=1, PWRWAIT= 2, FILLING=3, STOPPED=4, KILLPOWER=5, FILLCOMPLETED =6, FAULT=7 
 switch (s)
 {
 case IDLE:
   return "IDLE";
   break;
 case START:
   return "START";
   break;
 case PWRWAIT:
   return "PWRWAIT";
   break;
 case FILLING:
   return "FILLING";
   break;
 case STOPPED:
   return "STOPPED";
   break;
 case KILLPOWER:
   return "KILLPOWER";
   break;
 case FILLCOMPLETED:
   return "FILLCOMPLETED";
   break;
 case FAULT:
   return "FAULT";
   break;
 default:
  return "";
   break;
 }
}

String printStatusMsg(WELL_STATUS_MSGS m){
  switch (m)
  {
  case TANK_EMPTY:
    return "TANK_EMPTY";
    break;
  case TANK_FULL:
    return "TANK_FULL";
    break;
  case WELL_ERROR:
    return "WELL_ERROR";
    break;
  default:
  return "";
    break;
  }
}

String printWellMsgType(WELL_MSG_TYPE wmt){
  switch (wmt)
  {
  case WELL_MSG_TYPE::WMT_STATUS :
    return "STATUS";
    break;
  case WELL_MSG_TYPE::CMD :
    return "CMD";
    break;
  case WELL_MSG_TYPE::WELL_STATE :
    return "WELL_STATE";
    break;
  case WELL_MSG_TYPE::WELL_STATUS :
    return "WELL_STATUS";
    break;
  case WELL_MSG_TYPE::START_FILL :
    return "START_FILL";
    break;
  case WELL_MSG_TYPE::STOP_FILL :
    return "STOP_FILL";
    break;
  case WELL_MSG_TYPE::RESTART_WELL :
    return "RESTART_WELL";
    break;
  case WELL_MSG_TYPE::WELL_ERRORS :
    return "WELL_ERRORS";
    break;
  default:
    return "";
    break;
  }
}

void sendRadioMsg(String msg){
  radioPrint(msg +'\n');
}

void send(String msg)
{
    LoRa.beginPacket();
    LoRa.print(msg);
    LoRa.print(counter++);
    LoRa.endPacket();
}

void sendHeartBeat(){
  debugPrintln("Sending out HEARTBEAT via RADIO");
  debugPrintln("============================");
  debugPrint("Radio ID  = ");
  debugPrintln(RELAY_ID);  //**********  mark for RELAY station
  debugPrint("Well ID   = ");
  debugPrintln(WELL_ID);  
  debugPrint("Msg Type  = ");
  debugPrint(WELL_MSG_TYPE::HEARTBEAT );
  debugPrintln(" (Heartbeat)");
  RadioMsgRespone = "";
  RadioMsgRespone = RadioMsgRespone + String(RELAY_ID) + " ";
  RadioMsgRespone = RadioMsgRespone + String(WELL_ID) + " ";
  RadioMsgRespone = RadioMsgRespone  + String(WELL_MSG_TYPE::HEARTBEAT) + " ";
  RadioMsgRespone = RadioMsgRespone + String(previousMillis);
  //sendRadioMsg(RadioMsgRespone);
	send(RadioMsgRespone);
  debugPrintln("============================");
}

void sendStatus(){
  RadioMsgRespone = "";
  debugPrintln("Sending out STATUS via RADIO");
  debugPrintln("============================");
  debugPrint("Radio ID  = ");
  debugPrintln(RELAY_ID);  //**********  mark for RELAY station
  RadioMsgRespone = RadioMsgRespone + String(RELAY_ID) + " ";


  debugPrint("Well ID   = ");
  debugPrintln(WELL_ID);  // send this wells ID

  RadioMsgRespone = RadioMsgRespone + String(WELL_ID) + " ";

  debugPrint("Msg Type  = ");
  debugPrintln(wellMSG.Msg_Type);
  RadioMsgRespone = RadioMsgRespone + wellMSG.Msg_Type + " ";

  if(wellMSG.Msg_Type == WELL_MSG_TYPE::WELL_STATE){
    debugPrint("MSG       = (");
    debugPrint(wellMSG.Msg_Value);
    debugPrint(") ");
    debugPrintln(printStates((wellMSG.Msg_Value)));
    RadioMsgRespone = RadioMsgRespone + String(wellMSG.Msg_Value);
  }

  if(wellMSG.Msg_Type == WELL_MSG_TYPE::WELL_STATUS){
    debugPrint("MSG       = (");
    debugPrint(wellMSG.Msg_Value);
    debugPrint(") ");
    debugPrintln(printWellStatusCodes((wellMSG.Msg_Value)));
    RadioMsgRespone = RadioMsgRespone + String(wellMSG.Msg_Value);
  }
  
  if(wellMSG.Msg_Type == WELL_MSG_TYPE::WELL_ERRORS){
    debugPrint("MSG       = (");
    debugPrint(wellMSG.Msg_Value);
    debugPrint(") ");
    debugPrintln(printWellErrorMsgs((wellMSG.Msg_Value)));
    RadioMsgRespone = RadioMsgRespone + String(wellMSG.Msg_Value);
  }
  debugPrintln("============================");
  sendRadioMsg(RadioMsgRespone);
  debugPrintln("============================");
}

void processMsg(well_msg wm){
  // Process a Well CMD Msg
    debugPrintln("Recieved a MSG via Radio");
    wellMSG.Msg_Value = wellCMD.Msg_Value;

    debugPrintln("Processing " + printWellMsgType(wm.Msg_Type) + " request.");

    switch (wm.Msg_Type)
    {
      case WMT_STATUS:
       debugPrintln("Send Handshake back");
       break;
      case CMD:
        debugPrintln("Client does not COMMAND is ignored.");
        break;
      case WELL_STATE:
          wellMSG.Msg_Type = WELL_MSG_TYPE::WELL_STATE;
          wellMSG.Msg_Value = testState;
          sendStatus();
        break;
      case WELL_STATUS:
          wellMSG.Msg_Type = WELL_MSG_TYPE::WELL_STATUS;
          wellMSG.Msg_Value = testWellStatus;
          sendStatus();
        break;
      case START_FILL:
        debugPrintln("START FILL Request");
        break;
      case STOP_FILL:
        debugPrintln("STOP FILL Request");
        break;
      case RESTART_WELL:
        debugPrintln("RESTART WELL Request");
        break;      
      case WELL_ERRORS:
        debugPrintln("WELL ERRORS Request");
        wellMSG.Msg_Type = WELL_MSG_TYPE::WELL_ERRORS;
        wellMSG.Msg_Value = well_error;
        sendStatus();
        break;      
      case HEARTBEAT:
        debugPrintln("WELL Heartbeat Response");
        wellMSG.Msg_Type = WELL_MSG_TYPE::HEARTBEAT;
        wellMSG.Msg_Value = 0;
        sendHeartBeat();
        break;      
      default:
        debugPrint("(");
        debugPrint(wellCMD.Msg_Type);
        debugPrint(") ");
        debugPrintln("Invalid MSG TYPE Recieved");
        break;
    }
}

void checkForData(){
  // reads in the 4 part Well CMD msg
  // if there's any serial available, read it:
  int temp;
  while (Serial1.available() > 0) {
    
    wellCMD.Radio_ID = Serial1.parseInt();
    wellCMD.Well_ID = Serial1.parseInt();

    temp = Serial1.parseInt();
    switch (temp)
    {
    case WELL_MSG_TYPE::WMT_STATUS:
      wellCMD.Msg_Type = WELL_MSG_TYPE::WMT_STATUS;
      break;
    case WELL_MSG_TYPE::CMD   :
      wellCMD.Msg_Type = WELL_MSG_TYPE::CMD;
      break;
    case  WELL_MSG_TYPE::WELL_STATE :
      wellCMD.Msg_Type = WELL_MSG_TYPE::WELL_STATE;
      break;
    case WELL_MSG_TYPE::WELL_STATUS  :
      wellCMD.Msg_Type = WELL_MSG_TYPE::WELL_STATUS;
      break;
    case WELL_MSG_TYPE::START_FILL  :
      wellCMD.Msg_Type = WELL_MSG_TYPE::START_FILL;
      break;
    case WELL_MSG_TYPE::STOP_FILL  :
      wellCMD.Msg_Type = WELL_MSG_TYPE::STOP_FILL;
      break;
    case WELL_MSG_TYPE::RESTART_WELL  :
      wellCMD.Msg_Type = WELL_MSG_TYPE::RESTART_WELL;
      break;
    case WELL_MSG_TYPE::WELL_ERRORS  :
      wellCMD.Msg_Type = WELL_MSG_TYPE::WELL_ERRORS;
      break;
    case WELL_MSG_TYPE::HEARTBEAT  :
      wellCMD.Msg_Type = WELL_MSG_TYPE::HEARTBEAT;
      break;
    default:
      wellCMD.Msg_Type = INVALID_MSG;  // invalid command 
      wellCMD.Msg_Type = (WELL_MSG_TYPE)temp;        // invalid command 
      break;
    }
    
    wellCMD.Msg_Value = Serial1.parseInt();

    // look for the newline. That's the end of MSG:
    if (Serial1.read() == '\n') {
      // ******************** check to see if this msg is for us
      if(wellCMD.Well_ID == WELL_ID)  // my Well ID
        processMsg(wellCMD);
      else
        debugPrintln("Msg not for this well");
    }
  }
}


//*******************  Well Functions


void logo(){
	Heltec.display -> clear();
	Heltec.display -> drawXbm(0,5,logo_width,logo_height,(const unsigned char *)logo_bits);
	Heltec.display -> display();
}

void WIFISetUp(void)
{
	// Set WiFi to station mode and disconnect from an AP if it was previously connected
	WiFi.disconnect(true);
	delay(100);
	WiFi.mode(WIFI_STA);
	WiFi.setAutoConnect(true);
	WiFi.begin("Firehole2-2G","cad123dis");//fill in "Your WiFi SSID","Your Password"
	delay(100);

	byte count = 0;
//	while(WiFi.status() != WL_CONNECTED && count < 100)
	while(WiFi.status() != WL_CONNECTED )
	{
		count ++;
		delay(500);
		Heltec.display -> drawString(0, 0, "Connecting...");
		Heltec.display -> display();
    //Serial.print("Trying to connect ");
    //Serial.println(count);
	}

	Heltec.display -> clear();
	if(WiFi.status() == WL_CONNECTED)
	{
		Heltec.display -> drawString(0, 0, "Connecting...OK.");
		Heltec.display -> display();
//		delay(500);
	}
	else
	{
		Heltec.display -> clear();
		Heltec.display -> drawString(0, 0, "Connecting...Failed");
		Heltec.display -> display();
		//while(1);
	}
	Heltec.display -> drawString(0, 10, "WIFI Setup done");
	Heltec.display -> display();
	delay(500);
}

void WIFIScan(unsigned int value)
{
	unsigned int i;
    WiFi.mode(WIFI_STA);

	for(i=0;i<value;i++)
	{
		Heltec.display -> drawString(0, 20, "Scan start...");
		Heltec.display -> display();

		int n = WiFi.scanNetworks();
		Heltec.display -> drawString(0, 30, "Scan done");
		Heltec.display -> display();
		delay(500);
		Heltec.display -> clear();

		if (n == 0)
		{
			Heltec.display -> clear();
			Heltec.display -> drawString(0, 0, "no network found");
			Heltec.display -> display();
			//while(1);
		}
		else
		{
			Heltec.display -> drawString(0, 0, (String)n);
			Heltec.display -> drawString(14, 0, "networks found:");
			Heltec.display -> display();
			delay(500);

			for (int i = 0; i < n; ++i) {
			// Print SSID and RSSI for each network found
				Heltec.display -> drawString(0, (i+1)*9,(String)(i + 1));
				Heltec.display -> drawString(6, (i+1)*9, ":");
				Heltec.display -> drawString(12,(i+1)*9, (String)(WiFi.SSID(i)));
				Heltec.display -> drawString(90,(i+1)*9, " (");
				Heltec.display -> drawString(98,(i+1)*9, (String)(WiFi.RSSI(i)));
				Heltec.display -> drawString(114,(i+1)*9, ")");
				//            display.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN)?" ":"*");
				delay(10);
			}
		}

		Heltec.display -> display();
		delay(800);
		Heltec.display -> clear();
	}
}

bool resendflag=false;
bool deepsleepflag=false;
void interrupt_GPIO0()
{
  delay(10);
  if(digitalRead(0)==0)
  {
      if(digitalRead(LED)==LOW)
      {resendflag=true;}
      else
      {
        deepsleepflag=true;
      }     
  }
}

void send()
{
    LoRa.beginPacket();
    LoRa.print("hello ");
    LoRa.print(counter++);
    LoRa.endPacket();
}


void displaySendReceive()
{
    Heltec.display -> drawString(0, 50, "Packet " + (String)(counter-1) + " sent done");
    Heltec.display -> drawString(0, 0, "Received Size  " + packSize + " packages:");
    Heltec.display -> drawString(0, 10, packet);
    Heltec.display -> drawString(0, 20, "With " + rssi + "db");
    Heltec.display -> display();
    delay(100);
    Heltec.display -> clear();
}


void onReceive(int packetSize)//LoRa receiver interrupt service
{
	//if (packetSize == 0) return;

	  packet = "";
    packSize = String(packetSize,DEC);

    while (LoRa.available())
    {
		packet += (char) LoRa.read();
    }

    Serial.println(packet);
    rssi = "RSSI: " + String(LoRa.packetRssi(), DEC);    
    receiveflag = true;    
}

void setup()
{
  wellMSG.Radio_ID = 5;
  wellMSG.Well_ID = 2;
  wellMSG.Msg_Type = WELL_MSG_TYPE::WMT_STATUS;
	
	Heltec.begin(true /*DisplayEnable Enable*/, true /*LoRa Enable*/, true /*Serial Enable*/, false /*LoRa use PABOOST*/, BAND /*LoRa RF working band*/);

	logo();
	delay(300);
	Heltec.display -> clear();

	WIFISetUp();
	WiFi.disconnect(); //WIFI
	WiFi.mode(WIFI_STA);
	delay(100);

	WIFIScan(1);

  debugPrintln(WiFi.localIP());

	chipid=ESP.getEfuseMac();//The chip ID is essentially its MAC address(length: 6 bytes).
	Serial.printf("ESP32ChipID=%04X",(uint16_t)(chipid>>32));//print High 2 bytes
	Serial.printf("%08X\n",(uint32_t)chipid);//print Low 4bytes.

  attachInterrupt(0,interrupt_GPIO0,FALLING);
  
	LoRa.onReceive(onReceive);
    send();
  LoRa.receive();
  displaySendReceive();
}



void loop()
{

unsigned long currentMillis = millis();

 if(deepsleepflag)
 {
  LoRa.end();
  LoRa.sleep();
  delay(100);
  pinMode(4,INPUT);
  pinMode(5,INPUT);
  pinMode(14,INPUT);
  pinMode(15,INPUT);
  pinMode(16,INPUT);
  pinMode(17,INPUT);
  pinMode(18,INPUT);
  pinMode(19,INPUT);
  pinMode(26,INPUT);
  pinMode(27,INPUT);
  digitalWrite(Vext,HIGH);
  delay(2);
  esp_deep_sleep_start();
 }

 if(resendflag)
 {
   resendflag=false;
   send();      
   LoRa.receive();
   displaySendReceive();
 }

 /*if(receiveflag)
 {
    digitalWrite(25,HIGH);
    displaySendReceive();
    delay(2000);
    receiveflag = false;  
    //send();
    if(long(currentMillis - previousMillis) > interval1) {
      // save the last time you blinked the LED 
      previousMillis = currentMillis;  
    }
    sendHeartBeat();
    LoRa.receive();
    displaySendReceive();
  }
*/
 
  if(long(currentMillis - previousMillis) > interval1) {
    // save the last time you blinked the LED 
    previousMillis = currentMillis;   
  
    digitalWrite(25,HIGH);
    displaySendReceive();
    delay(2000);
    sendHeartBeat();
    //LoRa.receive();
    //displaySendReceive();
  }

}

