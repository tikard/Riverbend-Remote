/*********
  Relay Server part of WELL SYSTEM

  Use WebSockets to talk to a control web page
  Uses LORA to xmit and recieve WELL msgs

  Initial commit to Github
  Version 0.1  Full working version thru Well_Receiver to Riverbend Well Unit  11/19/21
  version 0.2  Full working with added Display Unit which sends well commands thru RELAY  11/28/21



  Note:   Jumper Pin 39 High = RELAY STATION MODE or  Low = DiSPLAY STATION MODE
          Jumper Pin 38 HIGH = WiFi Station       or  Low = WiFi Access Point (WellDisplay SSID No PW)


*********/

// Import required libraries
#include "Arduino.h"
#include "heltec.h"
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "images.h"
#include "SPIFFS.h"
#include "ArduinoJson-v6.18.5.h"
#include <stdio.h>
#include <stdlib.h>
//#include <sqlite3.h>
#include "SPI.h"
//#include "RTClib.h"
#include "FS.h"
#include "SD.h"


boolean RELAYROLE       = true;   // false is DISPLAY mode
boolean WIFIMODESTATION = true;   // false is DISPLAY mode

const int RELAY_ID = 99;
const int RELAY_RADIO_ID = 99;
const int RELAY_WELL_ID = 99;
const int DISPLAY_RADIO_ID = 100;
const int WEB_RADIO_ID = 101;

#define ROLE_GPIO 39
#define WIFI_GPIO 38

#define ONLINE  0
#define OFFLINE 1

//RTC_DS1307 rtc;  // Real Time Clock
SPIClass spi1;

#define RELAYLOGFILENAME "/relaylog.txt"
#define BAND    915E6  //you can set band here directly,e.g. 868E6,915E6

String rssi = "RSSI --";
String packSize = "--";
String packet;
String lastpacket;

unsigned int sent_msg_counter = 0;

bool receiveflag = false; // software flag for LoRa receiver, received data makes it true.

long lastSendTime = 0;        // last send time
int interval = 1000;          // interval between sends
uint64_t chipid;

// Replace with your network credentials
const char* ssid = "firehole";
const char* password = "cad123dis";

bool ledState = 0;
const int ledPin = 2;

unsigned long currentMillis;

bool toggle = false;

//************************************ Well Stuff **************************************************  



const long HEARTBEAT_DELAY = 2;  // Heartbeat delay in minutes

long keepaliveTimer = 25000;
long TIMER1 = 60000 * HEARTBEAT_DELAY;
long previousMillis = 0;         // Used in peridic timers in main loop
long previousMillis2 = 0;        // Used in peridic timers in main loop

const int maxHeartbeatMisses = 4;  // exceed (HEARTBEAT_DELAY)*(maxHeartbeatMisses) minutes
                                   // this should be greater than the well Heartbeat rate 
                                   // example well heartbeat 6 mins,  this set to 8  (2*4)

typedef struct
  {
      int wellID;
      int lastMillisCount;
      int missCount;
  }  heartbeat_trackin;

heartbeat_trackin heartbeatTracking[RELAY_ID];  // Used to track heartbeats from wells

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

StaticJsonDocument<256> doc;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");


// ******************  SQLite Routines
/*
const char* data = "Callback function called";
static int callback(void *data, int argc, char **argv, char **azColName){
   int i;
   Serial.printf("%s: ", (const char*)data);
   for (i = 0; i<argc; i++){
       Serial.printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
   }
   Serial.printf("\n");
   return 0;
}

int openDb(const char *filename, sqlite3 **db) {
   int rc = sqlite3_open(filename, db);
   if (rc) {
       Serial.printf("Can't open database: %s\n", sqlite3_errmsg(*db));
       return rc;
   } else {
       Serial.printf("Opened database successfully\n");
   }
   return rc;
}

char *zErrMsg = 0;
int db_exec(sqlite3 *db, const char *sql) {
   Serial.println(sql);
   long start = micros();
   int rc = sqlite3_exec(db, sql, callback, (void*)data, &zErrMsg);
   if (rc != SQLITE_OK) {
       Serial.printf("SQL error: %s\n", zErrMsg);
       sqlite3_free(zErrMsg);
   } else {
       Serial.printf("Operation done successfully\n");
   }
   Serial.print(F("Time taken:"));
   Serial.println(micros()-start);
   return rc;
}
*/


