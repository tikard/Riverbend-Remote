// Complete project details: https://randomnerdtutorials.com/esp8266-nodemcu-web-server-websocket-sliders/

var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
var msgcounter = 0;
var RADIO_ID = 101; // RADIO ID to send to RELAY/DISPLAY Stations

window.addEventListener("load", onload);

function onload(event) {
  initWebSocket();
}

function getValues() {
  //websocket.send("getValues");
  setInterval(WebSocketkeepalive, 15000);
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

function WebSocketkeepalive() {
  var obj = new Object();
  obj.KA = "1";
  var keepaliveRequest = JSON.stringify(obj);
  //console.log(JSON.parse(keepaliveRequest)); // Request in JSON
  websocket.send(keepaliveRequest);
}

function sendRequest(wellID, requestType) {
  // Put this in JSON format and send to server
  var obj = new Object();
  obj.RID = RADIO_ID;
  obj.WID = wellID;
  obj.MT = requestType;
  obj.MV = 0;

  //convert object to json string
  var wellRequest = JSON.stringify(obj);

  //convert string to Json Object
  console.log(JSON.parse(wellRequest)); // Request in JSON
  websocket.send(wellRequest);
}

function lastmsgtxt(input, msg) {
  var obj = document.getElementById(input);
  var w = "";
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
          w = "Well" + s[0];
          document.getElementById(w).style.backgroundColor = "greenyellow";
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
          w = "Well" + s[0];
          document.getElementById(w).style.backgroundColor = "yellow";
          break;
        default:
          text2 = "INVALID STATE CODE";
      }
      break;
    case "3":
      text = " (STATUS)";
      switch (s[2]) {
        case "-1":
          text2 = " Not EMPTY or FULL";
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
      w = "Well" + s[0];
      switch (s[2]) {
        case "0":
          text2 = " START FAILURE";
          w = "Well" + s[0];
          document.getElementById(w).style.backgroundColor = "yellow";
          break;
        case "1":
          text2 = " POWER FAIL ON FILL";
          w = "Well" + s[0];
          document.getElementById(w).style.backgroundColor = "yellow";
          break;
        case "2":
          text2 = " FILL TIME EXCEEDED";
          w = "Well" + s[0];
          document.getElementById(w).style.backgroundColor = "yellow";
          break;
        case "3":
          text2 = " STOP ERROR";
          w = "Well" + s[0];
          document.getElementById(w).style.backgroundColor = "yellow";
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
          text2 = " Not EMPTY or FULL";
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
  var w = "";
  console.log("msg in heartbeat is " + msg);
  if (s[1] == "0") {
    w = "Well" + s[0];
    if (document.getElementById(w).style.backgroundColor != "yellow") {
      document.getElementById(w).innerHTML = "Well " + s[0] + "  ONLINE";
      document.getElementById(w).style.backgroundColor = "greenyellow";
    }
  } else {
    w = "Well" + s[0];
    document.getElementById(w).innerHTML = "Well " + s[0] + "  OFFLINE";
    document.getElementById(w).style.backgroundColor = "Red";
  }
}

function onMessage(event) {
  console.log(event.data);
  lastcmd = JSON.parse(event.data);
  var radioID = lastcmd["RID"]; // Radio ID
  var wellID = lastcmd["WID"];
  var msgType = lastcmd["MT"];
  var msgValue = lastcmd["MV"];
  var Status = lastcmd["ST"];
  var keepalivemsg = lastcmd["KA"];

  if (keepalivemsg) {
    console.log("Got a keep alive from server");
    return;
  }

  updateHeartbeatStatus(lastcmd["WID"] + " " + lastcmd["ST"]);

  if (msgType) {
    // only update page if a msgType exists

    lastmsgtxt(
      "lastmsg",
      lastcmd["WID"] + " " + lastcmd["MT"] + " " + lastcmd["MV"]
    );

    addtxt(
      "msghistory",
      lastcmd["WID"] + " " + lastcmd["MT"] + " " + lastcmd["MV"]
    );

    updateCount();
  }
}
