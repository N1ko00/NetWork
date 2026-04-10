#pragma once

#include <iostream>
#include <chrono>
#include <mutex>
#include <thread>
#include <cstdint>

class Snowflake {
private:
    static constexpr uint64_t epoch = 1609459200000ULL; // 2021-01-01 00:00:00 UTC
    static constexpr uint64_t machine_bits = 10;        // マシン ID のビット数
    static constexpr uint64_t sequence_bits = 12;       // シーケンス番号のビット数
    static constexpr uint64_t max_sequence = (1ULL << sequence_bits) - 1;

    uint64_t machine_id;
    uint64_t last_timestamp;
    uint64_t sequence;
    std::mutex mutex;

    // 現在のミリ秒単位のタイムスタンプを取得
    static uint64_t get_current_timestamp() {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count()
        );
    }
public:
    static constexpr uint64_t kMaxMachineId = (1ULL << machine_bits) - 1;

    Snowflake() = delete;

    // コンストラクタ: マシン ID を指定
    explicit Snowflake(uint64_t machine_id) : machine_id(machine_id), last_timestamp(0), sequence(0) {
        if (machine_id > kMaxMachineId) {
            throw std::runtime_error("Machine ID is out of range");
        }
    }

    // 一意な ID を生成
    uint64_t next_id() {
        std::lock_guard<std::mutex> lock(mutex);
        uint64_t timestamp = get_current_timestamp();

        if (timestamp < last_timestamp) {
            throw std::runtime_error("Clock moved backwards. Refusing to generate ID");
        }

        if (timestamp == last_timestamp) {
            sequence = (sequence + 1) & max_sequence;
            if (sequence == 0) {
                // シーケンスがオーバーフローしたら次のミリ秒を待つ
                while (timestamp <= last_timestamp) {
                    timestamp = get_current_timestamp();
                }
            }
        }
        else {
            sequence = 0;
        }

        last_timestamp = timestamp;

        // Snowflake ID を構築して返す (符号なし 64 ビット整数)
        return ((timestamp - epoch) << (machine_bits + sequence_bits)) |
            (machine_id << sequence_bits) |
            sequence;
    }
};
