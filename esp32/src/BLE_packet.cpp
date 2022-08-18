#include <vector>
#include "WiFiStorage.h"
#include "WiFi.h"
#include <string>
#include "NimBLEDevice.h"
#include <Arduino.h>
#include <iostream>
#include <pb_encode.h>
#include <pb_decode.h>
#include <queue>
#include "packet.pb.h"
#include "DecodeException.h"
#include "WrongPacketException.h"

vector<uint8_t> decode(const vector<uint8_t> &in) {
    vector<uint8_t> returnVal;
    for (int i = 0; i < in.size(); i++) {
        if (in.at(i) == 1) {
            if (in.at(i + 1) == 2) {
                i++;
                returnVal.push_back(1);
            } else if (in.at(i + 1) == 3) {
                i++;
                returnVal.push_back(127);
            } else {
                throw DecodeException();
            }
        } else {
            returnVal.push_back(in.at(i));
        }
    }
    return returnVal;
}

vector<uint8_t> readData(NimBLECharacteristic *pRead) {
    NimBLEAttValue readVal = pRead->getValue();
    while (readVal.size() == 4 && readVal[0] == readVal[1] == readVal[2] == readVal[3] == 0);
    vector<uint8_t> buff;
    for (;;) {
        readVal = pRead->getValue();
        for (unsigned char i: readVal) {
            if (i == 127) {
                pRead->setValue(0);
                return decode(buff);
            }
            buff.push_back(i);
        }
    }
}

int send(const vector<WiFiStorage> &data, NimBLECharacteristic *pRead, NimBLECharacteristic *pWrite) {
    Packet p = Packet_init_zero;

    pb_byte_t buf[2048];
    pb_ostream_t output = pb_ostream_from_buffer(buf, sizeof(buf));
    p.which_type = Packet_wifi_tag;
    vector<WiFiData> payload;
    for (const auto d: data) {
        WiFiData wData = WiFiData_init_zero;
        wData = WiFiData{
                .BSSID = (uint64_t) d.BSSID[0] | ((uint64_t) d.BSSID.at(1) << 8) | ((uint64_t) d.BSSID.at(2) << 16) |
                         ((uint64_t) d.BSSID.at(3) << 24) | ((uint64_t) d.BSSID.at(4) << 32) |
                         ((uint64_t) d.BSSID.at(5) << 40),
                .Channel = static_cast<uint32_t>(d.Channel),
                .isEncrypted = d.isEncrypted
        };
        strncpy(wData.SSID, d.SSID.c_str(), std::min(d.SSID.size(), sizeof(wData.SSID) - 1));
        payload.push_back(wData);
    }

    p.type.wifi = WiFiVector{
            .data_count = (pb_size_t) (data.size()),
    };
    for (int i = 0; i < std::min(data.size(), sizeof(p.type.wifi.data) / sizeof(WiFiData)); i++) {
        p.type.wifi.data[i] = payload.at(i);
    }
    int status = pb_encode(&output, Packet_fields, &p);
    if (!status) {
        cout << "Encoding failed: " << PB_GET_ERROR(&output) << endl;
        return 1;
    }
    Serial.print("\n");
    for (int i = 0; i < output.bytes_written; i++) {
        Serial.print(buf[i]);
        Serial.print(" ");
    }
    Serial.print("\n");
    for (int i = 0; i < output.bytes_written; i++) {
        queue<uint8_t> a;
        if (buf[i] == 1) {
            a.push(1);
            a.push(2);
        } else if (buf[i] == 127) {
            a.push(1);
            a.push(3);
        } else {
            a.push(buf[i]);
        }
        if (a.size() > 128) {
            vector<uint8_t> send_buf;
            for (int j = 0; j < 128; j++) {
                send_buf.push_back(a.front());
                a.pop();
            }
            pWrite->setValue(send_buf);
            vector<uint8_t> inBytes = readData(pRead);
            Packet inPacket = Packet_init_zero;

            pb_istream_t stream = pb_istream_from_buffer(inBytes.data(), inBytes.size());

            status = pb_decode(&stream, Packet_fields, &inPacket);

            if (!status) {
                printf("Decoding failed: %s\n", PB_GET_ERROR(&stream));
                return 1;
            }

            if (inPacket.which_type != Packet_ack_tag) {
                throw WrongPacketException();
            }
        }
    }
    return 0;
}