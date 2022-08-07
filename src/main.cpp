#include <SPI.h>
#include <WiFi.h>
#include <WiFiAP.h>
#include <Arduino.h>
#include <AsyncUDP.h>
#include <TFT_eSPI.h>
#include <ArduinoJson.h>
#include <ArduinoHttpClient.h>

#include "esp_wifi.h"
#include "heartichoke.h"

const int Button0 = 0;
const int Button1 = 35;

TFT_eSPI tft = TFT_eSPI();

AsyncUDP UDP;
#define UDP_PORT 4210

String version = "0.51";
#define SSID "Heartichoke"
#define PASS "basicbitchpass"

//These are all of the devices Heartichoke will speak to...
//connections[node][Node IP, Node Identifier]
const int connectionsAvailable = 10;
String connections[connectionsAvailable][2];

//Heartichokes designated IP Address
IPAddress Heartichoke_IP(192, 168, 1, 216);

void throwError(String error) {
    //Add error led code here
    Serial.println("ERROR: " + error);
}

void drawScreen(String message = "");
void drawScreen(String message) {
    Serial.println("Drawing screen...");

    //Draw our background
    tft.fillScreen(TFT_GREEN);
    tft.pushImage(0, 0, 240, 135, heartichoke);

    //Draw IP and version
    tft.setTextColor(TFT_BLACK);
    tft.drawString("IP: " + WiFi.localIP().toString(), 3, 126, 1);
    tft.drawString("v" + version, 207, 126, 1);

    //Draw temp nodes and rgb nodes up top
    tft.setTextColor(TFT_BLUE);
    tft.drawString("Connected!", 3, 3, 2);
    tft.drawString("Nodes: " + String(WiFi.softAPgetStationNum()), 3, 17, 2);

    //Node count = WiFi.softAPgetStationNum()

    //If no message was sent and we're redrawing the screen assume we're loading
    if (message == "Loading...") {
        tft.setTextColor(TFT_RED);
        tft.drawString(message, 65, 58, 4);

    //If our connection status reads as disconnected...
    } else if (WiFi.status() != WL_CONNECTED) {
        tft.setTextColor(TFT_RED);
        tft.drawString("DISCONNECTED!", 20, 58, 4);

    //Else display whatever message we were passed here
    } else {
        tft.setTextColor(TFT_BLUE);
        tft.drawString(message, 10, 110, 1);
    }
}

int fetchNodeIdFor(String identifier) {
    for (int i = 0; i < connectionsAvailable; i++)
        if (connections[i][1] == identifier)
            return i;

    return -1;
}
void registerNodes() {
    wifi_sta_list_t wifi_sta_list;
    tcpip_adapter_sta_list_t adapter_sta_list;

    memset(&wifi_sta_list, 0, sizeof(wifi_sta_list));
    memset(&adapter_sta_list, 0, sizeof(adapter_sta_list));

    esp_wifi_ap_get_sta_list(&wifi_sta_list);
    tcpip_adapter_get_sta_list(&wifi_sta_list, &adapter_sta_list);

    for (int i = 0; i < adapter_sta_list.num; i++) {

        tcpip_adapter_sta_info_t station = adapter_sta_list.sta[i];

        char ip[16];
        connections[i][0] = String(esp_ip4addr_ntoa(&station.ip, ip, IP4ADDR_STRLEN_MAX));
//        if (connections[i][1].length() == 0)
//            identifyNode(i);

        Serial.println(connections[i][0]);
    }

    for (int c = 0; c < connectionsAvailable; c++) {
        Serial.println("Connections[" + String(c) + "] = [" + connections[c][0] + ", " + connections[c][1] + "]");
    }
} //Registers our connected nodes in the global connections[] array
void nodeChanger(WiFiEvent_t event, WiFiEventInfo_t info) {
    registerNodes();
} //Is called on node connection updates to re-register nodes

