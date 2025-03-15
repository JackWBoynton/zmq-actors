
#include <zmq.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>
#include <vector>
#include <cstring>
#include <algorithm>

// Helper function to split a string by a delimiter.
std::vector<std::string> split(const std::string &s, char delimiter) {
    std::vector<std::string> tokens;
    std::istringstream tokenStream(s);
    std::string token;
    while (std::getline(tokenStream, token, delimiter)) {
        token.erase(std::remove_if(token.begin(), token.end(), ::isspace), token.end());
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    return tokens;
}

int main(int argc, char* argv[]) {
    std::string client_id = "clientA";
    bool star_mode = false;
    std::string sub_topics = "";  // If empty, subscribe to all.
    std::string pub_topic = "cmd";  // default command topic if in star mode

    if (argc > 1) {
        client_id = argv[1];
    }
    if (argc > 2 && std::strcmp(argv[2], "star") == 0) {
        star_mode = true;
    }
    if (argc > 3) {
        sub_topics = argv[3];
    }
    if (argc > 4 && star_mode) {
        pub_topic = argv[4];
    }

    zmq::context_t context(1);

    // Query discovery service for available signals.
    {
        zmq::socket_t query_socket(context, zmq::socket_type::req);
        query_socket.connect("tcp://localhost:6004");
        std::string queryMsg = "QUERY SIGNALS";
        query_socket.send(zmq::buffer(queryMsg), zmq::send_flags::none);
        zmq::message_t queryReply;
        query_socket.recv(queryReply, zmq::recv_flags::none);
        std::string availableSignals(static_cast<char*>(queryReply.data()), queryReply.size());
        std::cout << "[" << client_id << "] Available SIGNALS: " << availableSignals << std::endl;
    }

    // Set up subscriber for receiving signals.
    zmq::socket_t sub_socket(context, zmq::socket_type::sub);
    sub_socket.connect("tcp://localhost:6001");
    if (!sub_topics.empty()) {
        auto topics = split(sub_topics, ',');
        for (const auto &topic : topics) {
            sub_socket.setsockopt(ZMQ_SUBSCRIBE, topic.c_str(), topic.size());
        }
        std::cout << "[" << client_id << "] Subscribing to topics: " << sub_topics << " at tcp://localhost:6001" << std::endl;
    } else {
        // Subscribe to everything.
        sub_socket.setsockopt(ZMQ_SUBSCRIBE, "", 0);
        std::cout << "[" << client_id << "] Subscribing to ALL signals at tcp://localhost:6001" << std::endl;
    }

    // In star mode, set up a publisher for sending commands.
    zmq::socket_t* pub_socket = nullptr;
    if (star_mode) {
        pub_socket = new zmq::socket_t(context, zmq::socket_type::pub);
        pub_socket->connect("tcp://localhost:6002");
        std::cout << "[" << client_id << "] STAR mode enabled: Publishing commands with topic [" << pub_topic
                  << "] to tcp://localhost:6002" << std::endl;
    }

    int cmd_counter = 0;
    while (true) {
        // Poll for incoming signal messages.
        zmq::pollitem_t items[] = {
            { static_cast<void*>(sub_socket), 0, ZMQ_POLLIN, 0 }
        };
        zmq::poll(items, 1, 100);
        if (items[0].revents & ZMQ_POLLIN) {
            zmq::message_t msg;
            sub_socket.recv(msg, zmq::recv_flags::none);
            std::string data(static_cast<char*>(msg.data()), msg.size());
            std::cout << "[" << client_id << "] Received: " << data << std::endl;
        }

        // In star mode, periodically send commands.
        if (star_mode && pub_socket) {
            static auto last_command_time = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_command_time);
            if (elapsed.count() >= 20) {
                last_command_time = now;
                std::ostringstream oss;
                // The command message begins with the command topic.
                oss << pub_topic << " " << client_id << " command #" << cmd_counter++;
                std::string cmd = oss.str();
                zmq::message_t cmd_msg(cmd.begin(), cmd.end());
                pub_socket->send(cmd_msg, zmq::send_flags::none);
                std::cout << "[" << client_id << "] Sent command: " << cmd << std::endl;
            }
        }
    }

    if (pub_socket) {
        delete pub_socket;
    }
    return 0;
}

