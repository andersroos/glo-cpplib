#pragma once

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <thread>

namespace glo {

   //
   // A class for servring status messages using a minimal HTTP (only 1.1 supported) server implementation (supports
   // only GET and will close the connection after every request). The server will return the same response for all
   // paths. All methods are thread safe.
   //
   // It is important that the server is not accessed to often because each request will possible lock mutexes shared
   // with the actual business logic. Therefore the server is throttled by sleeping 50 ms after each request. This is
   // configurable.
   //
   // It is possible to get the reposnse in two formats (controlled by HTTP query parameters):
   //
   // a json object as application/json: the default
   //
   // a jsonp callback as application/javascript: by adding callback=<javascript function name> as a request parameter
   //
   struct status_server : public group
   {
      // Create the server, which is also a group. For parameters key_prefix and mutexe see group doc. Set the port the
      // server should listen to with port, 0 for the first available port from 22200 to 22240
      status_server(uint16_t port=0);
      status_server(std::string key_prefix, uint16_t port=0);
      status_server(std::shared_ptr<std::mutex> mutex, uint16_t port=0);
      status_server(std::string key_prefix, std::shared_ptr<std::mutex> mutex, uint16_t port=0);

      // Get the actual port used if not manually set, returns 0 if failed to bind on (any) port.
      uint16_t port();
      
      // Serve one request and return or return after timeout seconds (0 for no timeout) or an unspecified time after
      // stop is called.
      void serve_once(double timeout=0); // TODO
      
      // Serve requests forever. Only returning an unspecified time after stop is called. Sleep sleep_time seconds
      // between each request, this works as a throttling mecanism.
      void serve_forever(double sleep_time=0.05);
      
      // Start a new thread responding to requests. The thread will be stopped and joined if stop is called.
      void start(double sleep_time=0.05);
      
      // Stop serving. If a thread was started with start it will block until the thread is stopped, otherwise it will
      // return immediately.
      void stop(); // TODO

      // TODO Make sure everything can be closed nicely in descrutor or method.

      // TODO Add status_server glo statistics, meta!
      
   private:

      void bind();
      
      std::string do_http(std::stringstream& request);

      int _socket;
      uint16_t _port;
      std::unique_ptr<std::thread> _server_thread;
   };

   status_server::status_server(uint16_t port)
      : _port(port)
   {
      bind();
   }
   
   status_server::status_server(std::string key_prefix, uint16_t port)
      : group(key_prefix), _port(port)
   {
      bind();
   }
   
   status_server::status_server(std::shared_ptr<std::mutex> mutex, uint16_t port)
      : group(mutex), _port(port)
   {
      bind();
   }
   
   status_server::status_server(std::string key_prefix, std::shared_ptr<std::mutex> mutex, uint16_t port)
      : group(key_prefix, mutex), _port(port)
   {
      bind();
   }

   // TODO Implement error handling, exceptions, error codes or both?
   void status_server::bind()
   {
      std::lock_guard<std::mutex> lock(_mutex);
      
      _socket = socket(AF_INET6, SOCK_STREAM, 0);
      if (_socket == -1) {
         throw std::runtime_error("failed to create socket");
      }

      int val;
      val = fcntl(_socket, F_GETFL, 0);
      if (fcntl(_socket, F_SETFL, val | O_NONBLOCK) == -1) {
         _socket = -1;
         throw std::runtime_error("failed to set socket to non blocking");
      }
      
      int opt = 1;
      if (setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
         throw std::runtime_error("failed to set socket options");
      }

      uint16_t port_min = _port ? _port : 22200;
      uint16_t port_max = _port ? _port : 22240;

      for (_port = port_min; true; ++_port) {
         
         if (_port > port_max) {
            throw std::runtime_error("could not bind socket on any port from port TODO to TODO.");
         }
         
         struct sockaddr_in6 addr;
         memset(&addr, 0, sizeof(sockaddr_in6));
         addr.sin6_family = AF_INET6;
         addr.sin6_port = htons(_port);
         
         if (::bind(_socket, (sockaddr*) & addr, sizeof(sockaddr_in6)) == 0) {
            break;
         }
         else {
            if (EADDRINUSE != errno) {
               throw std::runtime_error("could not bind socket on port"); // TODO _port
            }
         }
      }

      if (listen(_socket, 32) == -1) {
         throw std::runtime_error("could not listen to socket");
      }
   }
   