void connectToWifi() {

    //Connect to wifi
    WiFi.mode(WIFI_AP_STA);

//    WiFi.onEvent(nodeChanger, ARDUINO_EVENT_WIFI_AP_STACONNECTED);
    WiFi.onEvent(nodeChanger, ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED);
    WiFi.onEvent(nodeChanger, ARDUINO_EVENT_WIFI_AP_STADISCONNECTED);

    if (!WiFi.config(
        Heartichoke_IP,
        IPAddress(192, 168, 1, 1),
        IPAddress(255, 255, 0, 0)
    ))
        Serial.println("STA Failed to configure");

    char ssid[] = "Garden Amenities";
    char pass[] = "Am3nit1es4b1tch35";

    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(String(WiFi.status()));
        delay(1000);
    }
    delay(1000);
    Serial.println();
    Serial.println("IP: " + WiFi.localIP().toString());
    Serial.println("Gateway: " + WiFi.gatewayIP().toString());

    //Create our Access Point as well
    WiFi.softAP(SSID, PASS);
    Serial.println("Begun access point at " + WiFi.softAPIP().toString());

    drawScreen();
}


void sendNode(int id, StaticJsonDocument<200> data) {
    if (id < 0) {
        throwError("No node id requested for send node request");
        return;
    }

    Serial.println("YAY! sending connection for + " + String(id));
    IPAddress to;
    to.fromString(connections[id][0]);

    int r = data["r"];
    int g = data["g"];
    int b = data["b"];

    Serial.println("Sending " + to.toString() + ": r" + String(r) + " g" + String(g) + " b" + String(b));


    char rgb[128];
    serializeJson(data["rgb"], rgb);

    UDP.broadcast(rgb);
    UDP.broadcastTo(rgb, 4211);
}

void processSpiritRequest(String bufferStr) {
    Serial.println("Received from Spirit: " + bufferStr);

    StaticJsonDocument<200> json;
    DeserializationError error = deserializeJson(json, bufferStr);
    if (error) {
        Serial.println("JSON ERROR!");
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        throwError("Json deserialization failed!");
        return;
    }

    if (json.containsKey("action")) {
        if (json["action"] == "SetRGB") {
            Serial.println("setting rgb...");
            if (!json.containsKey("location")) {
                throwError("Tried to set RGB without a location");
                return;
            }

            //We have to reserialize rgb since it's a
            char rgb[128];
            serializeJson(json["rgb"], rgb);

            const char* location = json["location"];

            Serial.print("rgb:");
            Serial.println(rgb);
            Serial.println(location);

            if (String(location) == "all") {
                Serial.println("Sending to all.");
                for (int i = 0; i < connectionsAvailable; i++) {
                    sendNode(i, json["rgb"]);
                }

                //                    if (connections[i][1].indexOf("RGB|") == 0)     
            } else
                sendNode(fetchNodeIdFor("RGB|" + String(location)), json["rgb"]);
        } else
            throwError("Requested action not recognized.");
    } else
        throwError("Spirit requested no action.");

}
void processUdp(AsyncUDPPacket packet) {
    //Validate the incoming IP address
    String source = "UNKNOWN";
    if (packet.remoteIP().toString().indexOf("128.199.7.114") == 0) {
        Serial.println("Received UDP request from Spirit");
        source = "Spirit";
    } else if (packet.remoteIP().toString().indexOf("10.10.10.") == 0) {
        source = "Node";
    }

    char* tmpStr = (char*) malloc(packet.length() + 1);
    memcpy(tmpStr, packet.data(), packet.length());
    tmpStr[packet.length()] = '\0'; // ensure null termination
    String dataString = String(tmpStr);
    free(tmpStr);

    Serial.println("Received " + dataString + " from UDP@" + packet.remoteIP().toString());

//    if (source == "LocalNode")
//        processLocalNode(dataString);

    if (source == "Spirit")
        processSpiritRequest(dataString);

    drawScreen();
}

void setup() {
    //First off start up our screen and load up our boot graphic
    tft.init();
    tft.setRotation(3);
    tft.setSwapBytes(true);

    drawScreen("Loading...");


    // Setup device button pins
//    pinMode(Button0, INPUT); //This one doesn't work for whatever reason
    pinMode(Button1, INPUT);


    //Start Serial and connect to the network
    Serial.begin(115200);
    connectToWifi();

    //Listen for UDP data
    //We may also need to listen on the AP IP
    if(UDP.listen(Heartichoke_IP, UDP_PORT)) {
        Serial.println("UDP listening locally on IP \"" + Heartichoke_IP.toString() + ":" + UDP_PORT + "\"");
        UDP.onPacket([](AsyncUDPPacket packet) {
            processUdp(packet);
        });
    }
}
void loop() {
    if (digitalRead(Button1) == LOW) {
        registerNodes();

        delay(500);
    }
}