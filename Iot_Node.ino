#include <Wire.h>
#include <Adafruit_PN532.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ESP32Servo.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

// Pin Definitions
#define BUTTON_INTENSO_PIN 14  // Button for "Intenso"
#define BUZZER_PIN 15         // Buzzer GPIO
#define SERVO_PIN 13          // Servo GPIO

// PN532 Pins
#define PN532_SCK (18)
#define PN532_MISO (19)
#define PN532_MOSI (23)
#define PN532_SS (27)

// RFID reader instance
Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);
// Servo instance
Servo myServo;

// WiFi credentials
const char* ssid = "test";
const char* password = "12345678";

// Backend endpoints
const char* backend_verify_url = "http://127.0.0.1:8000/api/verify-rfid";
const char* backend_coffee_url = "http://127.0.0.1:8000/api/select-coffee";
// Machine Location
const char* machineLocation = "Office A - Floor 1";

// Variables
volatile bool accessGranted = false;
volatile bool buttonPressed = false;
String selectedCoffee = "";
String currentTagID = "";
unsigned long startTime = 0;

// Timer for timeout
esp_timer_handle_t timeout_timer;

// Task handles
TaskHandle_t RFIDTaskHandle = NULL;
TaskHandle_t ButtonTaskHandle = NULL;

// Timer callback for checking timeout
void IRAM_ATTR timeout_callback(void* arg) {
  if (accessGranted) {
    Serial.println("Timeout! Please pass the RFID tag again.");
    resetSystem();
  }
}

// Interrupt service routine (ISR) for button press
void IRAM_ATTR buttonISR() {
  if (accessGranted && digitalRead(BUTTON_INTENSO_PIN) == LOW) {
    selectedCoffee = "Intenso";
    buttonPressed = true;
  }
}
// Variables to track button press timeout
unsigned long buttonPressStartTime = 0;
const unsigned long buttonPressTimeout = 10000;  // 10 seconds timeout for button press

// RFID Task: Handles RFID reading
void RFID_Task(void *pvParameters) {
  while (1) {
    uint8_t success;
    uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
    uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on card type)

    success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
    if (success) {
      currentTagID = getRFIDTag(uid, uidLength);
      Serial.println("RFID Tag detected: " + currentTagID);

      // Verify tag with backend
      if (verifyTagWithBackend(currentTagID)) {
        Serial.println("Access Granted. Waiting for button press...");
        accessGranted = true;
        buttonPressStartTime = millis();  // Start the 10-second timer
        esp_timer_stop(timeout_timer);  // Stop the timer while waiting for button press
      } else {
        Serial.println("Access Denied.");
        buzzFor(5000);  // Buzz for 5 seconds if unauthorized
        accessGranted = false;
      }
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);  // Yield to other tasks
  }
}

// Button Task: Handles button press logic
void Button_Task(void *pvParameters) {
  while (1) {
    if (accessGranted) {
      // Check if button press timeout is exceeded
      unsigned long currentMillis = millis();
      if (currentMillis - buttonPressStartTime > buttonPressTimeout) {
        Serial.println("Timeout! Please pass the RFID tag again.");
        resetSystem();
      }

      if (buttonPressed) {
        buttonPressed = false;
        Serial.println("Button pressed, sending coffee selection...");
        sendCoffeeSelectionToBackend(currentTagID, selectedCoffee);
        activateServo();
        resetSystem();
      }
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);  // Yield to other tasks
  }
}

void setup() {
  Serial.begin(115200);

  // Initialize PN532
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.println("Didn't find PN532 board");
    while (1) ;  // Halt
  }
  nfc.SAMConfig();  // Configure the RFID reader

  // Initialize Servo
  myServo.attach(SERVO_PIN);
  myServo.write(0);  // Initial position

  // Set buttons as input and attach interrupt
  pinMode(BUTTON_INTENSO_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_INTENSO_PIN), buttonISR, FALLING);

  // Set buzzer as output
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Connect to Wi-Fi
  connectToWiFi();

  // Set up timer for timeout (10 seconds)
  esp_timer_create_args_t timer_args;
  timer_args.callback = timeout_callback;
  timer_args.arg = NULL;
  timer_args.dispatch_method = ESP_TIMER_TASK;
  timer_args.name = "Timeout Timer";
  esp_timer_create(&timer_args, &timeout_timer);
  esp_timer_start_once(timeout_timer, 10000 * 1000);  // 10 seconds in microseconds

  // Create FreeRTOS tasks
  xTaskCreatePinnedToCore(RFID_Task, "RFID Task", 4096, NULL, 1, &RFIDTaskHandle, 0);
  xTaskCreatePinnedToCore(Button_Task, "Button Task", 4096, NULL, 1, &ButtonTaskHandle, 1);

  Serial.println("Ready to scan RFID tag...");
}

void loop() {
  // Main loop can be empty since tasks are running in parallel
}

// Function to get RFID tag ID from the PN532
String getRFIDTag(uint8_t* uid, uint8_t uidLength) {
  String tagID = "";
  for (uint8_t i = 0; i < uidLength; i++) {
    tagID += String(uid[i], HEX);
  }
  return tagID;
}

// Function to verify the tag with the backend
bool verifyTagWithBackend(String tagID) {
  HTTPClient http;
  String fullUrl = String(backend_verify_url) + "?rfid_tag=" + tagID;

  http.begin(fullUrl);  // Add tag as query parameter
  int httpCode = http.GET();

  if (httpCode == 200) {  // HTTP OK
    String response = http.getString();
    if (response.indexOf("success\":true") >= 0) {
      http.end();  // Close the connection properly
      return true;
    }
  } else {
    http.end();  // Close the connection properly
  }
  return false;
}

// Function to send coffee selection to the backend
void sendCoffeeSelectionToBackend(String tagID, String coffeeType) {
  HTTPClient http;
  http.begin(backend_coffee_url);  // Coffee selection endpoint

  // Set content type to JSON
  http.addHeader("Content-Type", "application/json");

  // Create JSON payload
  String payload = "{\"rfid_tag\":\"" + tagID + "\",\"coffee_type\":\"" + coffeeType + "\", \"machine_location\":\"" + String(machineLocation) + "\"}";

  int httpCode = http.POST(payload);

  if (httpCode == 200) {
    Serial.println("Coffee selection sent successfully.");
  } else {
    //Serial.println(http.getString());
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
  selectedCoffee = "";
  currentTagID = "";
  buttonPressStartTime = 0;  // Reset button press timer
  Serial.println("System reset. Please scan RFID again.");

  // Restart the timer for the next session
  esp_timer_start_once(timeout_timer, 10000 * 1000);  // 10 seconds in microseconds
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