/*********
  Relay Server part of WELL SYSTEM

  Use WebSockets to talk to a control web page
  Uses LORA to xmit and recieve WELL msgs

  Initial commit to Github
  Version 0.1  Full working version thru Well_Receiver to Riverbend Well Unit  11/19/21


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
//#include "SPI.h"
//#include "FS.h"
//#include "SD.h"



#define BAND    915E6  //you can set band here directly,e.g. 868E6,915E6

String rssi = "RSSI --";
String packSize = "--";
String packet;
String lastpacket;

unsigned int counter = 0;

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
const int RELAY_ID = 99;
const int WELL_ID = 100;
const int RADIO_ID = 100;
const int DISPLAY_WELL_ID = 105;

const long HEARTBEAT_DELAY = 2;  // Heartbeat delay in minutes

long TIMER1 = 60000 * HEARTBEAT_DELAY;
long previousMillis = 0;        // will store last time LED was updated

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

void send(String msg)
{
    LoRa.beginPacket();
    LoRa.print(msg);
    counter++;
    LoRa.endPacket();
    delay(250);
    LoRa.receive();
    debugPrintln("Sending LORA Packet out to wells");
}


void sendrequestLORA(){  // send out a request to all wells via LORA 
  String requestBody;

  serializeJson(doc, requestBody);   // data is in the doc
  //debugPrintln(requestBody);
  send(requestBody); 
}


void notifyClients() {  // tracy
  String requestBody;
    
  doc["radioID"]  = RELAY_ID;          // Add values in the document
  doc["wellID"]   = wellMSG.Well_ID;   // Add values in the document
  doc["msgType"]  = wellMSG.Msg_Type;  // Add values in the document
  doc["msgValue"] = wellMSG.Msg_Value; // Add values in the document
  

  serializeJson(doc, requestBody);

  //debugPrintln("json :" + requestBody);
  ws.textAll(requestBody);

}

void pushtoDisplayUnits(){  // Send out whole msg to Display units which are listening
  String requestBody = "";
  doc.clear();
  doc["radioID"]  = DISPLAY_WELL_ID;          // Add values in the document
  doc["wellID"]   = wellMSG.Well_ID;   // Add values in the document
  doc["msgType"]  = wellMSG.Msg_Type;  // Add values in the document
  doc["msgValue"] = wellMSG.Msg_Value; // Add values in the document
  serializeJson(doc, requestBody);

  debugPrintln("Echo to Display unit = " + requestBody);

}

void sendHeartbeatFailure(int wellid){
  doc.clear();
  doc["Status"] = String(wellid) + " 1";  // Offline
  notifyClients();

    delay(100);  // wait a bit before sending out to Display Units

    pushtoDisplayUnits();

}

void processLORAMsg(String msg){  // process a JSON msg from a well station LORA 
  doc.clear();
  DeserializationError error = deserializeJson(doc, msg);
  if (error) {
    debugPrintln("Error decoded JSON msg from wells" + msg);
    debugPrintln(error.c_str()); 
    return;
  }

  wellMSG.Radio_ID  = doc["radioID"];
  wellMSG.Well_ID   = doc["wellID"];
  wellMSG.Msg_Type  = doc["msgType"];
  wellMSG.Msg_Value = doc["msgValue"];

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
        doc["Status"] = String(wellMSG.Well_ID) + " 1";  // Offline
      }
      else{
        doc["Status"] = String(wellMSG.Well_ID) + " 0";  // Online
      }
    }

    notifyClients();

    delay(100);  // wait a bit before sending out to Display Units

    pushtoDisplayUnits();

  }
  else
    debugPrintln("Msg not for the RELAY");
}


// *******************  Web Socket routines


void serverRequest(void *arg, uint8_t *data, size_t len) {
  //AwsFrameInfo *info = (AwsFrameInfo*)arg;

  DeserializationError error = deserializeJson(doc, data);
  if (error) {
      debugPrintln(error.c_str()); 
      return;
    }

  //int reqradioID = doc["radioID"];
  //int reqwellID  = doc["wellID"];
  //int reqreqType = doc["msgType"];
  //int reqvalue   = doc["msgValue"];

  //debugPrintln("Request from RELAY");
  //debugPrintln("Radio ID =" + reqradioID);
  //debugPrintln("Well ID =" + reqwellID);
  //debugPrintln("Req type =" + reqreqType);
  //debugPrintln("Req value =" + reqvalue);
  
  //sendrequestLORA(reqwellID, reqreqType);  // see what they want to do
  sendrequestLORA();  // see what they want to do
}

// recieved data on websocket
void wsDataEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {

  //debugPrintln("Got Websocket request");
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
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

void displaySendReceive()
{
  Heltec.display -> drawString(0, 0, "Last MSG Received:");
  //Heltec.display -> drawString(0, 0, "Received Size  " + packSize + " packages:");
  Heltec.display -> drawString(0, 10, packet);
  Heltec.display -> drawString(0, 20,  printWellMsgType(wellMSG.Msg_Type));
  Heltec.display -> drawString(0, 40, "With " + rssi + "db");
  Heltec.display -> drawString(0, 50, "Sent " + (String)(counter-1) + " requests");
  Heltec.display -> display();
  delay(50);
  Heltec.display -> clear();
}

void onLORAReceive(int packetSize)//LoRa receiver interrupt service
{
  //if (packetSize == 0) return;
  packet = "";
  packSize = String(packetSize,DEC);

  //debugPrintln("onRecieved Interupt handler" + packet);

  while (LoRa.available())
  {
  packet += (char) LoRa.read();
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


void setup(){  // ****************************   1 Time SETUP 

  //Serial.begin(115200);

  for (int i = 0; i < RELAY_ID-1; i++)  // reset tracking values
  {
    heartbeatTracking[i].wellID =-1;
    heartbeatTracking[i].lastMillisCount = -1;
    heartbeatTracking[i].missCount = 0;
  }
  
 

	Heltec.begin(true /*DisplayEnable Enable*/, true /*LoRa Enable*/, true /*Serial Enable*/, true /*LoRa use PABOOST*/, BAND /*LoRa RF working band*/);


  //SPI.begin(17, 13, 23, 22);
  //SD.begin(22,SPI);

  // writeFile(SD, "/hello.txt", "Hello ");

	logo();
	delay(300);
	Heltec.display -> clear();

	WIFISetUp();  // uncomment this to go back to station mode
	WiFi.mode(WIFI_MODE_STA);  // uncomment this to go back to station mode
	delay(100);

  /*
  //Connect to Wi-Fi network with SSID and password
  Serial.print("Setting AP (Access Point)…");
  // Remove the password parameter, if you want the AP (Access Point) to be open
  WiFi.softAP("Welltest");
  


  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  debugPrintln(IP);
  */

  // Mount up SPIFF for use
  if(!SPIFFS.begin(true)){
    debugPrintln("An Error has occurred while mounting SPIFFS");
    return;
  }
  server.begin();

	//WIFIScan(1);

  debugPrintln(WiFi.localIP());


	chipid=ESP.getEfuseMac();//The chip ID is essentially its MAC address(length: 6 bytes).
	Serial.printf("ESP32ChipID=%04X",(uint16_t)(chipid>>32));//print High 2 bytes
	Serial.printf("%08X\n",(uint32_t)chipid);//print Low 4bytes.

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

  // Start server
  server.begin();

  initWebSocket();   // ************************   Start up WebSockets 

}

void loop() {
  currentMillis = millis();  // used in testing for a regular interval test

  ws.cleanupClients();
  digitalWrite(ledPin, ledState);

  delay(250);

  if(receiveflag){  // got a packet in on LORA
    // debugPrintln("Receive flag true in main loop");
    LoRa.receive(); 
    processLORAMsg(packet);  // Process/discard the message
    delay(100);
    receiveflag = false;
  }

 
  if(long(currentMillis - previousMillis) > TIMER1) {
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
          debugPrint("Well " );
          debugPrint(heartbeatTracking[i].wellID);
          debugPrint(" count ");
          debugPrintln(heartbeatTracking[i].missCount);
        }

      }
    }
      
  
    displaySendReceive();

    LoRa.receive();  // gotta have this in loop to make sure to LISTEN
    //displaySendReceive();
  }


  LoRa.receive();  // gotta have this in loop to make sure to LISTEN
 
}
