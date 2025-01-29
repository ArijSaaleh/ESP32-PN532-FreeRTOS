# IoT Coffee Dispenser with RFID and Button Press System

## Project Overview

This project implements an IoT-enabled coffee dispenser system using an **ESP32** microcontroller, an **RFID module (PN532)**, and a **push-button** for user interaction. The system allows authorized users to scan their RFID tag to gain access to the coffee machine, select their preferred coffee type via a button press, and dispense the coffee. The system will reset if no button is pressed within a set time limit, prompting users to rescan their RFID tag.

## Features

1. **RFID Authentication**: Users authenticate themselves by scanning their RFID tag. Only authorized users (verified by the backend) can proceed.
2. **Button-Based Coffee Selection**: After successful RFID authentication, users can press a button to select a coffee type. 
3. **Timeout Handling**: If no button is pressed within 10 seconds after authentication, the system will reset, and the user must rescan their RFID tag.
4. **Wi-Fi Enabled**: The ESP32 connects to the backend via Wi-Fi for verifying the RFID tag and sending coffee selection details.
5. **Servo Mechanism**: A servo motor is used to simulate dispensing coffee by rotating when a coffee type is selected.
6. **Buzzer Alerts**: A buzzer is used to notify the user in case of access denial or errors.
7. **Real-Time Operations with FreeRTOS**: The system utilizes FreeRTOS tasks to handle RFID scanning and button press events concurrently, enabling smooth and efficient multitasking.

## Components

- **ESP32** microcontroller
- **PN532** RFID module
- **Push-button** for coffee selection
- **Servo motor** to simulate coffee dispensing
- **Buzzer** for feedback
- **Wi-Fi** for communication with the backend

## Hardware Setup

1. **PN532 RFID Module**: Connect as follows:
   - SCK (PN532) → GPIO 18 (ESP32)
   - MISO (PN532) → GPIO 19 (ESP32)
   - MOSI (PN532) → GPIO 23 (ESP32)
   - SS (PN532) → GPIO 27 (ESP32)
   
2. **Push-button**: Connect the button to GPIO 14 (ESP32) with an external pull-up resistor or use internal pull-up.

3. **Servo Motor**: Connect the signal pin of the servo to GPIO 13 (ESP32).

4. **Buzzer**: Connect the buzzer to GPIO 15 (ESP32).

## Software Dependencies

- **Arduino IDE** (for programming the ESP32)
- **Adafruit PN532 Library**: For interfacing with the PN532 RFID module.
- **ESP32Servo Library**: For controlling the servo motor.
- **WiFi Library**: For enabling Wi-Fi communication.
- **HTTPClient Library**: For making HTTP requests to the backend.
- **FreeRTOS**: Used for concurrent task management (already included in the ESP32 Arduino core).

Install the required libraries in the Arduino IDE through the Library Manager.

## FreeRTOS Task Management

The system leverages **FreeRTOS** to manage two parallel tasks:

1. **RFID Task**: Continuously reads the RFID tag and sends it to the backend for verification.
2. **Button Task**: Monitors button presses to capture the user's coffee selection.

Each of these tasks runs independently using FreeRTOS, ensuring that RFID scanning and button presses are handled in real-time without blocking each other.

The **RFID Task** and **Button Task** are prioritized and scheduled effectively:
- The RFID task starts by reading the user's RFID tag.
- After authentication, the button task waits for input (button press) for coffee selection.
- If no button is pressed within 10 seconds (handled by a FreeRTOS timer), the system resets, asking the user to rescan their RFID.

FreeRTOS is particularly beneficial in ensuring these tasks run simultaneously, improving the overall responsiveness of the system.

### Task Initialization

- `xTaskCreate()`: Initializes both the RFID and button tasks.
- **Software Timer**: Used for handling the 10-second timeout when waiting for a button press.

## Backend Configuration

1. **Verify RFID Tag Endpoint**: The backend must provide an endpoint for verifying the RFID tag:
   - URL: `/api/verify-rfid?rfid_tag=<TAG_ID>`
   - Method: GET
   - Response: `{ "success": true/false }`

2. **Send Coffee Selection Endpoint**: The backend must provide an endpoint for submitting the selected coffee:
   - URL: `/api/select-coffee`
   - Method: POST
   - Payload: `{ "rfid_tag": "<TAG_ID>", "coffee_type": "<Coffee_Type>", "machine_location": "<Location>" }`

## Code Overview

The code consists of two main tasks:
1. **RFID Task**: Reads the RFID tag and sends it to the backend for verification.
2. **Button Task**: Listens for button presses and sends the coffee selection to the backend.

Both tasks are handled in parallel using FreeRTOS tasks. The timeout logic is handled using a software timer that resets the system if no button is pressed within 10 seconds of scanning the RFID tag.

### Key Functions:

- `verifyTagWithBackend()`: Sends the RFID tag to the backend and waits for a verification response.
- `sendCoffeeSelectionToBackend()`: Sends the selected coffee type and RFID tag to the backend after the button press.
- `activateServo()`: Simulates coffee dispensing by rotating the servo motor.
- `resetSystem()`: Resets the system state, requiring the user to rescan their RFID tag.
- `timeout_callback()`: Handles the timeout scenario if no button is pressed within 10 seconds.

## Usage

1. Power up the ESP32 and connect it to Wi-Fi.
2. Scan the RFID tag with the PN532 module.
3. If the tag is authorized, the system will wait for the button press to select coffee.
4. If no button is pressed within 10 seconds, the system will reset and request the user to rescan their RFID tag.
5. Upon selecting a coffee type, the system sends the details to the backend and activates the servo to simulate dispensing.

## License

This project is open-source and free to use under the MIT License.

## Credits

- **RFID Module**: Adafruit PN532
- **ESP32 Servo Library**: ESP32Servo
- **ESP32 FreeRTOS**: Built-in support in the ESP32 Arduino core
