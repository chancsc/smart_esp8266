//Project: Monitor water level (HC-SR04P) of Balacing tank and the Temperature of Water (DS18b20) / Ambient (DHT22) 
//         Send alert via Blynk app if water level is too high, or water temperature is higher than 28 C
//         Trigger a Tasmota Water Pump when water level exceed 80%

/*
//https://community.blynk.cc/t/water-tank-level-indicator-with-low-level-warning-notifications/26271

//v1.0 - Wifi Manager + Blynk
//v1.1 - added deep sleep function - https://community.blynk.cc/t/esp-deep-sleep/5622/13
//v1.2 - added run time change of deep sleep interval via UI setup
//v1.3 - rewrite using chatgpt, updated to Blynk IOT, remove notification related code, Blynk app can do that
//v1.3.1 - adopt old smoothing logic
//v1.4 - Add terminal widget, to accept input of water pump tasmota S26 IP address
//     - commented off sleep interval widget, not enough widget for free tier


//**** Blynk Virtual Pin Assignments****
// V1 - Tank Height (cm)
// V2 - Sensor distance from Tank (cm)
// V3 - Temperature
// V6 - Sleep interval (mins)  -- removed
// V9 - Water Level %
// V10 - Terminal input

Hardware: 
1. Wemos D1 Pro / Nodemcu, remember to put D0 -- RST to enable wake up
2. DS18B20 Temperature Sensor
3. Ultra Sonic Sensor

Ultra sonic Sensor: Vcc - 3v3, Gnd - Gnd, Echo - 4, Trigger - 5
DS18820 -  Vcc - D5 / GPIO14, Gnd - Gnd, Data - D4


*/
// Template ID, Device Name and Auth Token are provided by the Blynk.Cloud
// See the Device Info tab, or Template settings
#define BLYNK_TEMPLATE_ID           "template id"
#define BLYNK_DEVICE_NAME           "Quickstart Device"
#define BLYNK_AUTH_TOKEN            "token"

// Pin connected to the data pin of the Dallas temperature sensor (DS18b20)
#define temperaturePin 2    // GPIO2 = D4 on nodemcu
#define powerPin 14 //GPIO12 = D6, GPIO14 = D5, GPIO15 = D8


#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h> // Library for Blynk IoT
#include <OneWire.h> // Library for Dallas temperature sensor
#include <DallasTemperature.h> // Library for Dallas temperature sensor

//HC-SR04 ultra sonic sensor
//const int trigPin = 5; // Pin connected to the trigger pin of the ultrasonic sensor, GPIO5 = D1
//const int echoPin = 4; // Pin connected to the echo pin of the ultrasonic sensor, GPIO4 = D2

// Define pin numbers
const int echoPin = D6;
const int trigPin = D7;


float duration;
int distance;

int tankDepth = 100; // Depth of the tank in centimeters
int sensorHeight = 15; // Height of the ultrasonic sensor above the tank in centimeters
int sleepInterval = 5; //how often run the check / sleep for how long before wake up, in mins
int pumpUrl = V10;

char auth[] = BLYNK_AUTH_TOKEN;
//char ssid[] = "ssid";
//char pass[] = "pwd";
char ssid[] = "ssid";
char pass[] = "pwd";

// String variable to store input from Terminal Widget
String inputString = "";
String tasmotaUrl = "";           // String to store Tasmota device URL


// Create a OneWire object and a DallasTemperature object
OneWire oneWire(temperaturePin);
DallasTemperature sensors(&oneWire);

BLYNK_WRITE(V1) {
  // Update the tank depth when the value on virtual pin V1 changes
  tankDepth = param.asInt();
  Serial.print("Tank Depth: ");
  Serial.println(tankDepth);
}

BLYNK_WRITE(V2) {
  // Update the sensor height when the value on virtual pin V2 changes
  Serial.print("Sensor Height: ");
  Serial.println(sensorHeight);
  sensorHeight = param.asInt();
}

BLYNK_WRITE(V10) {                // Called every time the Terminal Widget sends data to virtual pin V10
  inputString = param.asStr();    // Store the input string in the inputString variable
  Serial.println("New value: " + inputString);  // Print the new value to Serial
  Blynk.virtualWrite(V10, inputString, true);  // Write the input string to virtual pin V10 with the SYNC flag
}

//
//BLYNK_WRITE(V6) {
//  sleepInterval = param.asInt();
//  Serial.print("Sleep Interval: ");
//  Serial.println(sleepInterval);
//}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200); // Initialize serial communication

  Serial.println("");
  Serial.println("Starting...");
  
  Blynk.begin(auth, ssid, pass); // Connect to WiFi and Blynk server
  // Sync the value of all virtual pins
  Blynk.syncVirtual(V1, V2, V10);

  // Initialize the Dallas temperature sensor
  sensors.begin();

  //Ultrasonic sensor
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // For temperature sensor
  pinMode(powerPin, OUTPUT);
  digitalWrite(powerPin, HIGH);  //provide power
  delay(1000);
  
  //Serial.println("Getting temperature");
  sensors.requestTemperatures();
  float temperature = sensors.getTempCByIndex(0);

  digitalWrite(powerPin, HIGH);  //provide power
  
  Serial.print("Temperature (C) = ");
  Serial.print(temperature);

  // Send the temperature to virtual pin V3 on the Blynk app or web interface
  Serial.println("");
