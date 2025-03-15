
#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>
#include <zmq.hpp>

// Helper function to split a string by a delimiter.
std::vector<std::string> split(const std::string &s, char delimiter) {
  std::vector<std::string> tokens;
  std::istringstream tokenStream(s);
  std::string token;
  while (std::getline(tokenStream, token, delimiter)) {
    // Trim any extra spaces.
    token.erase(std::remove_if(token.begin(), token.end(), ::isspace),
                token.end());
    if (!token.empty()) {
      tokens.push_back(token);
    }
  }
  return tokens;
}

int main(int argc, char *argv[]) {
  // Default values.
  std::string actor_id = "actorA";
  bool star_mode = false;
  std::string defaultSignals =
      "engine/temperature,engine/pressure,oil/level,brake/status";
  std::string signal_list = defaultSignals;
  std::string cmd_sub_list = "cmd"; // default command subscription

  if (argc > 1) {
    actor_id = argv[1];
  }
  if (argc > 2 && std::strcmp(argv[2], "star") == 0) {
    star_mode = true;
  }
  if (argc > 3) {
    signal_list = argv[3];
  }
  if (argc > 4 && star_mode) {
    cmd_sub_list = argv[4];
  }

  // Parse the comma-separated lists.
  auto signals = split(signal_list, ',');
  std::vector<std::string> cmdSubscriptions;
  if (star_mode) {
    cmdSubscriptions = split(cmd_sub_list, ',');
  }

  zmq::context_t context(1);

  // Register each signal with the discovery service.
  {
    zmq::socket_t reg_socket(context, zmq::socket_type::req);
    reg_socket.connect("tcp://localhost:6004");
    for (const auto &sig : signals) {
      std::string regMsg = "REGISTER SIGNAL " + sig;
      reg_socket.send(zmq::buffer(regMsg), zmq::send_flags::none);
      zmq::message_t reply;
      reg_socket.recv(reply, zmq::recv_flags::none);
      std::cout << "[" << actor_id << "] Registration reply: "
                << std::string(static_cast<char *>(reply.data()), reply.size())
                << std::endl;
    }
    // If in star mode, also register each command topic.
    if (star_mode) {
      for (const auto &cmd : cmdSubscriptions) {
        std::string regMsg = "REGISTER COMMAND " + cmd;
        reg_socket.send(zmq::buffer(regMsg), zmq::send_flags::none);
        zmq::message_t reply;
        reg_socket.recv(reply, zmq::recv_flags::none);
        std::cout << "[" << actor_id << "] Registration reply: "
                  << std::string(static_cast<char *>(reply.data()),
                                 reply.size())
                  << std::endl;
      }
    }
  }

  // Set up publisher for sending signals.
  zmq::socket_t pub_socket(context, zmq::socket_type::pub);
  pub_socket.connect("tcp://localhost:6000");
  std::cout << "[" << actor_id << "] Publishing signals to tcp://localhost:6000"
            << std::endl;

  // In star mode, set up a subscriber for receiving commands.
  zmq::socket_t *sub_socket = nullptr;
  if (star_mode) {
    sub_socket = new zmq::socket_t(context, zmq::socket_type::sub);
    sub_socket->connect("tcp://localhost:6003");
    // Subscribe to each command topic.
    for (const auto &cmd : cmdSubscriptions) {
      sub_socket->setsockopt(ZMQ_SUBSCRIBE, cmd.c_str(), cmd.size());
    }
    std::cout << "[" << actor_id
              << "] STAR mode enabled: Subscribing to command topics: "
              << cmd_sub_list << " from tcp://localhost:6003" << std::endl;
  }

  int counter = 0;
  while (true) {
    // In star mode, poll for incoming commands.
    if (star_mode) {
      zmq::pollitem_t items[] = {
          {static_cast<void *>(*sub_socket), 0, ZMQ_POLLIN, 0}};
      zmq::poll(items, 1, 100);
      if (items[0].revents & ZMQ_POLLIN) {
        zmq::message_t cmd_msg;
        sub_socket->recv(cmd_msg, zmq::recv_flags::none);
        std::string cmd_str(static_cast<char *>(cmd_msg.data()),
                            cmd_msg.size());
        std::cout << "[" << actor_id << "] Received command: " << cmd_str
                  << std::endl;
      }
    }

    // For demo purposes, send one message per signal.
    for (const auto &sig : signals) {
      std::ostringstream oss;
      // The message begins with the topic (e.g. "engine/temperature") followed
      // by payload.
      oss << sig << " " << actor_id << " value #" << counter;
      std::string data = oss.str();
      zmq::message_t message(data.begin(), data.end());
      pub_socket.send(message, zmq::send_flags::none);
      std::cout << "[" << actor_id << "] Sent: " << data << std::endl;
      std::this_thread::sleep_for(
          std::chrono::milliseconds(1)); // slight delay between signals
    }
    ++counter;
  }

  if (sub_socket) {
    delete sub_socket;
  }
  return 0;
}
