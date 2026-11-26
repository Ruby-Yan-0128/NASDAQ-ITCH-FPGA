#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <vector>

// Portable big-endian readers (ITCH 5.0 is unsigned big-endian)
inline uint16_t read_be16(const uint8_t* p) {
    return (uint16_t(p[0]) << 8) | uint16_t(p[1]);
}

inline uint32_t read_be32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) |
           (uint32_t(p[2]) << 8)  |  uint32_t(p[3]);
}

inline uint64_t read_be48(const uint8_t* p) {
    return (uint64_t(p[0]) << 40) | (uint64_t(p[1]) << 32) |
           (uint64_t(p[2]) << 24) | (uint64_t(p[3]) << 16) |
           (uint64_t(p[4]) << 8)  |  uint64_t(p[5]);
}

inline uint64_t read_be64(const uint8_t* p) {
    return (uint64_t(read_be32(p)) << 32) | uint64_t(read_be32(p + 4));
}

int main() {
    std::ifstream in("itch_sample.bin", std::ios::binary);
    if (!in) {
        std::cerr << "Error: could not open itch_sample.bin\n";
        return 1;
    }

    // Hex file for SystemVerilog $readmemh (raw ITCH payloads, no 2-byte length prefix)
    std::ofstream hex_out("itch_vectors.hex");
    std::map<char, uint64_t> type_counts;

    uint64_t total_msgs = 0;
    uint64_t add_orders_printed = 0;
    const uint64_t HEX_EXPORT_LIMIT = 200; // First 200 messages for fast XSim simulation

    uint8_t len_buf[2];
    while (in.read(reinterpret_cast<char*>(len_buf), 2)) {
        uint16_t msg_len = read_be16(len_buf);
        if (msg_len == 0) break;

        std::vector<uint8_t> payload(msg_len);
        if (!in.read(reinterpret_cast<char*>(payload.data()), msg_len)) {
            break;
        }

        char msg_type = static_cast<char>(payload[0]);
        type_counts[msg_type]++;
        total_msgs++;

        // Export raw payload bytes (1 byte per line in hex) for tb_itch_parser.sv
        // Start exporting as soon as the first 'A' (Add Order) message arrives
        static bool started_export = false;
        static uint64_t exported_msgs = 0;

        if (!started_export && msg_type == 'A') {
            started_export = true;
        }
        if (started_export && exported_msgs < 200) {
            for (uint8_t b : payload) {
            hex_out << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<int>(b) << "\n";
        }
            exported_msgs++;
        }

        // Decode 'A' (Add Order - No MPID, 36 bytes)
        if (msg_type == 'A' && msg_len == 36 && add_orders_printed < 10) {
            uint16_t stock_locate = read_be16(&payload[1]);
            uint64_t timestamp    = read_be48(&payload[5]);
            uint64_t order_ref    = read_be64(&payload[11]);
            char     side         = static_cast<char>(payload[19]);
            uint32_t shares       = read_be32(&payload[20]);
            std::string ticker(reinterpret_cast<const char*>(&payload[24]), 8);
            uint32_t price_raw    = read_be32(&payload[32]);

            std::cout << "[Add Order #" << (add_orders_printed + 1) << "] "
                      << "Locate=" << stock_locate
                      << " Ticker=\"" << ticker << "\""
                      << " Side=" << side
                      << " Shares=" << shares
                      << " Price=$" << std::fixed << std::setprecision(4)
                      << (price_raw / 10000.0)
                      << " TS=" << timestamp
                      << " Ref=" << order_ref << "\n";
            add_orders_printed++;
        }
    }

    std::cout << "\n--- Message Type Summary (" << total_msgs << " total) ---\n";
    for (const auto& [type, count] : type_counts) {
        std::cout << "  '" << type << "' : " << count << "\n";
    }
    std::cout << "\nExported first " << std::min(total_msgs, HEX_EXPORT_LIMIT)
              << " messages to itch_vectors.hex\n";
    return 0;
}