//*******************  Well Functions
// Helpers to send debug output to right port  change serial port in 1 spot
void debugPrint(String s){ Serial.print(s);}
void debugPrint(int s){ Serial.print(s);}
void debugPrintln(String s){ Serial.println(s);}
void debugPrintln(int s){ Serial.println(s);}

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
  case WELL_MSG_TYPE::HEARTBEAT :
    return "HEARTBEAT";
    break;
  case WELL_MSG_TYPE::INVALID_MSG :
    return "INVALID_MSG";
    break;
  default:
    return "";
    break;
  }
}

void displaySendReceive()
{
  Heltec.display -> clear();
  Heltec.display -> display();

  Heltec.display -> drawString(0, 0, "Last MSG Received:");
  //Heltec.display -> drawString(0, 0, "Received Size  " + packSize + " packages:");
  Heltec.display -> drawString(0, 10, packet);
  Heltec.display -> drawString(0, 20,  printWellMsgType(wellMSG.Msg_Type));
  Heltec.display -> drawString(0, 40, "With " + rssi + "db");
  Heltec.display -> drawString(0, 50, "Sent " + (String)(sent_msg_counter-1) + " requests");
  Heltec.display -> display();
  delay(50);
  Heltec.display -> clear();
}


void send(String msg, String debugMsg)
{
    LoRa.beginPacket();
    LoRa.print(msg); 
    LoRa.endPacket();
    delay(50);
    LoRa.receive();
    debugPrintln(debugMsg);
    sent_msg_counter++;
    displaySendReceive();
}


void sendrequestLORA(String debugMsg){  // send out a request to all wells via LORA 
  String requestBody;
  serializeJson(doc, requestBody);   // data is in the doc
  //debugPrintln(requestBody);
  send(requestBody, debugMsg); 
}

void sendKeepAlive(){
  String keepaliveBody;
  doc.clear();
  doc["KA"] =  1;
  serializeJson(doc, keepaliveBody);
  //Serial.println("Sending out keepalive msg via websocket");
  //Serial.println(keepaliveBody);
  ws.textAll(keepaliveBody);
  doc.clear();
}


void notifyClients() {
  String requestBody;
    
  doc["RID"]  = RELAY_ID;          // Add values in the document
  doc["WID"]   = wellMSG.Well_ID;   // Add values in the document
  doc["MT"]  = wellMSG.Msg_Type;  // Add values in the document
  doc["MV"] = wellMSG.Msg_Value; // Add values in the document
  
  serializeJson(doc, requestBody);

  debugPrintln("json :" + requestBody);
  ws.textAll(requestBody);

}

void pushtoDisplayUnits(){  // Send out whole msg to Display units which are listening
  String requestBody = "";
  doc.clear();
  doc["RID"]  = DISPLAY_RADIO_ID;          // Add values in the document
  doc["WID"]   = wellMSG.Well_ID;   // Add values in the document
  doc["MT"]  = wellMSG.Msg_Type;  // Add values in the document
  doc["MV"] = wellMSG.Msg_Value; // Add values in the document
  serializeJson(doc, requestBody);

  debugPrintln("Sending MSG via Lora to Display Station");
  debugPrintln("Echo to Display unit = " + requestBody);

  send(requestBody, "Pushing LORA  MSG to DISPLAY unit");
}

void sendHeartbeatFailure(int wellid){
  doc.clear();
  doc["ST"] = OFFLINE;  // Offline
  notifyClients();

  delay(100);  // wait a bit before sending out to Display Units

}

//void appendFile(fs::FS &fs, const char * path, const char * message){
void appendFile(fs::FS &fs, const char * path, String message){
    Serial.printf("Appending to file: %s\n", path);

    File file = fs.open(path, FILE_APPEND);
    if(!file){
        Serial.println("Failed to open file for appending");
        return;
    }
    if(file.println(message)){
        //Serial.println("Message appended");
    } else {
        Serial.println("Append failed");
    }
    file.close();
}


void writeLogMessage(String msg){  // appends msg to the log file
  //appendFile(SD, RELAYLOGFILENAME, msg);  

  File file = SD.open(RELAYLOGFILENAME, FILE_APPEND);
  if(!file){
      Serial.println("Failed to open file for appending");
      return;
  }
  if(file.println(msg)){
      //Serial.println("Message appended");
  } else {
      Serial.println("Append failed");
  }
  file.close();

}

