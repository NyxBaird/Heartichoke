#include <TFT_eSPI.h>
#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <ArduinoHttpClient.h>
#include "esp_wifi.h"

char packetBuffer[255];
const char* ssid       = "Garden Amenities";
const char* password   = "Am3nit1es4b1tch35";

IPAddress softstaticIP(192,168,5,1);
IPAddress softgateway(192,168,5,1);
IPAddress softsubnet(255,255,255,0);

WiFiUDP udpout;
WiFiUDP udpin;

#define UDP_PORT_OUT 4211
#define UDP_PORT_IN  4210


char buff[512];
int vref = 1100;
int btnCick = false;

//These are all of the devices Heartichoke will speak to...
//connections[nodeID] = [Node IP, Node Identifier]
const int connectionsAvailable = 10;
String connections[connectionsAvailable][2];

TFT_eSPI tft = TFT_eSPI(135, 240); // Invoke custom library

//This does nothing and is just here to allow me future wiggle room on how errors are handled
void throwError(String error) {
    Serial.println("ERROR: " + error);
}
void setup()
{
    Serial.begin(115200);

    //connect to WiFi
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);


    for (int x=1; x <100; x++)
    {
        delay(500);
        if (WiFi.status() == WL_CONNECTED)
        {
            x=200;

            Serial.println(" CONNECTED");
        }
    }

    if (WiFi.status() == WL_CONNECTED)
    {
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
        Serial.println(WiFi.softAP("heartichoke") ? "Ready" : "Failed!");
        Serial.print("Soft-AP IP address = ");
        Serial.println(WiFi.softAPIP());

    }
    udpout.begin(UDP_PORT_OUT);
    udpin.begin(UDP_PORT_IN);

}

void loop()
{
    delay(2000);
    Serial.print("We have " );
    Serial.print(WiFi.softAPgetStationNum());
    Serial.println(" Node/s connected ");
    IPAddress node_1_IP(192, 168, 5, 2);
    udpout.beginPacket(node_1_IP, 4211);
    udpout.println("Register_node");
    udpout.endPacket();
    delay(1000);
    udpin.parsePacket();
    udpin.read(packetBuffer, 255);

    if (packetBuffer[0] != 0) {
        Serial.println("Received UDP!" + String(packetBuffer));
    }

    delay(2000);
}