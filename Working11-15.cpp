/*********
  Relay Server part of WELL SYSTEM

  Use WebSockets to talk to a control web page
  Uses LORA to xmit and recieve WELL msgs

*********/

// Import required libraries
#include "Arduino.h"
#include "heltec.h"
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
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

const long HEARTBEAT_DELAY = 5;  // Heartbeat delay in seconds

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



// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP Web Server</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <style>
  html {
    font-family: Arial, Helvetica, sans-serif;
    text-align: center;
  }
  h1 {
    font-size: 1.8rem;
    color: white;
  }
  h2{
    font-size: 1.5rem;
    font-weight: bold;
    color: #143642;
  }
  .topnav {
    overflow: hidden;
    background-color: #143642;
  }
  body {
    margin: 0;
  }
  .content {
    padding: 30px;
    max-width: 600px;
    margin: 0 auto;
  }
  .card {
    background-color: #F8F7F9;;
    box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5);
    padding-top:10px;
    padding-bottom:20px;
  }
  .button {
    padding: 15px 50px;
    font-size: 24px;
    text-align: center;
    outline: none;
    color: #fff;
    background-color: #0f8b8d;
    border: none;
    border-radius: 5px;
    -webkit-touch-callout: none;
    -webkit-user-select: none;
    -khtml-user-select: none;
    -moz-user-select: none;
    -ms-user-select: none;
    user-select: none;
    -webkit-tap-highlight-color: rgba(0,0,0,0);
   }
   /*.button:hover {background-color: #0f8b8d}*/
   .button:active {
     background-color: #0f8b8d;
     box-shadow: 2 2px #CDCDCD;
     transform: translateY(2px);
   }
   .state {
     font-size: 1.5rem;
     color:#8c8c8c;
     font-weight: bold;
   }
  </style>
<title>ESP Web Server</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<link rel="icon" href="data:,">
</head>
<body>
  <div class="topnav">
    <h1>ESP WebSocket Server</h1>
  </div>
  <div class="content">
    <div class="card">
      <h2>Output - GPIO 2</h2>
      <p class="state">state: <span id="state">%STATE%</span></p>
      <p><button id="button" class="button">Toggle</button></p>
    </div>
  </div>
<script>
  var gateway = `ws://${window.location.hostname}/ws`;
  var websocket;
  window.addEventListener('load', onLoad);
  function initWebSocket() {
    console.log('Trying to open a WebSocket connection...');
    websocket = new WebSocket(gateway);
    websocket.onopen    = onOpen;
    websocket.onclose   = onClose;
    websocket.onmessage = onMessage; // <-- add this line
  }
  function onOpen(event) {
    console.log('Connection opened');
  }
  function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
  }
  function onMessage(event) {
    var state;
    if (event.data == "1"){
      state = "ON";
    }else if (event.data == "0"){
      state = "OFF";
    }else if (event.data == "TRACY"){
      state = "Tracy";
    }else if (event.data == "3"){
      state = "Ikard";
    }else {state ="NONE"}


    document.getElementById('state').innerHTML = state;
  }
  function onLoad(event) {
    initWebSocket();
    initButton();
  }
  function initButton() {
    document.getElementById('button').addEventListener('click', toggle);
  }
  function toggle(){
    websocket.send('toggle');
  }
</script>
</body>
</html>
)rawliteral";

//*******************  Well Functions
// Helpers to send output to right port
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
    //LoRa.print(counter++);
    counter++;
    LoRa.endPacket();
}

void requestHeartBeat(String wellNumber){
  RadioMsgRespone = "";
  RadioMsgRespone = RadioMsgRespone + String(RELAY_ID) + " ";
  RadioMsgRespone = RadioMsgRespone + wellNumber + " ";
  RadioMsgRespone = RadioMsgRespone  + String(WELL_MSG_TYPE::HEARTBEAT) + " ";
  RadioMsgRespone = RadioMsgRespone + "0";
	send(RadioMsgRespone);
}

void requestState(String wellNumber){
  RadioMsgRespone = "";
  RadioMsgRespone = RadioMsgRespone + String(RELAY_ID) + " ";
  RadioMsgRespone = RadioMsgRespone + wellNumber + " ";
  RadioMsgRespone = RadioMsgRespone  + String(WELL_MSG_TYPE::WELL_STATE) + " ";
  RadioMsgRespone = RadioMsgRespone + "0";
	send(RadioMsgRespone);
}

