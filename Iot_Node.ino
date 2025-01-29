#include <WiFi.h>
#include <HTTPClient.h>
#include <MFRC522.h>
#include <ESP32Servo.h>

#define SS_PIN 5   // SDA for RFID
#define RST_PIN 4  // RST for RFID

#define BUTTON_INTENSO_PIN 14  // Button for "Intenso"

#define BUZZER_PIN 15  // Buzzer GPIO
#define SERVO_PIN 13   // Servo GPIO

// RFID reader instance
MFRC522 rfid(SS_PIN, RST_PIN);
// Servo instance
Servo myServo;

// WiFi credentials
const char* ssid = "your-SSID";
const char* password = "your-PASSWORD";

// Backend endpoints
const char* backend_verify_url = "http://192.168.1.18:8000/api/verify-rfid";
const char* backend_coffee_url = "http://192.168.1.18:8000/api/select-coffee";
//Machine Location
const char* machineLocation = "Office Floor 1";
// Variables
bool accessGranted = false;
unsigned long startTime = 0;
bool buttonPressed = false;
String selectedCoffee = "";
String currentTagID = "";

void setup() {
  Serial.begin(115200);

  // Initialize RFID
  SPI.begin();
  rfid.PCD_Init();

  // Initialize Servo
  myServo.attach(SERVO_PIN);
  myServo.write(0);  // Initial position

  // Set buttons as input
  pinMode(BUTTON_INTENSO_PIN, INPUT_PULLUP);

  // Set buzzer as output
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Connect to Wi-Fi
  connectToWiFi();

  Serial.println("Ready to scan RFID tag...");
}

void loop() {
   // Debug: Check the button state
 int buttonState = digitalRead(BUTTON_INTENSO_PIN);
 // Serial.println("Button State: " + String(buttonState));  // Debugging button state

  // Check for a new RFID card
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    currentTagID = getRFIDTag();
    Serial.println("RFID Tag detected: " + currentTagID);

    // Verify tag with backend
    if (verifyTagWithBackend(currentTagID)) {
      Serial.println("Access Granted. Waiting for button press...");
      accessGranted = true;
      startTime = millis();  // Start 10-second timer
    } else {
      Serial.println("Access Denied.");
      buzzFor(5000);  // Buzz for 5 seconds if unauthorized
      accessGranted = false;
    }
    rfid.PICC_HaltA();
  }

  // Check if access was granted and handle button presses
  if (accessGranted) {
    unsigned long elapsedTime = millis() - startTime;
    
   // Serial.println(buttonState);
    // Check if any button is pressed
    if (digitalRead(BUTTON_INTENSO_PIN) == LOW) {
      selectedCoffee = "Intenso";
      buttonPressed = true;
      Serial.println("Button pressed, sending coffee selection...");
    }else{
      buttonPressed = false;
    }
    // If a button is pressed within 10 seconds, send the coffee type and activate the servo
    if (buttonPressed && elapsedTime <= 10000) {
      Serial.println("Selected coffee: " + selectedCoffee);
      sendCoffeeSelectionToBackend(currentTagID, selectedCoffee);
      activateServo();
      resetSystem();
    } else if (elapsedTime > 10000) {
      Serial.println("Timeout! Please pass the RFID tag again.");
      resetSystem();
    }
  }
}

// Function to get RFID tag ID
String getRFIDTag() {
  String tagID = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    tagID += String(rfid.uid.uidByte[i], HEX);
  }
  return tagID;
}

// Function to verify the tag with the backend
bool verifyTagWithBackend(String tagID) {
  HTTPClient http;
  String fullUrl = String(backend_verify_url) + "?rfid_tag=" + tagID;


  http.begin(fullUrl);  // Add tag as query parameter

  int httpCode = http.GET();
  Serial.println("HTTP Code: " + String(httpCode));

  if (httpCode == 200) {  // HTTP OK
    String response = http.getString();
    Serial.println("Backend response: " + response);
    // Parse JSON response (assuming backend returns a JSON response)
    if (response.indexOf("success\":true") >= 0) {
      return true;
    }
  }
  String response = http.getString();
  Serial.println("Backend response: " + response);
  return false;
}

// Function to send coffee selection to the backend
void sendCoffeeSelectionToBackend(String tagID, String coffeeType) {
  HTTPClient http;
  http.begin(backend_coffee_url);  // Coffee selection endpoint
  Serial.println(backend_coffee_url);
  // Set content type to JSON
  http.addHeader("Content-Type", "application/json");

  // Create JSON payload
  String payload = "{\"rfid_tag\":\"" + String(tagID) +"\" ,\"coffee_type\":\"" + String(coffeeType) + "\", \"machine_location\":\"" + String(machineLocation) + "\"}";
 
  // Send POST request
  // Debug: Print the payload
  Serial.println("Sending payload: " + payload);
  int httpCode = http.POST(payload);

  // Check if the response is a redirect
  if (httpCode == 302) {
    String response = http.getString();
    Serial.println("Backend response: " + response);
    String redirectURL = http.getLocation();  // Get redirect URL from Location header
    Serial.println("HTTP Code: 302, Redirecting to: " + redirectURL);
  } else {
    String response = http.getString();  // Get the response from server
    Serial.println("HTTP Code: " + String(httpCode));
    Serial.println("Backend response: " + response);
  }
  if (httpCode == 200) {
    Serial.println("Coffee selection sent successfully.");
  } else {
    Serial.println("Failed to send coffee selection.");
  }

  http.end();
}

// Function to activate the servo
void activateServo() {
  myServo.write(90);  // Rotate to 90 degrees
  delay(2000);        // Hold for 2 seconds
  myServo.write(0);   // Return to original position
}

// Function to buzz for a certain duration
void buzzFor(int duration) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(duration);
  digitalWrite(BUZZER_PIN, LOW);
}

// Function to reset the system state
void resetSystem() {
  accessGranted = false;
  buttonPressed = false;
  selectedCoffee = "";
  currentTagID = "";
  startTime = 0;
  Serial.println("System reset. Please scan RFID again.");
}

// Function to connect to Wi-Fi
void connectToWiFi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi.");
}