   in_port_t status_server::port()
   {
      std::lock_guard<std::mutex> lock(_mutex);
      return _port;
   }

   void status_server::serve_forever(double sleep_time)
   {
      // TODO Implement read timeout and accept timeout.
      char buf[2048];
      int server = -1;
         
      while (true) {
         {
            std::lock_guard<std::mutex> lock(_mutex);
            server = accept(_socket, NULL, NULL);
         }
         if (server == -1) {
            if (errno == EAGAIN) {
               usleep(std::max<uint64_t>(200, sleep_time * 2e5));
               continue;
            }
            
            throw std::runtime_error("error accepting connection"); // TODO errno
         }
            
         std::stringstream data;
         ssize_t received;
         while ((received = recv(server, buf, sizeof(buf), 0)) > 0) {
            data.write(buf, received);
            data.seekg(-4, data.end);
            data.read(buf, 4);
            if (strncmp("\r\n\r\n", buf, 4) == 0) {
               std::string response = do_http(data);
         
               if (send(server, response.c_str(), response.size(), 0) == -1) {
                  // Failed to send data, just ignore this error.
               }
               break;
            }
         }

         close(server); // TODO Make sure closed on any exception.

         usleep(uint64_t(sleep_time * 1e6));
      }
   }

   std::string error_response(std::string message)
   {
      return "HTTP/1.1 400 " + message + "\r\n\r\n";
   }

   std::string status_server::do_http(std::stringstream& request)
   {
      // Parse request.

      std::string method;
      std::string url;
      std::string version;

      request.seekg(0);
      if (not request.good()) return error_response("empty request");
      
      std::getline(request, method, ' ');
      if (not request.good()) return error_response("missing method");
      if (method != "GET") return error_response("only get is supported");
      
      std::getline(request, url, ' ');
      if (not request.good()) return error_response("missing url");
      
      std::getline(request, version, '\r');
      if (not request.good()) return error_response("missing version");
      if (version != "HTTP/1.1") return error_response("only http/1.1 is supported");

      std::string cb;
      
      auto index = url.find("callback=");
      if (index != std::string::npos and index > 0 and (url[index - 1] == '?' or url[index - 1] == '&')) {
         auto end = url.find('&', index + 9);
         if (end == std::string::npos) {
            cb = url.substr(index + 9);
         }
         else {
            cb = url.substr(index + 9, end - index - 9);
         }
      }

      // Format response.
      
      std::stringstream content;
      
      content << std::setprecision(19);
      
      if (cb.length()) {
         content << cb << "(";
      }
      
      std::chrono::duration<double> now = std::chrono::system_clock::now().time_since_epoch();
      content << "{\"version\":4,\"timestamp\":" << now.count() << ",\"items\":[";

      const char* delimiter = "";
      json_format_items(content, "", delimiter);
      
      content << "]}";
      
      if (cb.length()) {
         content << ");";
      }

      auto content_str = content.str();

      std::stringstream response;

      response << "HTTP/1.1 200 OK\r\n";
      if (cb.length()) {
         response << "Content-Type: application/javascript; charset=utf-8\r\n";
      }
      else {
         response << "Content-Type: application/json; charset=utf-8\r\n";
      }
      response << "Cache-Control: no-cache, no-store" << "\r\n"
               << "Content-Length: " << content_str.length() << "\r\n"
               << "\r\n"
               << content_str;
      
      return response.str();
   }
   
   void status_server::start(double sleep_time)
   {
      std::lock_guard<std::mutex> lock(_mutex);
      if (!_server_thread) {
         _server_thread = std::make_unique<std::thread>([this, sleep_time](){ this->serve_forever(sleep_time); });
      }
   }
   
}
