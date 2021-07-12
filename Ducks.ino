#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SocketIoClient.h>
#include <EEPROM.h>
#define duck D3
#define state D4
#define toggleButton D5
#define ack D6

//ESP8266WiFi WiFi;
ESP8266WebServer server(80);
SocketIoClient webSocket;

char ssid[32];
char pass[64];
char room[32];
int id;

int inputs = 0;
bool sending = false;
bool ackEnable = true;
bool togEnable = true;
bool connectionState = true;

void setup() {
  //Begin setup
  Serial.begin(115200);
  Serial.println("Beginning setup");

  //Enable pins
  pinMode(duck, OUTPUT);
  pinMode(state, OUTPUT);
  pinMode(toggleButton, INPUT_PULLUP);
  pinMode(ack, INPUT_PULLUP);

  digitalWrite(duck, HIGH);
  
  //Read values from EEPROM
  EEPROM.begin(512);
  for (int i = 0; i < 32; i++) { //Read SSID
    ssid[i] = EEPROM.read(i);
  }
  for (int i = 0; i < 64; i++) { //Read password
    pass[i] = EEPROM.read(i + 32);
  }
  for (int i = 0; i < 32; i++) { //Read room
    room[i] = EEPROM.read(i+96);
  }
  id = EEPROM.read(128); //Read ID

  Serial.println(ssid);
  Serial.println(pass);
  Serial.println(room);
  Serial.println(id);

  WiFi.mode(WIFI_STA); //Try to connect to Wi-Fi
  WiFi.begin(ssid, pass);
  int count = 0;
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    digitalWrite(state, digitalRead(state) == HIGH ? LOW : HIGH);
    count++;
    if (count > 40) {
      connectionState = false;
      break;
    }
  }
  if (digitalRead(toggleButton) == LOW && digitalRead(ack) == LOW) {
    Serial.println("Manual override!");
  }
  if (connectionState && (digitalRead(toggleButton) == HIGH || digitalRead(ack) == HIGH)) {
    Serial.println("\nConnected.");

    //Register events and connect to server
    webSocket.on("toggle", toggle);
    webSocket.on("acknowledge", acknowledge);
    webSocket.on("connect", connected);
    webSocket.on("disconnected", disconnected);
    webSocket.begin("ec2-13-58-213-225.us-east-2.compute.amazonaws.com", 4100);
    digitalWrite(duck, LOW);
    //webSocket.emit("register", (String("\"") + room + "\"").c_str());
  } else {
    connectionState = false;
    Serial.println("\nNot connected.");
    WiFi.mode(WIFI_AP);
    IPAddress apIP(192, 168, 0, 1);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP("DuckSetup");
    server.on("/", root);
    server.on("/submit", submit);
    server.begin();
    Serial.println("Set up server");
    digitalWrite(duck, HIGH);
  }


  Serial.println("Done with setup");
}

void loop() {
  if (connectionState) {
    clientLoop();
  } else {
    serverLoop();
  }
}

void serverLoop() {
  server.handleClient();
}

void clientLoop() {
  //Make the websocket work
  webSocket.loop();

  //Turn off duck if button is pressed
  if (digitalRead(ack) == LOW && ackEnable) {
    digitalWrite(duck, LOW);
    webSocket.emit("acknowledge", (String("\"") + room + "," + String(id) + (sending ? "1" : "0") + "\"").c_str());
    ackEnable = false;
    inputs = 0;
  } else if (digitalRead(ack) == HIGH) {
    ackEnable = true;
  }

  //Toggle other person's duck
  if (digitalRead(toggleButton) == LOW && togEnable) {
    sending = !sending;
    webSocket.emit("toggle", (String("\"") + room + "," + String(id) + (sending ? "1" : "0") + "\"").c_str());

    //Have light reflect other person's
    if (sending) {
      digitalWrite(state, LOW);
    } else {
      digitalWrite(state, HIGH);
    }
    togEnable = false;
  } else if (digitalRead(toggleButton) == HIGH) {
    togEnable = true;
  }
}

void root() {
  server.send(200, "text/html", "<form action=\"/submit\">"
  "<label for=\"ssid\">WiFi name:</label><br/>"
  "<input type=\"text\" id=\"ssid\" name=\"ssid\"><br/>"
  "<label for=\"pass\">WiFi password:</label><br/>"
  "<input type=\"text\" id=\"pass\" name=\"pass\"><br/>"
  "<label for=\"room\">Room name:</label><br/>"
  "<input type=\"text\" id=\"room\" name=\"room\"><br/>"
  "<label for=\"id\">ID:</label><br/>"
  "<input type=\"number\" id=\"id\" name=\"id\" min=\"0\" max=\"9\"><br/>"
  "<input type=\"submit\" value=\"Submit\">"
  "</form>");
}

void submit() {
  String ssidString = server.arg("ssid");
  String passString = server.arg("pass");
  String roomString = server.arg("room");
  String idString = server.arg("id");

  Serial.println(ssidString);
  Serial.println(passString);
  Serial.println(roomString);
  Serial.println(idString);

  if (ssidString != "") {
      for (int i = 0; i < 32; i++) { //Write SSID
        EEPROM.write(i, ssidString[i]);
      }
  }
  if (passString != "") {
    for (int i = 0; i < 64; i++) { //Write password
      EEPROM.write(i + 32, passString[i]);
    }
  }
  if (roomString != "") {
    for (int i = 0; i < 32; i++) { //Write room
      EEPROM.write(i+96, roomString[i]);
    }
  }
  if (idString != "") {
    EEPROM.write(128, (String(idString)).toInt()); //Write ID
  }
  EEPROM.commit();
  server.send(200, "text/plain", "Success!");
  ESP.restart();
}

//Handle data
void toggle(const char* data, size_t length) {
  Serial.println("Got data!");
  String dat = String(data);
  
  //Only act if ID is not own
  if (dat.toInt() / 10 != id) {

    //Count inputs
    if (dat.toInt() % 10 == 1) {
      inputs++;
    } else {
      inputs--;
    }

    //Reflect inputs on duck
    if (inputs < 0) {
      inputs = 0;
    }
    if (inputs > 0) {
      digitalWrite(duck, HIGH);
    } else {
      digitalWrite(duck, LOW);
    }
  }
}

//Handle data
void acknowledge(const char* data, size_t length) {
  Serial.println("Got ackowledgement!");
  String dat = String(data);
  
  //Only act if ID is not own
  if (dat.toInt() / 10 != id) {
    digitalWrite(state, HIGH);
    sending = false;
  }
}

//Handle connection
void connected(const char* data, size_t length) {
  Serial.println("Registering");
  webSocket.emit("register", (String("\"") + room + "\"").c_str());
}

//Handle disconnection...?
void disconnected(const char* data, size_t length) {
  setup();
}