void processMsg(String msg){
  // process a message 
    // reads in the 4 part Well CMD msg
  int temp, ind1, ind2, ind3, ind4;
 
  ind1 = msg.indexOf(' ');  //finds location of first ,
  wellMSG.Radio_ID = msg.substring(0, ind1).toInt();   //captures first data String

  ind2 = msg.indexOf(' ', ind1+1 );   //finds location of second ,
  wellMSG.Well_ID = msg.substring(ind1+1, ind2+1).toInt();   //captures second data String
  
  ind3 = msg.indexOf(' ', ind2+1 );
  temp = msg.substring(ind2+1, ind3+1).toInt();
  
  // look for newline
  ind4 = msg.indexOf('\n', ind3 );
  wellMSG.Msg_Value = msg.substring(ind3+1,ind4).toInt(); 

  switch (temp)
  {
  case WELL_MSG_TYPE::WMT_STATUS:
    wellMSG.Msg_Type = WELL_MSG_TYPE::WMT_STATUS;
    break;
  case WELL_MSG_TYPE::CMD   :
    wellMSG.Msg_Type = WELL_MSG_TYPE::CMD;
    break;
  case  WELL_MSG_TYPE::WELL_STATE :
    wellMSG.Msg_Type = WELL_MSG_TYPE::WELL_STATE;
    break;
  case WELL_MSG_TYPE::WELL_STATUS  :
    wellMSG.Msg_Type = WELL_MSG_TYPE::WELL_STATUS;
    break;
  case WELL_MSG_TYPE::START_FILL  :
    wellMSG.Msg_Type = WELL_MSG_TYPE::START_FILL;
    break;
  case WELL_MSG_TYPE::STOP_FILL  :
    wellMSG.Msg_Type = WELL_MSG_TYPE::STOP_FILL;
    break;
  case WELL_MSG_TYPE::RESTART_WELL  :
    wellMSG.Msg_Type = WELL_MSG_TYPE::RESTART_WELL;
    break;
  case WELL_MSG_TYPE::WELL_ERRORS  :
    wellMSG.Msg_Type = WELL_MSG_TYPE::WELL_ERRORS;
    break;
  case WELL_MSG_TYPE::HEARTBEAT  :
    wellMSG.Msg_Type = WELL_MSG_TYPE::HEARTBEAT;
    break;
  default:
    wellMSG.Msg_Type = INVALID_MSG;  // invalid command 
    wellMSG.Msg_Type = (WELL_MSG_TYPE)temp;        // invalid command 
    break;
  }

  // ******************** check to see if this msg is for us
  if(wellMSG.Radio_ID == RELAY_ID) { //   For the RELAY Station
    //debugPrintln("RAW MSG is" + msg);
    debugPrintln("In Process (MSG) from WELL (" + (String)wellMSG.Well_ID + ")");
    debugPrintln("Message type was " + printWellMsgType(wellMSG.Msg_Type));

  }
  else
    debugPrintln("Msg not for the RELAY");
    
}


// *******************  Web Socket routines
void notifyClients() {
  ws.textAll(String(ledState));
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    if (strcmp((char*)data, "toggle") == 0) {
      ledState = !ledState;
      currentMillis = 0;
      notifyClients();
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

String processor(const String& var){
  Serial.println(var);
  if(var == "STATE"){
    if (ledState){
      return "ON";
    }
    else{
      return "OFF";
    }
  }
}

// ************ LORA METHODS


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
  Heltec.display -> drawString(0, 40, "With " + rssi + "db");
  Heltec.display -> drawString(0, 50, "Sent " + (String)(counter-1) + " requests");
  Heltec.display -> display();
  delay(50);
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

  //debugPrintln(packet);
  rssi = "RSSI: " + String(LoRa.packetRssi(), DEC);    
  receiveflag = true;    
}

// ****************************
void setup(){

  wellMSG.Radio_ID = 5;
  wellMSG.Well_ID = 2;
  wellMSG.Msg_Type = WELL_MSG_TYPE::WMT_STATUS;
	
	Heltec.begin(true /*DisplayEnable Enable*/, true /*LoRa Enable*/, true /*Serial Enable*/, false /*LoRa use PABOOST*/, BAND /*LoRa RF working band*/);

	logo();
	delay(300);
	Heltec.display -> clear();

	WIFISetUp();
	//WiFi.disconnect(); //WIFI
	//WiFi.mode(WIFI_MODE_AP);
	WiFi.mode(WIFI_MODE_STA);
	delay(100);

	WIFIScan(1);

  Serial.println(WiFi.localIP());


	chipid=ESP.getEfuseMac();//The chip ID is essentially its MAC address(length: 6 bytes).
	Serial.printf("ESP32ChipID=%04X",(uint16_t)(chipid>>32));//print High 2 bytes
	Serial.printf("%08X\n",(uint32_t)chipid);//print Low 4bytes.

  attachInterrupt(0,interrupt_GPIO0,FALLING);
  
	LoRa.onReceive(onReceive);

  LoRa.receive();
  displaySendReceive();

  
  initWebSocket();   // ************************   Start up WebSockets 

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

  // Start server
  server.begin();

}

void loop() {
  currentMillis = millis();  // used in testing for a regular interval test

  ws.cleanupClients();
  digitalWrite(ledPin, ledState);

  sleep(1);
  //ws.textAll("TRACY");
  sleep(1);
  //ws.textAll("3");



  if(receiveflag){
    //debugPrintln("Receive flag true in main loop");
    // *********** Take care of this packet if it is for this location

    receiveflag = false;
    LoRa.receive();
    processMsg(packet);  // Process/discard the message
    delay(5);
  }

 
  if(long(currentMillis - previousMillis) > interval1) {
    // save the last time you blinked the LED 
    previousMillis = currentMillis;   
  
    //digitalWrite(25,HIGH);
    displaySendReceive();

   if(currentMillis%2 == 0){
      debugPrintln("Request Heartbeat From Well 3 ....");
      requestHeartBeat("3");
      toggle=!toggle;
    }else{
      debugPrintln("Request State From Well 3 ........");
      requestState("3");
      toggle=!toggle;
    }  

    LoRa.receive();  // gotta have this in loop to make sure to LISTEN
    //displaySendReceive();
  }
 
}