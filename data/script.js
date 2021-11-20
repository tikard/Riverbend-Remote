// Complete project details: https://randomnerdtutorials.com/esp8266-nodemcu-web-server-websocket-sliders/

var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
var msgcounter = 0;

window.addEventListener("load", onload);

function onload(event) {
  initWebSocket();
}

function getValues() {
  websocket.send("getValues");
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
  var wellID = document.getElementById("wellID").value;
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
  obj.requestType = requestType;
  obj.wellID = wellID;

  //convert object to json string
  var wellRequest = JSON.stringify(obj);

  //convert string to Json Object
  //console.log(JSON.parse(wellRequest)); // this is your requirement.
  websocket.send(wellRequest);
}

function lastmsgtxt(input, msg) {
  var obj = document.getElementById(input);

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
  var timestamp = Date.now();
  console.log(timestamp);

  // Converting it back to human-readable date and time
  var d = new Date(timestamp);
  console.log(d);

  obj.value =
    d.toTimeString().substring(0, 8) + ":    " + msg + "\n" + obj.value;
  //obj.value+=msg;
  //obj.value+="\n";
}
function updateCount() {
  msgcounter++;
  //document.getElementById("msgCount").value = msgcounter + " Msgs";
  document.getElementById("ClrHistory").value =
    "Clear History (" + msgcounter + ")";
}

function onMessage(event) {
  console.log(event.data);
  lastcmd = JSON.parse(event.data);
  lastmsgtxt("lastmsg", lastcmd["radiomsg"]);
  addtxt("msghistory", lastcmd["radiomsg"]);
  updateCount();

  /*
    var keys = Object.keys(myObj);

    for (var i = 0; i < keys.length; i++){
        var key = keys[i];
        document.getElementById(key).innerHTML = myObj[key];
        document.getElementById("slider"+ (i+1).toString()).value = myObj[key];
    }
    */
}
