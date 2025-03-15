#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>
#include <zmq.hpp>

int main(int argc, char *argv[]) {
  std::string endpoint = (argc > 1) ? argv[1] : "tcp://localhost:7002";

  zmq::context_t context(1);
  zmq::socket_t sub(context, zmq::socket_type::sub);
  sub.connect(endpoint);

  sub.setsockopt(ZMQ_SUBSCRIBE, "", 0);

  long msg_count = 0;
  double total_latency = 0.0;
  double min_latency = 1e9;
  double max_latency = 0.0;

  auto start_time = std::chrono::steady_clock::now();

  while (true) {
    zmq::message_t message;
    sub.recv(message, zmq::recv_flags::none);

    if (message.size() < sizeof(double)) {
      continue;
    }

    double sent_timestamp;
    std::memcpy(&sent_timestamp, message.data(), sizeof(double));

    auto now = std::chrono::system_clock::now();
    double now_timestamp =
        std::chrono::duration_cast<std::chrono::duration<double>>(
            now.time_since_epoch())
            .count();

    double latency = now_timestamp - sent_timestamp;
    if (latency < min_latency)
      min_latency = latency;
    if (latency > max_latency)
      max_latency = latency;
    total_latency += latency;
    msg_count++;

    if (msg_count % 10000 == 0) {
      auto current_time = std::chrono::steady_clock::now();
      double elapsed =
          std::chrono::duration_cast<std::chrono::duration<double>>(
              current_time - start_time)
              .count();
      double throughput = msg_count / elapsed;
      double avg_latency = total_latency / msg_count;
      std::cout << "Messages: " << msg_count << " | Throughput: " << throughput
                << " msgs/sec"
                << " | Avg Latency: " << avg_latency * 1000 << " ms"
                << " | Min Latency: " << min_latency * 1000 << " ms"
                << " | Max Latency: " << max_latency * 1000 << " ms"
                << std::endl;
    }
  }
  return 0;
}
