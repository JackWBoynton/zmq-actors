#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>
#include <zmq.hpp>

int main(int argc, char *argv[]) {
  int rate = (argc > 1) ? std::atoi(argv[1]) : 1000;
  long count = (argc > 2) ? std::atol(argv[2]) : 0; // 0 means infinite
  std::string endpoint = (argc > 3) ? argv[3] : "tcp://localhost:7001";

  zmq::context_t context(1);
  zmq::socket_t pub(context, zmq::socket_type::pub);
  pub.connect(endpoint);

  auto sleep_duration = std::chrono::microseconds(1000000 / rate);

  long sent = 0;
  while (count == 0 || sent < count) {
    auto now = std::chrono::system_clock::now();
    double timestamp =
        std::chrono::duration_cast<std::chrono::duration<double>>(
            now.time_since_epoch())
            .count();

    // Create a 70-byte message
    // First 8 bytes: binary double (timestamp)
    // Next 62 bytes: filler data (e.g., 'A')
    char msg[70];
    std::memset(msg, 0, 70);
    std::memcpy(msg, &timestamp, sizeof(double));
    for (int i = 8; i < 70; i++) {
      msg[i] = 'A';
    }

    zmq::message_t zmq_msg(msg, 70);
    pub.send(zmq_msg, zmq::send_flags::none);
    sent++;

    std::this_thread::sleep_for(sleep_duration);
  }
  std::cout << "Sent " << sent << " messages" << std::endl;
  return 0;
}