// *******************  Web Socket routines

void serverRequest( String data) {

  DeserializationError error = deserializeJson(doc, data);
  if (error) {
      debugPrintln(error.c_str()); 
      return;
    }

  const char* keepalive = doc["KA"];
  if (keepalive) {
    return;
  }
  else{
    // change RADIO ID if we are DISPLAY 
    if(RELAYROLE== false){  // DISPLAY will change this request to DISPLAY_RADIO_ID
      doc["RID"] = DISPLAY_RADIO_ID;
    }

    sendrequestLORA("Sent LORA From WebSocket Request");  // Send out request may go to RELAY or WELL
  }
}

void serverRequest(void *arg, uint8_t *data, size_t len) {

  //String str = (char*)data;
  //Serial.println(str);
  doc.clear();

  DeserializationError error = deserializeJson(doc, data);
  if (error) {
      debugPrintln(error.c_str()); 
      return;
    }

  //int reqradioID = doc["RID"];
  //int reqwellID  = doc["WID"];
  //int reqreqType = doc["MT"];
  //int reqvalue   = doc["MV"];

  //debugPrintln("Request from RELAY");
  //Serial.println("Radio ID =" + reqradioID);
  //Serial.println("Well ID =" + reqwellID);
  //Serial.println("Req type =" + reqreqType);
  //Serial.println("Req value =" + reqvalue);
  
  const char* keepalive = doc["KA"];
  if (keepalive) {
    return;
  }
  else{  
    if(RELAYROLE== true){  // RELAY will change this request to DISPLAY_RADIO_ID
      doc["RID"] = WEB_RADIO_ID;   // this will signal a command from WebSocket
      sendrequestLORA("Sending LORA data to WELLS");  // Send out request to the WELLS
    }else{
      sendrequestLORA("ServerRequest sent LORA Msg.... ");  // Send out request to the WELLS
    }
  }
}

// recieved data on websocket
void wsDataEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {

  //debugPrintln("Got Websocket request");
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n\r", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n\r", client->id());
      break;
    case WS_EVT_DATA:
      //debugPrintln("Data on WEBSOCKET")  ;
      serverRequest(arg, data, len);     
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(wsDataEvent);
  server.addHandler(&ws);
}

String processor(const String& var){
  //debugPrintln(var);
  if(var == "STATE"){
    if (ledState){
      return "ON";
    }
    else{
      return "OFF";
    }
  }return " ";
}


// ************ LORA METHODS

