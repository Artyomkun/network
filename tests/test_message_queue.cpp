// Copyright 2026 Artyomkun
// SPDX-License-Identifier: Apache-2.0

#include <chrono>
#include <thread>

#include "logger/record.hpp"

#include "../app/message_queue.hpp"
#include "test_framework.hpp"

TEST("MessageQueue preserves push order") {
    app::MessageQueue queue;
    for (int i = 0; i < 100; ++i) {
        logger::Record record;
        record.message = "message " + std::to_string(i);
        record.level = logger::Level::Info;
        queue.push(record);
    }
    for (int i = 0; i < 100; ++i) {
        const auto record = queue.pop();
        CHECK(record.has_value());
        CHECK(record->message == "message " + std::to_string(i));
    }
}

TEST("MessageQueue pop returns empty after stop") {
    app::MessageQueue queue;
    queue.push(logger::Record{});
    queue.stop();
    CHECK(queue.pop().has_value());
    CHECK(!queue.pop().has_value());
    CHECK(!queue.pop().has_value());
}

TEST("MessageQueue delivers messages from a producer thread") {
    app::MessageQueue queue;
    const int count = 1000;
    std::thread producer([&queue, count] {
        for (int i = 0; i < count; ++i) {
            logger::Record record;
            record.message = std::to_string(i);
            queue.push(record);
        }
        queue.stop();
    });

    int received = 0;
    for (;;) {
        const auto record = queue.pop();
        if (!record) {
            break;
        }
        ++received;
    }
    producer.join();
    CHECK_EQ(received, count);
}

TEST("MessageQueue push blocks when the queue is full") {
    app::MessageQueue queue(2);
    queue.push(logger::Record{});
    queue.push(logger::Record{});

    // The third push into the full queue must block until room appears.
    bool pushed = false;
    std::thread producer([&queue, &pushed] {
        queue.push(logger::Record{});
        pushed = true;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    CHECK(!pushed);  // still waiting for room

    queue.pop();  // free one slot
    producer.join();
    CHECK(pushed);
}

TEST("MessageQueue full push returns false after stop") {
    app::MessageQueue queue(1);
    queue.push(logger::Record{});  // the queue is full
    queue.stop();                  // wakes the waiting push
    CHECK(!queue.push(logger::Record{}));
}

TEST("MessageQueue bounded on full capacity") {
    app::MessageQueue queue(3);
    for (int i = 0; i < 100; i += 3) {
        for (int j = 0; j < 3; ++j) {
            queue.push(logger::Record{});
        }
        for (int j = 0; j < 3; ++j) {
            CHECK(queue.pop().has_value());
        }
    }
}
