#include <WiFi.h>
#include "VideoStream.h"
#include "AmebaFatFS.h"

char ssid[] = "Unagi";           // Your network SSID
char pass[] = "ApartmentE7";     // Your network password
int status = WL_IDLE_STATUS;     // Wi-Fi status indicator
uint32_t img_addr = 0;
uint32_t img_len = 0;

WiFiServer server(80);
#define LED_PIN LED_B
#define CHANNEL  0
#define FILENAME "/image.jpg"

VideoSetting config(VIDEO_FHD, CAM_FPS, VIDEO_JPEG, 1);
AmebaFatFS fs;

void setup() {
    Serial.begin(115200);
    Camera.configVideoChannel(CHANNEL, config);
    Camera.videoInit();
    Camera.channelBegin(CHANNEL);
    pinMode(LED_PIN, OUTPUT);

    while (status != WL_CONNECTED) {
        Serial.print("Connecting to: ");
        Serial.println(ssid);
        status = WiFi.begin(ssid, pass);
        delay(10000);
    }

    server.begin();
    printWifiStatus();
}

void loop() {
    WiFiClient client = server.available();

    if (client) {
        Serial.println("New Client Connected");
        String request = "";
        while (client.connected()) {
            if (client.available()) {
                char c = client.read();
                request += c;
                Serial.write(c);

                if (c == '\n' && request.endsWith("\r\n\r\n")) {
                    if (request.indexOf("GET /capture") >= 0) {
                        Serial.println("Capture request received");
                        captureImage();
                        
                        // Redirect to homepage to display the image
                        client.println("HTTP/1.1 303 See Other");
                        client.println("Location: /");
                        client.println();
                    } else if (request.indexOf("GET /image.jpg") >= 0) {
                        sendCapturedImage(client);
                    } else {
                        sendHTML(client);
                    }
                    break;
                }
            }
        }
        client.stop();
        Serial.println("Client Disconnected");
    }
}

void captureImage() {
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);

    // Capture image in memory
    Camera.getImage(CHANNEL, &img_addr, &img_len);

    // Save to SD card after sending it to the client
    saveImageToSD();
}

void saveImageToSD() {
    fs.begin();
    File file = fs.open(String(fs.getRootPath()) + String(FILENAME));
    
    if (!file) {
        Serial.println("Failed to open file for writing");
        return;
    }

    file.write((uint8_t *)img_addr, img_len);
    file.close();
    fs.end();
}

void sendHTML(WiFiClient &client) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-type:text/html");
    client.println();

    client.println("<!DOCTYPE html><html><head>");
    client.println("<title>AMB82 Mini Image Capture</title>");
    client.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
    client.println("<style>");
    client.println("body { text-align: center; font-family: Arial, sans-serif; margin: 20px; }");
    client.println(".button { padding: 15px 30px; font-size: 20px; color: white; background-color: blue; border: none; border-radius: 8px; cursor: pointer; }");
    client.println(".button:hover { background-color: darkblue; }");
    client.println("</style>");
    client.println("</head><body>");
    client.println("<h2>ESP32 Image Capture</h2>");
    client.println("<button class='button' onclick=\"window.location.href='/capture'\">Capture Image</button>");
    client.println("<br><br>");
    client.println("<img src='/image.jpg' style='max-width: 50%; height: auto;'>");
    client.println("</body></html>");
    client.println();
}

void sendCapturedImage(WiFiClient &client) {
    fs.begin();
    File file = fs.open(String(fs.getRootPath()) + String(FILENAME));
    
    if (!file) {
        Serial.println("Failed to open file for reading");
        client.println("HTTP/1.1 404 Not Found");
        client.println("Content-Type: text/plain");
        client.println();
        client.println("No image available");
        return;
    }

    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: image/jpeg");
    client.println("Connection: close");
    client.println();

    uint8_t buffer[512];
    while (file.available()) {
        int bytesRead = file.read(buffer, sizeof(buffer));
        client.write(buffer, bytesRead);
    }

    file.close();
    fs.end();
}

void printWifiStatus() {
    Serial.print("Connected to: ");
    Serial.println(WiFi.SSID());
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    Serial.print("Access Webpage at: http://");
    Serial.println(WiFi.localIP());
}
