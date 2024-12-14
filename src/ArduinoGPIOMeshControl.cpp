/*
   based on: Meshtastic send/receive client
*/

#include <Meshtastic.h>

#define SERIAL_RX_PIN D7
#define SERIAL_TX_PIN D6
#define BAUD_RATE 9600
#define SEND_PERIOD 30

const int leds[] = {D0, D1, D2};       // Array to hold LED pins
bool led_states[] = {LOW, LOW, LOW};   // Array to track each LED's state
const int num_leds = sizeof(leds) / sizeof(leds[0]); // Number of LEDs
bool can_send = false;

uint32_t next_send_time = 0;
bool not_yet_connected = true;

void toggleLED(int led_index) {
  if (led_index < 0 || led_index >= num_leds) return; // Ensure valid index

  // Toggle the state
  led_states[led_index] = !led_states[led_index];
  
  // Set the LED to the new state
  digitalWrite(leds[led_index], led_states[led_index]);

  // Print the state
  Serial.print("Toggled LED ");
  Serial.print(led_index);
  Serial.print(" to state: ");
  Serial.println(led_states[led_index] ? "HIGH" : "LOW");
}

String collect_led_state() {
  String ledState = "[";
  for (int i = 0; i < num_leds; i++) {
    ledState += (led_states[i] ? "1" : "0");
    if (i < num_leds - 1) {
      ledState += ",";
    }
  }
  ledState += "]";
  return ledState;
}

void send_led_state(uint32_t to_address) {
  String ledState = collect_led_state(); // Collect the LED state as a string

  // Create a char array large enough to hold the string, including the null terminator
  char ledStateChar[ledState.length() + 1]; 

  // Copy the String into the char array
  ledState.toCharArray(ledStateChar, sizeof(ledStateChar));

  if (can_send) {
    uint8_t channel_index = 0;
    mt_send_text(ledStateChar, to_address, channel_index); // Send the LED state
  }
}

// This callback function will be called whenever the radio connects to a node
void connected_callback(mt_node_t *node, mt_nr_progress_t progress) {
  if (not_yet_connected) 
    Serial.println("Connected to Meshtastic device!");
  not_yet_connected = false;
}

// This callback function will be called whenever the radio receives a text message
void text_message_callback(uint32_t from_id, uint32_t to_id, uint8_t channel, const char* msg) {

  Serial.print("Received a text message from ");
  Serial.print(from_id);

  Serial.print(" to: ");
  Serial.print(to_id);

  Serial.print(" message: ");
  Serial.println(msg);

  if (to_id == my_node_num){
    Serial.println("This is a DM to me!");

    // Check which LED to toggle based on the message
    if (strcmp(msg, "1") == 0) {
      toggleLED(0);
      send_led_state(from_id);
    } else if (strcmp(msg, "2") == 0) {
      toggleLED(1);
      send_led_state(from_id);
    } else if (strcmp(msg, "3") == 0) {
      toggleLED(2);
      send_led_state(from_id);
    } else if (strcmp(msg, "check") == 0){
      send_led_state(from_id);
    }
  }
  
if (to_id == 4294967295){
    Serial.println("This is a broadcast message.");
  }
}

void node_report_callback(mt_node_t *node, mt_nr_progress_t progress) {
  if (progress == MT_NR_DONE) {
    Serial.println("Node report completed!");
    return;
  }

  if (progress == MT_NR_INVALID) {
    Serial.println("Invalid node report response.");
    return;
  }

  if (node) {
    // Check if this node is the current device
    if (node->is_mine) {
      Serial.print("I am: ");
      Serial.println(my_node_num);
    }
  }
}


void setup() {

  for (int i = 0; i < num_leds; i++) {
    pinMode(leds[i], OUTPUT);
    digitalWrite(leds[i], led_states[i]);
  }

  randomSeed(micros());

  Serial.begin(9600);
  while(true) {
    if (Serial) break;
    if (millis() > 5000) {
      Serial.print("Couldn't find a serial port after 5 seconds, continuing anyway");
      break;
    }
  }

  Serial.print("Booted Meshtastic send/receive client in serial mode");
  mt_serial_init(SERIAL_RX_PIN, SERIAL_TX_PIN, BAUD_RATE);

  // Set to true if you want debug messages
  mt_set_debug(true);
  
  randomSeed(micros());

  // Initial connection to the Meshtastic device
  mt_request_node_report(connected_callback);

  // Register a callback function to be called whenever a text message is received
  set_text_message_callback(text_message_callback);  
}

void loop() {
  // Record the time that this loop began (in milliseconds since the device booted)
  uint32_t now = millis();

  // Run the Meshtastic loop, and see if it's able to send requests to the device yet
  can_send = mt_loop(now);

  // If we can send, and it's time to do so, send a text message and schedule the next one.
  if (can_send && now >= next_send_time) {
    
    // uint32_t dest = 1166139310;
    // uint8_t channel_index = 0; 
    // mt_send_text("Hello, world!", dest, channel_index);


    // Serial.print("My Node Number is: ");
    // Serial.println(my_node_num);

    next_send_time = now + SEND_PERIOD * 1000;
  }

}
