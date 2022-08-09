#include <TFT_eSPI.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiAP.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <ArduinoHttpClient.h>


#include "esp_wifi.h"
#include "heartichoke.h"

//This does nothing and is just here to allow me future wiggle room on how errors are handled
void throwError(String error) {
    Serial.println("ERROR: " + error);
}//Declare this at the top so it can be used wherever needed

const int Button0 = 0;
const int Button1 = 35;

TFT_eSPI tft = TFT_eSPI();

char packetBuffer[255];
const char* ssid       = "Garden Amenities";
const char* password   = "Am3nit1es4b1tch35";

IPAddress softstaticIP(192,168,5,1);
IPAddress softgateway(192,168,5,1);
IPAddress softsubnet(255,255,255,0);

WiFiUDP udpIn;
#define UDP_PORT_IN  4210


//These are all of the devices Heartichoke will speak to...
struct RegisteredDevice{
    bool connected = false;
    String identifier;
    IPAddress ip;
    String mac;
    int port{};
    WiFiUDP udpOut;
    void sendData(const String& data) {
        if (!connected) {
            throwError("Tried to send \"" + data + "\" to " + identifier + " which is not connected");
            return;
        }

        Serial.println("Sending " + ip.toString() + "@" + port + ": " + data);
        udpOut.begin(port);
        udpOut.beginPacket(ip, port);
        udpOut.println(data);
        udpOut.endPacket();
        udpOut.stop();
    }
};
const int connectionsAvailable = 10;
RegisteredDevice connections[connectionsAvailable];

//Heartichokes designated IP Address
IPAddress Heartichoke_IP(192, 168, 1, 216);
String version = "1.12";

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

void registerNodes() {
    Serial.println("Registering nodes on core " + String(xPortGetCoreID()));

    wifi_sta_list_t wifi_sta_list;
    tcpip_adapter_sta_list_t adapter_sta_list;

    memset(&wifi_sta_list, 0, sizeof(wifi_sta_list));
    memset(&adapter_sta_list, 0, sizeof(adapter_sta_list));

    esp_wifi_ap_get_sta_list(&wifi_sta_list);
    tcpip_adapter_get_sta_list(&wifi_sta_list, &adapter_sta_list);

    for (int i = 0; i < adapter_sta_list.num; i++) {

        tcpip_adapter_sta_info_t station = adapter_sta_list.sta[i];

        Serial.print("station nr ");
        Serial.println(i);

        Serial.print("MAC: ");

        for(int i = 0; i< 6; i++){

            Serial.printf("%02X", station.mac[i]);
            if(i<5)Serial.print(":");
        }

        Serial.print("\nIP: ");
        Serial.println(ip4addr_ntoa(reinterpret_cast<const ip4_addr_t *>(&(station.ip))));
    }

    //Check if connections are active
//    if (adapter_sta_list.num > 0) {
//        for (auto &connection: connections) {
//            connection.connected = Ping.ping(connection.ip, 1);
//            if (connection.connected)
//                Serial.println("Successfully pinged " + connection.ip.toString() + " with an avg respond time of " + String(Ping.averageTime()) + "ms");
//        }
//    }

    Serial.println("Connected devices: ");
    for (int c = 0; c < connectionsAvailable; c++)
        if (connections[c].connected)
            Serial.println("Connections[" + String(c) + "] = [" + connections[c].identifier + ", " + connections[c].ip.toString() + ", " + connections[c].mac + "]");

} //Registers our connected nodes in the global connections[] array

void nodeChanger(WiFiEvent_t event, WiFiEventInfo_t info) {
    Serial.println("Event captured!");
    registerNodes();
} //Is called on node connection updates to re-register nodes

