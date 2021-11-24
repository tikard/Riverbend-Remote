// Complete project details: https://randomnerdtutorials.com/esp8266-nodemcu-web-server-websocket-sliders/

var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
var msgcounter = 0;

window.addEventListener("load", onload);

function onload(event) {
  initWebSocket();
}

function getValues() {
  //websocket.send("getValues");
}

function initWebSocket() {
  console.log("Trying to open a WebSocket connection…");
  websocket = new WebSocket(gateway);
  websocket.onopen = onOpen;
  websocket.onclose = onClose;
  websocket.onmessage = onMessage;
}

function onOpen(event) {
  console.log("Connection opened");
  getValues();
}

function onClose(event) {
  console.log("Connection closed");
  setTimeout(initWebSocket, 2000);
}

function buttonpressed(requestType) {
  var wellID = document.getElementById("wellID").value; // from the pulldown
  console.log(requestType + " " + wellID);
  sendRequest(wellID, requestType);
}

function cmdbuttonpressed(requestType) {
  var wellID = document.getElementById("wellID").value;
  console.log(requestType + " " + wellID);
  sendRequest(wellID, requestType);
}

function clearHistory() {
  console.log("clearing history");
  document.getElementById("ClrHistory").value = "Clear History ";
  document.getElementById("msghistory").value = "";
  msgcounter = 0;
}

function sendRequest(wellID, requestType) {
  // Put this in JSON format and send to server
  var obj = new Object();
  obj.radioID = 99;
  obj.wellID = wellID;
  obj.msgType = requestType;
  obj.msgValue = 0;

  //convert object to json string
  var wellRequest = JSON.stringify(obj);

  //convert string to Json Object
  console.log(JSON.parse(wellRequest)); // Request in JSON
  websocket.send(wellRequest);
}

function lastmsgtxt(input, msg) {
  var obj = document.getElementById(input);
  //console.log("msg in " + msg);
  text = "";
  text2 = "";
  const s = msg.split(" ");
  switch (s[1]) {
    case "2":
      text = " (STATE)";
      switch (s[2]) {
        case "0":
          text2 = " IDLE";
          break;
        case "1":
          text2 = " START";
          break;
        case "2":
          text2 = " POWER FAIL";
          break;
        case "3":
          text2 = " FILLING";
          break;
        case "4":
          text2 = " STOPPING";
          break;
        case "5":
          text2 = " KILLING POWER";
          break;
        case "6":
          text2 = " FILL COMPLETED";
          break;
        case "7":
          text2 = " FAULT";
          break;
        default:
          text2 = "INVALID STATE CODE";
      }
      break;
    case "3":
      text = " (STATUS)";
      switch (s[2]) {
        case "-1":
          text2 = " Not EMPTY and Not FULL";
          break;
        case "0":
          text2 = " TANK EMPTY";
          break;
        case "1":
          text2 = " TANK FULL";
          break;
        case "2":
          text2 = " WELL ERROR";
          break;
        default:
          text2 = "INVALID STATUS CODE";
      }
      break;
    case "7":
      text = " (ERRORS)";
      switch (s[2]) {
        case "0":
          text2 = " START FAILURE";
          break;
        case "1":
          text2 = " POWER FAIL ON FILL";
          break;
        case "2":
          text2 = " FILL TIME EXCEEDED";
          break;
        case "3":
          text2 = " STOP ERROR";
          break;
        case "4":
          text2 = " NO ERRORS";
          break;
        default:
          text2 = "INVALID ERROR CODE";
      }
      break;
    case "8":
      text = " (HEARTBEAT)";
      text2 = s[2];
      break;
    default:
      text = "";
  }

  obj.value = msg + text + " msg->" + text2;
}

function addtxt(input, msg) {
  var obj = document.getElementById(input);

  // Converting it back to human-readable date and time
  var d = new Date(Date.now());

  text = "";
  text2 = "";
  const s = msg.split(" ");
  switch (s[1]) {
    case "2":
      text = " (STATE)";
      switch (s[2]) {
        case "0":
          text2 = " IDLE";
          break;
        case "1":
          text2 = " START";
          break;
        case "2":
          text2 = " POWER FAIL";
          break;
        case "3":
          text2 = " FILLING";
          break;
        case "4":
          text2 = " STOPPING";
          break;
        case "5":
          text2 = " KILLING POWER";
          break;
        case "6":
          text2 = " FILL COMPLETED";
          break;
        case "7":
          text2 = " FAULT";
          break;
        default:
          text2 = "INVALID STATE CODE";
      }
      break;
    case "3":
      text = " (STATUS)";
      switch (s[2]) {
        case "-1":
          text2 = " Not EMPTY and Not FULL";
          break;
        case "0":
          text2 = " TANK EMPTY";
          break;
        case "1":
          text2 = " TANK FULL";
          break;
        case "2":
          text2 = " WELL ERROR";
          break;
        default:
          text2 = "INVALID STATUS CODE";
      }
      break;
    case "7":
      text = " (ERRORS)";
      switch (s[2]) {
        case "0":
          text2 = " START FAILURE";
          break;
        case "1":
          text2 = " POWER FAIL ON FILL";
          break;
        case "2":
          text2 = " FILL TIME EXCEEDED";
          break;
        case "3":
          text2 = " STOP ERROR";
          break;
        case "4":
          text2 = " NO ERRORS";
          break;
        default:
          text2 = "INVALID ERROR CODE";
      }
      break;
    case "8":
      text = " (HEARTBEAT)";
      text2 = s[2];
      break;
    default:
      text = "";
  }

  obj.value =
    d.toTimeString().substring(0, 7) +
    ": " +
    msg +
    "  WELL " +
    s[0] +
    "  " +
    text +
    " msg->" +
    text2 +
    "\n" +
    obj.value;

  //obj.value+=msg;
  //obj.value+="\n";
}
function updateCount() {
  msgcounter++;
  //document.getElementById("msgCount").value = msgcounter + " Msgs";
  document.getElementById("ClrHistory").value =
    "Clear History (" + msgcounter + ")";
}

function updateHeartbeatStatus(msg) {
  const s = msg.split(" ");
  if (s[1] == "0") {
    w = "Well" + s[0];
    document.getElementById(w).innerHTML = "Well " + s[0] + "  ONLINE";
    document.getElementById(w).style.backgroundColor = "greenyellow";
  } else {
    document.getElementById(w).innerHTML = "Well " + s[0] + "  OFFLINE";
    document.getElementById(w).style.backgroundColor = "Red";
  }
}

function onMessage(event) {
  console.log(event.data);
  lastcmd = JSON.parse(event.data);
  var radioID = lastcmd["radioID"];
  var wellID = lastcmd["wellID"];
  var msgType = lastcmd["msgType"];
  var msgValue = lastcmd["msgValue"];
  var Status = lastcmd["Status"];

  lastmsgtxt(
    "lastmsg",
    lastcmd["wellID"] + " " + lastcmd["msgType"] + " " + lastcmd["msgValue"]
  );

  addtxt(
    "msghistory",
    lastcmd["wellID"] + " " + lastcmd["msgType"] + " " + lastcmd["msgValue"]
  );

  updateCount();

  updateHeartbeatStatus(Status);

  //addtxt("msghistory", "*** WELL3 is ONLINE ***");
}
