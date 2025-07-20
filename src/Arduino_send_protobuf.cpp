/*
   Powered by Meshtastic.org
*/

#include <Meshtastic.h>
#include "meshtastic/admin.pb.h"
#include "meshtastic/config.pb.h"

#define SERIAL_RX_PIN D7
#define SERIAL_TX_PIN D6
#define BAUD_RATE 9600
#define SEND_PERIOD 30

#define MT_DEBUGGING

extern bool mt_serial_send_radio(const char *buf, size_t len);

extern pb_byte_t pb_buf[];
constexpr size_t PB_BUFSIZE = 512;

bool can_send = false;

uint32_t next_send_time = 0;
bool not_yet_connected = true;

// Magic number at the start of all MT packets
#define MT_MAGIC_0 0x94
#define MT_MAGIC_1 0xc3


size_t pb_encode_to_bytes(uint8_t *destbuf, size_t destbufsize, const pb_msgdesc_t *fields, const void *src_struct)
{
    pb_ostream_t stream = pb_ostream_from_buffer(destbuf, destbufsize);
    if (!pb_encode(&stream, fields, src_struct)) {
        assert(
            0); // If this assert fails it probably means you made a field too large for the max limits specified in mesh.options
    } else {
        return stream.bytes_written;
    }
}


// Encode AdminMessage into meshtastic_Data_payload_t
bool encode_admin_message(const meshtastic_AdminMessage& admin, meshtastic_Data_payload_t& payload) {
    pb_ostream_t stream = pb_ostream_from_buffer(payload.bytes, sizeof(payload.bytes));
    if (!pb_encode(&stream, meshtastic_AdminMessage_fields, &admin)) {
        Serial.println("Failed to encode AdminMessage");
        return false;
    }
    payload.size = stream.bytes_written;
    return true;
}

bool mt_send_toRadio(meshtastic_ToRadio toRadio) {

    pb_buf[0] = MT_MAGIC_0;
    pb_buf[1] = MT_MAGIC_1;

    pb_ostream_t stream = pb_ostream_from_buffer(pb_buf + 4, PB_BUFSIZE);
    bool status = pb_encode(&stream, meshtastic_ToRadio_fields, &toRadio);
    if (!status) {
        Serial.println("Couldn't encode toRadio");
        return false;
    } else {
        Serial.println("Encoded toRadio successfully");
    }

    // Store the payload length in the header
    pb_buf[2] = stream.bytes_written / 256;
    pb_buf[3] = stream.bytes_written % 256;

    bool rv = mt_serial_send_radio((const char *)pb_buf, 4 + stream.bytes_written);
    Serial.print("mt_serial_send_radio returned: ");
    Serial.println(rv);
        return rv;
}

// Send encoded admin payload via ADMIN_APP
bool mt_send_admin_payload(const meshtastic_Data_payload_t& payload) {
    // Initialize base ToRadio message
    meshtastic_ToRadio toRadio = meshtastic_ToRadio_init_default;
    toRadio.which_payload_variant = meshtastic_ToRadio_packet_tag;

    // Fill in packet fields
    meshtastic_MeshPacket& packet = toRadio.packet;
    packet.from = my_node_num;
    packet.to = my_node_num;
    packet.id = random(0x7FFFFFFF);
    packet.which_payload_variant = meshtastic_MeshPacket_decoded_tag;

    // Fill in decoded submessage
    packet.decoded.portnum = meshtastic_PortNum_ADMIN_APP;
    packet.decoded.payload = payload;
    packet.decoded.want_response = true;

    packet.want_ack = true;
    packet.channel = 0;

    return mt_send_toRadio(toRadio);
}

bool mt_send_set_role() {
    meshtastic_AdminMessage admin = meshtastic_AdminMessage_init_default;
    admin.which_payload_variant = meshtastic_AdminMessage_set_config_tag;
    admin.set_config.payload_variant.device.role = meshtastic_Config_DeviceConfig_Role_CLIENT_MUTE;

    meshtastic_Data_payload_t payload;
    if (!encode_admin_message(admin, payload)) return false;

    return mt_send_admin_payload(payload);
}


// This callback function will be called whenever the radio connects to a node
void connected_callback(mt_node_t *node, mt_nr_progress_t progress) {
  if (not_yet_connected) 
    Serial.println("Connected to Meshtastic device!");
  not_yet_connected = false;
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
  delay(4000);

  Serial.begin(9600);
  while(true) {
    if (Serial) break;
    if (millis() > 5000) {
      Serial.print("Couldn't find a serial port after 5 seconds, continuing anyway");
      break;
    }
  }

  Serial.println("Booted Meshtastic in serial mode");

  mt_serial_init(SERIAL_RX_PIN, SERIAL_TX_PIN, BAUD_RATE);
  randomSeed(micros());
  mt_request_node_report(connected_callback);

  Serial.print("Waiting for my node number to be set... ");
  while (my_node_num == 0) {
    mt_loop(millis());
    Serial.print(".");
    delay(10);
  }

  Serial.println("sending role change...");
  mt_send_set_role();

}

void loop() {
  uint32_t now = millis();
  can_send = mt_loop(now);

  if (can_send && now >= next_send_time) {
    
    Serial.print("My Node Number is: ");
    Serial.println(my_node_num);
    next_send_time = now + SEND_PERIOD * 1000;
    Serial.println("Looping...");
  }

}
