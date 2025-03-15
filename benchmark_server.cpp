
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <zmq.hpp>

int main(int argc, char *argv[]) {
  std::string frontend_endpoint = (argc > 1) ? argv[1] : "tcp://*:7001";
  std::string backend_endpoint = (argc > 2) ? argv[2] : "tcp://*:7002";
  long long max_throughput = (argc > 3) ? std::stoll(argv[3]) : 100000;

  zmq::context_t context(4);
  zmq::socket_t frontend(context, zmq::socket_type::sub);
  zmq::socket_t backend(context, zmq::socket_type::pub);

  frontend.bind(frontend_endpoint);
  frontend.setsockopt(ZMQ_SUBSCRIBE, "", 0);
  backend.bind(backend_endpoint);

  long long msg_count = 0;
  auto start_time = std::chrono::steady_clock::now();

  while (true) {
    zmq::message_t message;
    if (frontend.recv(message, zmq::recv_flags::none)) {
      msg_count++;
      backend.send(message, zmq::send_flags::none);
    }
    auto current_time = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(
                         current_time - start_time)
                         .count();
    if (elapsed >= 1.0) {
      double throughput = msg_count / elapsed;
      long long remaining =
          (throughput < max_throughput)
              ? max_throughput - static_cast<long long>(throughput)
              : 0;
      std::cout << "Server throughput: " << throughput << " msgs/sec"
                << " | Remaining capacity: " << remaining << " msgs/sec"
                << std::endl;
      msg_count = 0;
      start_time = current_time;
    }
  }

  return 0;
}