//  Serial.println("Sending temperature data to Blynk");
  Blynk.virtualWrite(V3, temperature);

  // Check the water level and send a notification if necessary
  //getWaterLevel();

  //using diff algorithm
  Serial.println("\nMeasure Water Level");
  MeasureCmForSmoothing();

  Serial.print("Enter into deep sleep for: ");
  Serial.print(sleepInterval);
  Serial.print(" mins");
  Serial.println("");
  ESP.deepSleep(sleepInterval * 60 * 1000000); //deepSleep is microseconds 1 sec = 1000000
  delay(100);

}

void loop() {
  // put your main code here, to run repeatedly:

}

// Callback function to set the tank depth when a value is received on virtual pin V1
void setTankDepth(const BlynkParam &param) {
  tankDepth = param.asInt(); // Update the tank depth with the received value
}

float measureDistance() {
//  long sum = 0; // Sum of the readings
//  int count = 0; // Number of valid readings

  // Make 10 readings of the distance to the water surface
  //for (int i = 0; i < 10; i++) {
  
  // Clears the trigPin condition
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  // Sets the trigPin HIGH (ACTIVE) for 10 microseconds
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  // Reads the echoPin, returns the sound wave travel time in microseconds
  duration = pulseIn(echoPin, HIGH);
  // Calculating the distance
  distance = duration * 0.034 / 2; // Speed of sound wave divided by 2 (go and back)

  return distance;
  //}
}

void MeasureCmForSmoothing() {
  
  int smoothDistance = mesureDistance();
    
  Serial.print("Smoothed Distance: ");
  Serial.println(smoothDistance);

  if (smoothDistance > 0) {
    //tank height 100 cm, sensor is 20 cm above the tank
    int waterLevel = (tankDepth + sensorHeight - smoothDistance);
    Serial.print("Water Level (cm): ");
    Serial.println(waterLevel);
//    Serial.print("tankDepth: ");
//    Serial.println(tankDepth);
    //convert to %
    int waterLevelPercent = (waterLevel * 100) / tankDepth;;
    Serial.print("Water Level (%): ");
    Serial.println(waterLevelPercent);
  
    Blynk.virtualWrite(V9, waterLevelPercent); // virtual pin

    // Check if water level is more than 80%
    if (waterLevelPercent > 80) {
      // Get Tasmota device URL from Terminal Widget
      tasmotaUrl = getTasmotaUrl();
  
      // Trigger Tasmota device for 2 minutes if the Tasmota URL is not empty
      if (tasmotaUrl != "") {
        triggerTasmotaDevice(tasmotaUrl, 2);
      }
    }
  }
  
  delay(1000);
}

// Function to get Tasmota device URL from Terminal Widget
String getTasmotaUrl() {
  // Read input from Terminal Widget (assuming it's linked to virtual pin V10)
  String inputString = "";
  Blynk.syncVirtual(V10);  // Request latest value from server
  while (inputString.length() == 0) {
    if (Serial.available()) {
      inputString = Serial.readStringUntil('\n');
      inputString.trim();
    }
    Blynk.run();
  }
  return inputString;
}

// Function to trigger Tasmota device for a specified number of minutes
void triggerTasmotaDevice(String url, int minutes) {  
  // Construct URL with parameters
  String command = "http://" + url + "cm?cmnd=backlog%20Power%20On%3B%20Delay%20120000%3B%20Power%20Off%201";
  
  // Send HTTP GET request to trigger Tasmota device
  WiFiClient client;
  if (client.connect(url.c_str(), 80)) {
    client.print("GET " + command + " HTTP/1.1\r\n" +
                 "Host: " + url + "\r\n" +
                 "Connection: close\r\n\r\n");
    while (client.connected()) {
      if (client.available()) {
        String line = client.readStringUntil('\n');
        // Uncomment the following line to print the response from Tasmota device
        // Serial.println(line);
      }
    }
    client.stop();
  }
}


/**
 * mresure distnce average delay 10ms
 */
int mesureDistance() {
    const int ULTRASONIC_MIN_DISTANCE = sensorHeight;  //20
    const int ULTRASONIC_MAX_DISTANCE = tankDepth + sensorHeight;  //120

      int i, total_count = 0, total_distance = 0, distance, values[10], average, temp, new_count = 0;

      for(i=0; i<10; i++){
          distance = mesureSingleDistance();
          //check if out of range
          if(distance > ULTRASONIC_MIN_DISTANCE && distance < ULTRASONIC_MAX_DISTANCE) {
              total_distance += distance;
              values[total_count++]=distance;
              Serial.printf("Value %d...\n", distance);
          }
      }

      Serial.print("total_count = ");
      Serial.println(total_count);
      if(total_count > 0) {

          average = total_distance / total_count;
          total_distance = 0;
          Serial.printf("Average %d...\n", average);

          for(i=0; i<total_count; i++){
              temp = (average - values[i])*100/average;
              Serial.printf("Analyze %d....%d...\n", values[i],temp);
              if(temp < 10 && temp > -10){
                  total_distance += values[i];
                  new_count++;
              }
          }

          if(new_count > 0) {
//              Serial.print("total_distance = ");
//              Serial.println(total_distance);
//              Serial.print("new_count = ");
//              Serial.println(new_count);
              return total_distance / new_count;
          }
      }
      Serial.println("Sonar sensor out of range");
      return 0;  //return 0 meaning the sensor out of range
  }

int mesureSingleDistance() {
      long duration, distance;

      digitalWrite(trigPin, LOW);
      delayMicroseconds(2);
      digitalWrite(trigPin, HIGH);
      delayMicroseconds(10);
      digitalWrite(trigPin, LOW);
      duration = pulseIn(echoPin, HIGH);
      distance = (duration/2) / 29.1;

      Serial.print("Distance: ");
      Serial.println(distance);

      return distance;
  }