void WIFISetUp(void)
{
	// Set WiFi to station mode and disconnect from an AP if it was previously connected
	WiFi.disconnect(true);
	delay(100);
	WiFi.mode(WIFI_STA);
	WiFi.setAutoConnect(true);
	WiFi.begin("firehole","cad123dis");//fill in "Your WiFi SSID","Your Password"
	//WiFi.begin("Firehole2-2G","cad123dis");//fill in "Your WiFi SSID","Your Password"
	delay(100);

	byte count = 0;
//	while(WiFi.status() != WL_CONNECTED && count < 100)
	while(WiFi.status() != WL_CONNECTED )
	{
		count ++;
		delay(500);
		Heltec.display -> drawString(0, 0, "Connecting...");
		Heltec.display -> display();
    debugPrintln("Trying to connect ");
    debugPrintln(count);
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

void logo(){
	Heltec.display -> clear();
	Heltec.display -> drawXbm(0,5,logo_width,logo_height,(const unsigned char *)logo_bits);
	Heltec.display -> display();
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

void onLORAReceive(int packetSize)//LoRa receiver interrupt service
{
  //if (packetSize == 0) return;
  packet = "";
  packSize = String(packetSize,DEC);

  //Serial.print(" In onRecieved Interupt handler Packet size = ");
  //Serial.println(packetSize);
  
  while (LoRa.available()){
    packet += (char) LoRa.read();  // READ in all the data
  }

  lastpacket = packet;  // save packet for processing

  //debugPrintln("On Recieved " + packet);
  //debugPrintln("On Recieved " + lastpacket);

  rssi = "RSSI: " + String(LoRa.packetRssi(), DEC);    
  receiveflag = true;    
}

void writeFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }
    if(file.print(message)){
        Serial.println("File written");
    } else {
        Serial.println("Write failed");
    }
    file.close();
}


void processLORAMsg(String msg){  // process a JSON msg from a well station LORA 
  doc.clear();
  DeserializationError error = deserializeJson(doc, msg);
  if (error) {
    debugPrintln("Error decoded JSON msg from wells" + msg);
    debugPrintln(error.c_str()); 
    return;
  }

  Serial.println("In processLORAMsg: " + msg);

  wellMSG.Radio_ID  = doc["RID"];
  wellMSG.Well_ID   = doc["WID"];
  wellMSG.Msg_Type  = doc["MT"];
  wellMSG.Msg_Value = doc["MV"];
  doc["ST"]     = ONLINE;  // Online

  if(wellMSG.Radio_ID == RELAY_ID) {   // ******************** check to see if msg is for RELAY Station
    //debugPrintln("RAW MSG is" + msg);
    debugPrintln("In Process (MSG) from WELL (" + (String)wellMSG.Well_ID + ")  " + 
                 "Message type was " + printWellMsgType(wellMSG.Msg_Type));

    // Keep Heartbeat signals up to date and send out to clients
    // Msg from well,  track well heartbeat counts
    if(wellMSG.Well_ID < RELAY_ID){  
      
      heartbeatTracking[wellMSG.Well_ID].wellID = wellMSG.Well_ID;

      // Any message from a WELL reset the miscount and means we have contact with WELL
      heartbeatTracking[wellMSG.Well_ID].lastMillisCount = wellMSG.Msg_Value;
      heartbeatTracking[wellMSG.Well_ID].missCount = 0;

      if(heartbeatTracking[wellMSG.Well_ID].missCount > maxHeartbeatMisses){
        doc["ST"] = OFFLINE;  // Offline
      }
      else{
        doc["ST"] = ONLINE;  // Online
      }
    }

    if(RELAYROLE){
      notifyClients();  // Update the web clients

      delay(100);  // wait a bit before sending out to Display Units

      pushtoDisplayUnits();

      writeLogMessage(msg ='\n');  // save msg to log file JSON format

    }
    //debugPrintln("Msg was for the RELAY");
  }


  if(wellMSG.Radio_ID == DISPLAY_RADIO_ID) {   // ******************** check to see if msg is for DISPLAY UNIT
    //debugPrintln("Msg was for the DISPLAY");
    doc["ST"]     = ONLINE;  // Online
    notifyClients();  // Update the web clients at DISPLAY UNIT
  }

  if(wellMSG.Radio_ID == WEB_RADIO_ID && RELAYROLE) {   // ******************** check to see if msg came DISPLAY UNIT
    //debugPrintln("Msg came from RELAY");
    wellMSG.Radio_ID  = RELAY_ID;
    sendrequestLORA("Relayed LORA msg sent out WELLS");  // change to sent from RELAY and send out via lora to wells
  }

}


void setup(){  // ****************************   1 Time SETUP 

  Serial.begin(115200);
  pinMode(ROLE_GPIO, INPUT_PULLDOWN);
  pinMode(WIFI_GPIO, INPUT_PULLDOWN);

  RELAYROLE = digitalRead(ROLE_GPIO);  // set the role based on jumper
  WIFIMODESTATION = digitalRead(WIFI_GPIO);  // set the WiFi to STATION OR AP based on jumper

  for (int i = 0; i < RELAY_ID-1; i++)  // reset tracking values
  {
    heartbeatTracking[i].wellID =-1;
    heartbeatTracking[i].lastMillisCount = -1;
    heartbeatTracking[i].missCount = 0;
  }
  
	Heltec.begin(true /*DisplayEnable Enable*/, true /*LoRa Enable*/, true /*Serial Enable*/, true /*LoRa use PABOOST*/, BAND /*LoRa RF working band*/);

  if(RELAYROLE){  // RELAY records data on SD CARD

    debugPrintln("ROLE IS RELAY");

    // Set up SD Card
    SPIClass(1);
    //spi1.begin(17, 13, 23, 22);
    spi1.begin(17, 13, 23, 2);

    if(!SD.begin(22, spi1)){
        Serial.println("Card Mount Failed");
        //return;
    }
    uint8_t cardType = SD.cardType();

    if(cardType == CARD_NONE){
        Serial.println("No SD card attached");
        //return;
    }

    Serial.print("SD Card Type: ");
    if(cardType == CARD_MMC){
        Serial.println("MMC");
    } else if(cardType == CARD_SD){
        Serial.println("SDSC");
    } else if(cardType == CARD_SDHC){
        Serial.println("SDHC");
    } else {
        Serial.println("UNKNOWN");
    }
  }else{
      debugPrintln("ROLE IS DISPLAY");
  }

//  Setup RTC
/*
 if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
  }

  DateTime now = rtc.now();
      Serial.print(now.year(), DEC);
    Serial.print('/');
    Serial.print(now.month(), DEC);
    Serial.print('/');
    Serial.print(now.day(), DEC);
    Serial.print(" (");
*/

   //sqlite3 *db1;
   //char *zErrMsg = 0;
   //int rc;

   //sqlite3_initialize();
   //rc = db_exec(db1, "SELECT * from history");
   
   //if (rc != SQLITE_OK) {
   //    sqlite3_close(db1);
   //    return;
   //}


	logo();
	delay(500);
	Heltec.display -> clear();

  if(WIFIMODESTATION){
    WIFISetUp();  // uncomment this to go back to station mode
    WiFi.mode(WIFI_MODE_STA);  // uncomment this to go back to station mode
    delay(100);
  }else{
    //Connect to Wi-Fi network with SSID and password
    //Serial.print("Setting AP (Access Point)…");
    // Remove the password parameter, if you want the AP (Access Point) to be open
    if(RELAYROLE)
      WiFi.softAP("WellRelay");
    else
      WiFi.softAP("WellDisplay");

  }

  
 
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  //WIFIScan(1);
  

  // Mount up SPIFF for use
  if(!SPIFFS.begin(true)){
    debugPrintln("An Error has occurred while mounting SPIFFS");
    return;
  }
  server.begin();

  Serial.println(WiFi.localIP());


	chipid=ESP.getEfuseMac();//The chip ID is essentially its MAC address(length: 6 bytes).
	Serial.printf("ESP32ChipID=%04X",(uint16_t)(chipid>>32));//print High 2 bytes
	Serial.printf("%08X\n\r",(uint32_t)chipid);//print Low 4bytes.

  attachInterrupt(0,interrupt_GPIO0,FALLING);
  
	LoRa.onReceive(onLORAReceive);

  LoRa.receive();

  displaySendReceive();

  // Route for root / web page
  //server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
  //  request->send_P(200, "text/html", index_html, processor);
  //});
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(SPIFFS, "/style.css","text/css");
  });

  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(SPIFFS, "/script.js","text/css");
  });

  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(200,"text/html"); 
  });

  server.on("/relaylog.txt", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(SD, "/relaylog.txt","text/html");
  });


  // Start server
  server.begin();

  initWebSocket();   // ************************   Start up WebSockets 

}

