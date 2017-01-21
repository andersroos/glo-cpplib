# Glo #

## About ##

Header only library for using Glo monitoring in C++. For more
information on Glo see [here](https://github.com/andersroos/glo).

Features:

* Status server for exposing internal values.

Current source version is 0.0.0-dev.0 and this lib uses [semantic
versioning](http://semver.org/).

## Examples ##

Basic usage example:

```c++
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
   glo::status_server server;

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
```

Fore more examples see [example dir](https://github.com/andersroos/glo-cpplib/examples) for usage examples.

## API Doc ##

TODO
