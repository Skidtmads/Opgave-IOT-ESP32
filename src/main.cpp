#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <SD.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// SD Kort
const int chipSelect = 5;

// DS18B20 temperatur sensor
const int oneWireBus = 4;
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

// WiFi
const char* ssid = "Henriksens";
const char* password = "Ypn@sted5";
WebServer server(80);

// Data opsamling/indsamling
const int interval = 300000; // 5 minutter i millisekunder
unsigned long previousMillis = 0;
float temperatureData[36]; // De sidste tre timers data skal gemmes på SD-kortet (5 min interval = 12 datapunkter i timen)
int dataIndex = 0;

// Deklaration af funktioner
void handleRoot();
void handleTemperature();

const char webpage[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
  <title>Temperatur Monitor</title>
  <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
</head>
<body>
  <h1>Temperatur Monitor</h1>
  <canvas id="tempChart" width="400" height="200"></canvas>
  <script>
    async function fetchData() {
      const response = await fetch('/temperature');
      const data = await response.json();
      return data;
    }

    async function drawChart() {
      const data = await fetchData();
      const ctx = document.getElementById('tempChart').getContext('2d');
      const chart = new Chart(ctx, {
        type: 'line',
        data: {
          labels: data.labels,
          datasets: [{
            label: 'Temperature (°C)',
            data: data.temperatures,
            borderColor: 'rgba(75, 192, 192, 1)',
            borderWidth: 1,
            fill: false
          }]
        },
        options: {
          scales: {
            x: {
              type: 'time',
              time: {
                unit: 'minute'
              }
            }
          }
        }
      });
    }

    drawChart();
    setInterval(drawChart, 300000); // Refresh the chart every 5 minutes
  </script>
</body>
</html>
)=====";

void setup() {
  Serial.begin(115200);

  // Initialisér SD kortet
  if (!SD.begin(chipSelect)) {
    Serial.println("SD kortet kunne ikke initialiseres");
    return;
  }

  // Initialisér temperatur sensoren
  sensors.begin();

  // Set up af WiFi access point
  WiFi.softAP(ssid, password);
  Serial.println("Access Point Started");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  // Opsætning af webserver-ruter
  server.on("/", handleRoot);
  server.on("/temperature", handleTemperature);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    sensors.requestTemperatures();
    float tempC = sensors.getTempCByIndex(0);

    // Gem temperaturmålingerne på SD-kortet
    File dataFile = SD.open("/temperature.txt", FILE_WRITE);
    if (dataFile) {
      dataFile.print("Temperature: ");
      dataFile.print(tempC);
      dataFile.println("°C");
      dataFile.close();
      Serial.println("Data skrevet til SD kort");
    } else {
      Serial.println("Filen kunne ikke åbnes");
    }

    // Gem temperaturen til array
    temperatureData[dataIndex] = tempC;
    dataIndex = (dataIndex + 1) % 36; // 36 data punkter i tre timer (5 min interval)
  }
}

void handleRoot() {
  server.send(200, "text/html", webpage);
}

void handleTemperature() {
  StaticJsonDocument<1024> doc; // Opdateret til at benytte StaticJsonDocument
  JsonArray temps = doc.createNestedArray("temperatures");
  JsonArray labels = doc.createNestedArray("labels");
  unsigned long currentMillis = millis();
  for (int i = 0; i < 36; i++) {
    int index = (dataIndex + i) % 36;
    temps.add(temperatureData[index]);
    labels.add(currentMillis - (36 - i) * interval);
  }
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}