void loop() {
  currentMillis = millis();  // used in testing for a regular interval test

  //ws.cleanupClients();
  //digitalWrite(ledPin, ledState);

  delay(200);

  if(LoRa.available())
    LoRa.receive();

  if(receiveflag){  // got a packet in on LORA
    // debugPrintln("Receive flag true in main loop");
    LoRa.receive(); 
    processLORAMsg(packet);  // Process/discard the message
    delay(100);
    displaySendReceive();  // update OLED display
    receiveflag = false;
  }

 
  if(long(currentMillis - previousMillis) > TIMER1) {  // Periodic tasks
    previousMillis = currentMillis; 

    for (int i = 0; i < RELAY_ID-1; i++)  // loop over all wells
    { 
      if(heartbeatTracking[i].wellID != -1){
        heartbeatTracking[i].missCount++;  // update the miss counts for all wells

        if(heartbeatTracking[i].missCount > maxHeartbeatMisses){
          debugPrint("Well " );
          debugPrint(heartbeatTracking[i].wellID);
          debugPrintln(" has exceeded expected HEARTBEAT last seen time" );
          sendHeartbeatFailure(heartbeatTracking[i].wellID);
        }
        else{
          //debugPrint("Well " );
          //debugPrint(heartbeatTracking[i].wellID);
          //debugPrint(" count ");
          //debugPrintln(heartbeatTracking[i].missCount);
        }

      }
    }

    LoRa.receive();  // gotta have this in loop to make sure to LISTEN

  }

  if(long(currentMillis - previousMillis2) > keepaliveTimer) {  // Periodic tasks
    previousMillis2 = currentMillis; 
    sendKeepAlive();
  }


  if(LoRa.available())
    LoRa.receive();  // gotta have this in loop to make sure to LISTEN
 
}