void connectToWifi() {
    WiFi.mode(WIFI_STA);

    if (!WiFi.config(
        Heartichoke_IP,
        IPAddress(192, 168, 1, 1),
        IPAddress(255, 255, 0, 0)
    ))
        Serial.println("STA Failed to configure");

    WiFi.begin(ssid, password);
    for (int x = 1; x < 100; x++) {
        delay(500);
        if (WiFi.status() == WL_CONNECTED) {
            x = 200;
            Serial.println(" CONNECTED");
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        Serial.print("Connected, IP address: ");
        Serial.println(WiFi.localIP());
        Serial.print("MAC Address: ");
        Serial.println(WiFi.macAddress());
        Serial.print("Gateway IP: ");
        Serial.println(WiFi.gatewayIP());
        Serial.print("DNS Server: ");
        Serial.println(WiFi.dnsIP());
        /*SoftAP configuration */
        Serial.print("Setting soft-AP ... ");
        WiFi.softAPConfig(softstaticIP, softgateway, softsubnet);
        Serial.println(WiFi.softAPConfig(softstaticIP, softgateway, softsubnet) ? "Ready" : "Failed!");
        delay(1000);   // hack for wrong address WiFi.softAPConfig needs time to start/finish
        Serial.println(WiFi.softAP("Heartichoke") ? "Ready" : "Failed!");
        Serial.print("Soft-AP IP address = ");
        Serial.println(WiFi.softAPIP());
    }

    drawScreen();
}

String startRequest() {
    //Validate the incoming IP address
    if (udpIn.remoteIP().toString().indexOf("128.199.7.114") == 0)
        return "Spirit";
    else {

        //Check if it's one of our registered connections
        for (auto & connection : connections) {
            if (connection.ip == udpIn.remoteIP()) {
                if (connection.identifier.length() > 0)
                    return connection.identifier;
                else
                    return "UnidentifiedRegisteredNode";
            }
        }

        return "UNKNOWN";
    }
} //First things we have to do before every incoming UDP request
void endRequest() {
    packetBuffer[0] = 0; //Clear our UDP buffer
} //Final things we have to do after every incoming UDP request

void processSpiritRequest(String bufferStr) {
    Serial.println("Received from Spirit: " + bufferStr);

    StaticJsonDocument<200> json;
    DeserializationError error = deserializeJson(json, bufferStr);
    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        endRequest();
        return;
    }

    if (json.containsKey("action")) {
        if (json["action"] == "SetRGB") {
            if (!json.containsKey("location")) {
                throwError("Tried to set RGB without a location");
                return;
            }

            if (json["location"] == "all") {
                for (auto & connection : connections) {
                    if (connection.identifier.indexOf("RGB|") == 0)
                        connection.sendData(json["rgb"]);
                }
            } else {
                String location = json["location"];
                for (auto & connection : connections) {
                    if (connection.identifier.indexOf(location) > -1)
                        connection.sendData(json["rgb"]);
                }
            }
        } else
            throwError("Requested action not recognized.");
    } else
        throwError("Spirit requested no action.");
}

void setup() {
    tft.init();
    tft.setRotation(3);
    tft.setSwapBytes(true);

    drawScreen("Loading...");

    Serial.begin(115200);
    pinMode(Button1, INPUT);

    WiFi.onEvent(nodeChanger, ARDUINO_EVENT_WIFI_AP_STACONNECTED);
    WiFi.onEvent(nodeChanger, ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED);
    WiFi.onEvent(nodeChanger, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

    //connect to WiFi
    connectToWifi();

    //Register any nodes Heartichoke will need to be able to talk to here
    connections[0].identifier = "RGB|LivingRoom";
    connections[0].ip = IPAddress(192, 168, 5, 2);
    connections[0].mac = "CC:50:E3:28:3C:20";
    connections[0].port = 4211;

    connections[1].identifier = "RGB|Development";
    connections[1].ip = IPAddress(192, 168, 5, 3);
    connections[1].mac = "48:55:19:13:06:78";
    connections[1].port = 4212;

    //Start up our UDP connections
    udpIn.begin(UDP_PORT_IN); //Start our incoming port
}
void loop()
{
    udpIn.parsePacket();
    udpIn.read(packetBuffer, 255);
    if (packetBuffer[0] != 0) {
        Serial.println("Received request on core " + String(xPortGetCoreID()));

        String from = startRequest();
        if (from == "UNKNOWN") {
            throwError("Denied request from unknown source.");
            endRequest();
            return;
        }

        Serial.println("Processing data from " + from);

//        if (from == "LocalNode")
//            processLocalNode(String(packetBuffer));

        if (from == "Spirit")
            processSpiritRequest(String(packetBuffer));

        endRequest();
    }

    if (digitalRead(Button1) == LOW) {
        Serial.println("Sending UDP...");

        //Get this through connections
        connections[0].sendData("Data from the heartichoke!");

        delay(500);
    }
}