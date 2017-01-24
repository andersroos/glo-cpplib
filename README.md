[![Build Status](https://travis-ci.org/andersroos/glo-cpplib.svg?branch=master)](https://travis-ci.org/andersroos/glo-cpplib)

# Glo #

## About ##

C++ library for using Glo monitoring, the library is currently header
only. For more information on Glo see
[here](https://github.com/andersroos/glo).

Features:

* Status server for exposing internal values.

Current source version is 0.0.0-dev.1 and this lib uses [semantic
versioning](http://semver.org/).

## Usage Example ##

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

   // Create status server (letting the server select port).
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

Fore more examples see example dir in source.

## API Doc ##

TODO
