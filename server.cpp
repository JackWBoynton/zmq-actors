

#include <cstring>
#include <iostream>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <zmq.hpp>

// Global registries and mutex for thread-safety.
std::set<std::string> signalTopics;
std::set<std::string> commandTopics;
std::mutex registryMutex;

void discoveryService() {
  zmq::context_t context(1);
  zmq::socket_t rep(context, zmq::socket_type::rep);
  rep.bind("tcp://*:6004"); // Discovery service endpoint

  while (true) {
    zmq::message_t request;
    rep.recv(request, zmq::recv_flags::none);
    std::string reqStr(static_cast<char *>(request.data()), request.size());
    std::istringstream iss(reqStr);
    std::string command;
    iss >> command;
    std::string response;

    if (command == "REGISTER") {
      std::string type, topic;
      iss >> type >> topic;
      std::lock_guard<std::mutex> lock(registryMutex);
      if (type == "SIGNAL") {
        signalTopics.insert(topic);
        response = "Registered SIGNAL: " + topic;
      } else if (type == "COMMAND") {
        commandTopics.insert(topic);
        response = "Registered COMMAND: " + topic;
      } else {
        response = "Unknown registration type";
      }
    } else if (command == "QUERY") {
      std::string type;
      iss >> type;
      std::lock_guard<std::mutex> lock(registryMutex);
      std::ostringstream oss;
      if (type == "SIGNALS") {
        for (const auto &s : signalTopics) {
          oss << s << " ";
        }
        response = oss.str();
      } else if (type == "COMMANDS") {
        for (const auto &s : commandTopics) {
          oss << s << " ";
        }
        response = oss.str();
      } else {
        response = "Unknown query type";
      }
    } else {
      response = "Unknown command";
    }

    zmq::message_t reply(response.size());
    memcpy(reply.data(), response.data(), response.size());
    rep.send(reply, zmq::send_flags::none);
  }
}

int main() {
  // Start the discovery service in its own thread.
  std::thread discoveryThread(discoveryService);

  // Create the main ZeroMQ context.
  zmq::context_t context(1);

  // Set up bridging sockets.
  // Data flow: Actors publish data -> broker forwards to clients.
  zmq::socket_t sub_actors(context, zmq::socket_type::sub);
  zmq::socket_t pub_clients(context, zmq::socket_type::pub);
  sub_actors.bind("tcp://*:6000");  // Actors publish data here.
  pub_clients.bind("tcp://*:6001"); // Clients subscribe to data here.
  sub_actors.setsockopt(ZMQ_SUBSCRIBE, "", 0);

  // Command flow: Star clients publish commands -> broker forwards to star
  // actors.
  zmq::socket_t sub_clients(context, zmq::socket_type::sub);
  zmq::socket_t pub_actors(context, zmq::socket_type::pub);
  sub_clients.bind("tcp://*:6002"); // Star clients publish commands here.
  pub_actors.bind("tcp://*:6003");  // Star actors subscribe for commands here.
  sub_clients.setsockopt(ZMQ_SUBSCRIBE, "", 0);

  std::cout << "[Broker] Bridging service started." << std::endl;
  std::cout << "[Broker] Discovery service started on tcp://*:6004."
            << std::endl;

  while (true) {
    std::vector<zmq::pollitem_t> poll_items = {
        {static_cast<void *>(sub_actors), 0, ZMQ_POLLIN, 0},
        {static_cast<void *>(sub_clients), 0, ZMQ_POLLIN, 0}};

    zmq::poll(poll_items, -1);

    if (poll_items[0].revents & ZMQ_POLLIN) {
      zmq::message_t msg;
      sub_actors.recv(msg, zmq::recv_flags::none);
      pub_clients.send(msg, zmq::send_flags::none);
      std::string data(static_cast<char *>(msg.data()), msg.size());
      std::cout << "[Broker] Forwarding Actor->Client data: " << data
                << std::endl;
    }

    if (poll_items[1].revents & ZMQ_POLLIN) {
      zmq::message_t msg;
      sub_clients.recv(msg, zmq::recv_flags::none);
      pub_actors.send(msg, zmq::send_flags::none);
      std::string cmd(static_cast<char *>(msg.data()), msg.size());
      std::cout << "[Broker] Forwarding Client->Actor command: " << cmd
                << std::endl;
    }
  }

  discoveryThread.join();
  return 0;
}
