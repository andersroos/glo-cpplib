#include <atomic>
#include <glo.hpp>

//
// Basic example of a server exposing a counter.
//

int main()
{
   // Create a counter.
   std::atomic<uint32_t> count(0);

   // Create status server.
   glo::http_status_server server;

   // Print bound port.
   std::cerr << "Started server on port " << server.port() << std::endl;
   
   // Add ref to count to server, tagging it with COUNT.
   server.add(std::cref(count), "/server/basic", {glo::tag::COUNT}, glo::level::MEDIUM, "Simple counter.");

   // Start server thread.
   server.start();

   // Adding to the counter in main loop.
   while (true) {
      ++count;
      usleep(1000);
   }
}
