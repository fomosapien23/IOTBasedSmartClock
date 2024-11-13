// Blynk and WiFi configuration
#define BLYNK_TEMPLATE_ID ""
#define BLYNK_TEMPLATE_NAME ""
#define BLYNK_AUTH_TOKEN ""

#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <DHT.h>
#include <Wire.h>
#include <RTClib.h>
#include <ThingSpeak.h>

// WiFi credentials
char ssid[] = "";
char pass[] = "";

// ThingSpeak API details
unsigned long myChannelNumber = ;
const char* myWriteAPIKey = "";

// Pin Definitions and Constants
#define BUZZER_PIN D4          // Buzzer pin is now D4
#define BUTTON_PIN D3          // Push button connected to D3
#define DHTPIN D6              // DHT11 data pin connected to D6
#define DHTTYPE DHT11
#define CLK_PIN D5             // LED Matrix CLK connected to D5
#define DATA_PIN D7            // LED Matrix DIN connected to D7
#define CS_PIN D8              // LED Matrix CS connected to D8

// DHT and RTC setup
RTC_DS3231 rtc;
BlynkTimer timer;
DHT dht(DHTPIN, DHTTYPE);
WiFiClient client;

// LED Matrix setup with MD_Parola
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
MD_Parola myDisplay = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

#define SCROLL_DELAY 75
String messages[] = {"IOT based Smart Clock", "", ""}; // Initial message "IOT based Smart Clock"
int currentMessageIndex = 0;      // To keep track of the message to display
bool displayAppMessage = false;   // Flag to display app message once

// Function declarations
void prepareMessages();
void sendThingSpeakData(String message);
String formatTime(DateTime now);
String formatDate(DateTime now);
void checkButtonPress();
void updateDisplayContent();
void sendSensorData();
BLYNK_WRITE(V2); // Declare virtual pin V2 to receive message from Blynk app

// Setup function
void setup() {
  Serial.begin(9600);
  WiFi.begin(ssid, pass);

  // Wait until WiFi is connected
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  dht.begin();

  // RTC setup
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  ThingSpeak.begin(client);

  // Initialize the LED matrix display
  myDisplay.begin();
  myDisplay.setIntensity(8);  // Set the brightness level
  myDisplay.displayClear();
  myDisplay.displayText(messages[currentMessageIndex].c_str(), PA_CENTER, SCROLL_DELAY, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);

  // Prepare initial messages for display
  prepareMessages();

  // Set up the timer to send sensor data periodically
  timer.setInterval(18000L, sendSensorData);
}

void loop() {
  Blynk.run();
  timer.run();

  // Check for button press
  checkButtonPress();

  // Update the LED matrix display if the current animation is complete
  if (myDisplay.displayAnimate()) {
    if (displayAppMessage) {
      // Display app message once and then reset
      myDisplay.displayText(messages[2].c_str(), PA_CENTER, SCROLL_DELAY, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
      displayAppMessage = false;
    } else {
      // Cycle through normal messages
      prepareMessages();  // Ensure messages are up-to-date
      currentMessageIndex = (currentMessageIndex + 1) % 3;
      myDisplay.displayText(messages[currentMessageIndex].c_str(), PA_CENTER, SCROLL_DELAY, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
    }
  }
}

// Button logic to trigger buzzer, send data to Blynk, and log to ThingSpeak
void checkButtonPress() {
  static unsigned long lastButtonPressTime = 0;
  unsigned long currentTime = millis();

  // Check if button is pressed (active LOW) and debounce
  if (digitalRead(BUTTON_PIN) == LOW && (currentTime - lastButtonPressTime > 500)) {
    lastButtonPressTime = currentTime;  // Update the last press time

    tone(BUZZER_PIN, 2500);  // Start the buzzer sound
    Serial.println("Button Pressed! Buzzer Activated!");

    // Log the event in Blynk to trigger a notification in the app
    Blynk.logEvent("panicbutton", "Emergency! Button Pressed! Immediate action required.");

    // Send custom message data to ThingSpeak when button is pressed
    ThingSpeak.setField(3, "Button Pressed");
    int result = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
    if (result == 200) {
      Serial.println("Button press event successfully logged to ThingSpeak.");
    } else {
      Serial.print("Error logging to ThingSpeak. Error code: ");
      Serial.println(result);
    }

    // Stop the buzzer after 200 ms
    timer.setTimeout(200L, []() {
      noTone(BUZZER_PIN);
    });
  }
}

// Prepare default messages if no app message is active
void prepareMessages() {
  if (!displayAppMessage) {  // Only update messages if no app message is active
    DateTime now = rtc.now();
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    if (!isnan(temperature) && !isnan(humidity)) {
      messages[0] = "IOT based Smart Clock";  // Initial message
      messages[1] = formatDate(now) + " " + formatTime(now);  // Date and time message
      messages[2] = "Temp: " + String(temperature, 1) + "C Hum: " + String(humidity, 1) + "%";
    } else {
      Serial.println("Failed to read from DHT sensor!");
    }
  }
}

// Function to send sensor data to ThingSpeak
void sendSensorData() {
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  DateTime now = rtc.now();
  String timeStr = formatTime(now);
  String dateStr = formatDate(now);

  Serial.print("Time: ");
  Serial.print(timeStr);
  Serial.print(", Date: ");
  Serial.println(dateStr);
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.print(" C, Humidity: ");
  Serial.print(humidity);
  Serial.println(" %");

  Blynk.virtualWrite(V0, temperature);
  Blynk.virtualWrite(V1, humidity);

  ThingSpeak.setField(1, temperature);
  ThingSpeak.setField(2, humidity);
  int result = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  if (result == 200) {
    Serial.println("Data Successfully Sent to ThingSpeak.");
  } else {
    Serial.print("Error sending data to ThingSpeak. Error code: ");
    Serial.println(result);
  }
}

// Function to format the time for display
String formatTime(DateTime now) {
  int hour = now.hour();
  String period = "AM";
  if (hour >= 12) {
    period = "PM";
    if (hour > 12) hour -= 12;
  } else if (hour == 0) {
    hour = 12;
  }
  return String(hour) + ":" + String(now.minute()) + ":" + String(now.second()) + " " + period;
}

// Function to format the date for display
String formatDate(DateTime now) {
  const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  return String(now.day()) + " " + months[now.month() - 1] + " " + String(now.year());
}

// Blynk function to handle receiving a message from Blynk app
BLYNK_WRITE(V5) {
  String message = param.asString();  // Get the message sent from the app
  messages[1] = "Message:";
  messages[2] = message;
  displayAppMessage = true;  // Set flag to display message once
  myDisplay.displayClear();  // Clear display for the new message
  myDisplay.displayText(messages[1].c_str(), PA_CENTER, SCROLL_DELAY, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
}
