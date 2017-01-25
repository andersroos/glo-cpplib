#include <atomic>
#include <glo.hpp>

//
// Example of a group hierarchy.
//

int main()
{
   // Values and group for a cache.
   
   std::atomic<uint32_t> size1(0);
   std::atomic<uint32_t> hits1(0);
   std::atomic<uint32_t> misses1(0);
   auto cache_group1 = std::make_shared<glo::group>("/cache");
   cache_group1->add(std::cref(size1), "", {glo::tag::SIZE}, glo::level::HIGH, "Size of the cache.");
   cache_group1->add(std::cref(hits1), "/hit", {glo::tag::COUNT}, glo::level::MEDIUM, "Cache hit count.");
   cache_group1->add(std::cref(misses1), "/miss", {glo::tag::COUNT}, glo::level::MEDIUM, "Cache miss count.");

   // Values and group for another cache.
   
   std::atomic<uint32_t> size2(0);
   auto cache_group2 = std::make_shared<glo::group>("/cache");
   cache_group2->add(std::cref(size2), "", {glo::tag::SIZE}, glo::level::HIGH, "Size of the cache.");
   
   // Values and group for a request handler, that contains one of the caches.

   std::atomic<uint32_t> request_count(0);
   auto handler_group = std::make_shared<glo::group>("/request_handler");
   handler_group->add(std::cref(request_count), "/request", {glo::tag::COUNT}, glo::level::HIGH, "Number of requests.");
   handler_group->add_group(cache_group2);

   // Create status server which is also a group.
   glo::http_status_server server;

   // Add groups.
   server.add_group(handler_group);
   server.add_group(cache_group1, "/app");
   
   // Print bound port.
   std::cerr << "Started server on port " << server.port() << std::endl;
   
   // Start server thread.
   server.start();

   // Adding to values in the main loop.
   while (true) {
      ++size1;
      ++hits1;
      ++misses1;
      ++size2;
      ++request_count;
      usleep(1000);
   }
}
