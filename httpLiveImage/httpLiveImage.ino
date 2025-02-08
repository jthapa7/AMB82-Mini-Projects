#include <WiFi.h>
#include "VideoStream.h"
#include "AmebaFatFS.h"

char ssid[] = "Unagi";           // Your network SSID
char pass[] = "ApartmentE7";     // Your network password
int status = WL_IDLE_STATUS;     // Wi-Fi status indicator
uint32_t img_addr = 0;
uint32_t img_len = 0;

WiFiServer server(80);
#define CHANNEL  0
#define FILENAME "/image.jpg"

// Set video resolution to VGA (640x480) for faster streaming
VideoSetting config(VIDEO_VGA, CAM_FPS, VIDEO_JPEG, 1);  // Decrease the resolution for faster FPS
AmebaFatFS fs;

void setup() {
    Serial.begin(115200);
    Camera.configVideoChannel(CHANNEL, config);
    Camera.videoInit();
    Camera.channelBegin(CHANNEL);

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
                    if (request.indexOf("GET /save") >= 0) {
                        Serial.println("Save request received");
                        saveImageToSD();
                        client.println("HTTP/1.1 303 See Other");
                        client.println("Location: /");
                        client.println();
                    } else if (request.indexOf("GET /live_image") >= 0) {
                        sendLiveImage(client);
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

void saveImageToSD() {
    // Capture image in memory
    Camera.getImage(CHANNEL, &img_addr, &img_len);

    // Save image to SD card
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
    client.println("<h2>AMB82 Mini Image Capture</h2>");
    client.println("<button class='button' onclick=\"window.location.href='/save'\">Save Image to SD Card</button>");
    client.println("<br><br>");

    // Use JavaScript to refresh the live image every 50ms (20 FPS)
    client.println("<script>");
    client.println("setInterval(function() {");
    client.println("document.getElementById('liveImage').src = '/live_image?" + String(millis()) + "';");
    client.println("}, 50);"); // 50ms delay for 100 FPS
    client.println("</script>");

    // Display live image with id for JavaScript to update
    client.println("<img id='liveImage' src='/live_image' style='width: 600px; height: auto;'>");
    client.println("</body></html>");
    client.println();
}

void sendLiveImage(WiFiClient &client) {
    // Capture a fresh image from the camera and send it to the client
    Camera.getImage(CHANNEL, &img_addr, &img_len);

    // Send the image to the client
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: image/jpeg");
    client.println("Cache-Control: no-cache, no-store, must-revalidate"); // Prevent caching
    client.println("Pragma: no-cache");
    client.println("Expires: 0");
    client.println("Connection: close");
    client.println();

    uint8_t buffer[512];
    int bytesRead;
    for (uint32_t i = 0; i < img_len; i += sizeof(buffer)) {
        bytesRead = (i + sizeof(buffer)) > img_len ? img_len - i : sizeof(buffer);
        memcpy(buffer, (uint8_t *)img_addr + i, bytesRead);
        client.write(buffer, bytesRead);
    }
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
