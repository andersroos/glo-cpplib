#pragma once

#include <netinet/in.h>
#include <fcntl.h>
#include <string.h>

#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <glo/common.hpp>


// TODO Make sure all classes follows the rule of three/five/zero.


namespace glo {

   using namespace std::literals::chrono_literals;
   
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
   struct http_status_server : public group
   {
      // Create the server, which is also a group. For parameters key_prefix and mutexe see group doc. Set the port the
      // server should listen to with port, 0 for the first available port from 22200 to 22240. Throws glo::os_error on
      // failed system calls or if no free port is found.
      http_status_server() { bind(); }
      http_status_server(uint16_t port) : _port(port) { bind(); }
      http_status_server(std::string key_prefix, uint16_t port) : group(key_prefix), _port(port) { bind(); }
      http_status_server(std::shared_ptr<std::mutex> mutex, uint16_t port) : group(mutex), _port(port) { bind(); }
      http_status_server(std::string key_prefix, std::shared_ptr<std::mutex> mutex, uint16_t port)
         : group(key_prefix, mutex), _port(port) { bind(); }

      // Get the actual port used if not manually set, returns 0 if failed to bind on (any) port.
      inline uint16_t port();
      
      // Serves one request and returns or returns if no one connected after about timeout time has passed or returns an
      // unspecified time after stop is called. Throws glo::os_error on failed system calls. Returns false is there was
      // a timeout or if stop was called and true otherwise, note that true does not mean that a request was fully
      // completed.
      template<typename Rep, typename Period>
      bool serve_once(const std::chrono::duration<Rep, Period>& timeout);
      bool serve_once() { return serve_once(std::chrono::seconds(0)); };
      
      // Serve requests forever. Only returning an unspecified time after stop is called. Sleep sleep_time seconds
      // between each request, this works as a throttling mechanism. Throws glo::os_error on failed system calls.
      template<typename Rep, typename Period>
      void serve_forever(const std::chrono::duration<Rep, Period>& sleep_time);
      void serve_forever() { serve_forever(50ms); };
      
      // Start a new thread responding to requests (calls serve_forever). The thread will be stopped and joined if stop
      // is called. Throws glo::os_error on failed system calls.
      template<typename Rep, typename Period>
      void start(const std::chrono::duration<Rep, Period>& sleep_time);
      void start() { start(50ms); };
      
      // Stop serving. If a thread was started with start it will block until the thread is stopped, otherwise it will
      // return immediately.
      inline void stop();

      // TODO Add status_server glo statistics, meta!

      virtual ~http_status_server() { if (_socket != -1) close(_socket); }
      
   private:

      inline void bind();

      inline bool internal_serve_once(const std::chrono::microseconds& accept_timeout,
                                      const std::chrono::microseconds& poll_wait);
      
      inline bool handle_request(int server, const std::chrono::microseconds& poll_wait);
         
      inline std::string do_http(std::stringstream& request);

      int _socket{-1};
      uint16_t _port{0};
      std::unique_ptr<std::thread> _server_thread;
      std::atomic<bool> _stop{false};

   };

   //
   // Implementation.
   //

   // If a request is not completed completed within this time it will be closed.
   constexpr auto REQUEST_MAX_TIME = 2s;
      
   // Min time to wait between polling.
   constexpr auto MIN_POLL_WAIT = 200us;
   
   inline void set_non_blocking(int sock)
   {
      int val = fcntl(sock, F_GETFL, 0);
      if (fcntl(sock, F_SETFL, val | O_NONBLOCK) == -1) {
         sock = -1;
         throw os_error("failed to set socket to non blocking");
      }
   }
   
   void http_status_server::bind()
   {
      std::lock_guard<std::mutex> lock(_mutex);
      
      _socket = socket(AF_INET6, SOCK_STREAM, 0);
      if (_socket == -1) {
         throw os_error("failed to create socket");
      }

      set_non_blocking(_socket);
      
      int opt = 1;
      if (setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
         throw os_error("failed to set socket options");
      }

      uint16_t port_min = _port ? _port : 22200;
      uint16_t port_max = _port ? _port : 22240;

      for (_port = port_min; true; ++_port) {
         
         if (_port > port_max) {
            std::stringstream ss;
            ss << "could not bind socket on any port from port " << uint32_t(port_max) << " to " << uint32_t(port_max);
            throw os_error(ss.str());
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
               std::stringstream ss;
               ss << "could not bind socket on port " << uint32_t(_port); 
               throw os_error(ss.str());
            }
         }
      }

      if (listen(_socket, 32) == -1) {
         throw os_error("could not listen to socket");
      }
   }
   
   in_port_t http_status_server::port()
   {
      std::lock_guard<std::mutex> lock(_mutex);
      return _port;
   }

   template<typename Rep, typename Period>
   void http_status_server::serve_forever(const std::chrono::duration<Rep, Period>& sleep_time)
   {
      auto poll_wait = std::max(MIN_POLL_WAIT, std::chrono::duration_cast<std::chrono::microseconds>(sleep_time) / 10);
      auto accept_timeout = 24h;
      
      while (not _stop) {
         if (internal_serve_once(accept_timeout, poll_wait)) {
            std::this_thread::sleep_for(sleep_time);
         }
      }
   }

   template<typename Rep, typename Period>
   bool http_status_server::serve_once(const std::chrono::duration<Rep, Period>& timeout)
   {
      return internal_serve_once(std::chrono::duration_cast<std::chrono::microseconds>(timeout), MIN_POLL_WAIT);
   }

   bool http_status_server::internal_serve_once(const std::chrono::microseconds& accept_timeout,
                                                const std::chrono::microseconds& poll_wait)
   {
      auto accept_timeout_time = std::chrono::high_resolution_clock::now() + accept_timeout;

      int server = -1;
      while (true) {
         if (_stop) return false;
         
         server = accept(_socket, NULL, NULL);

         if (server != -1) {
            break;
         }
         
         if (errno == EAGAIN) {
            if (std::chrono::high_resolution_clock::now() > accept_timeout_time) {
               return false;
            }
            std::this_thread::sleep_for(poll_wait);
            continue;
         }
         
         throw std::runtime_error("error accepting connection");
      }
         
      return handle_request(server, poll_wait);
   }
   
   bool http_status_server::handle_request(int server, const std::chrono::microseconds& poll_wait)
   {
      char buf[2048];
      
      close_guard server_close(server);
  
      auto request_timeout_time = std::chrono::high_resolution_clock::now() + REQUEST_MAX_TIME;
            
      set_non_blocking(server);

      // Read request.
      std::stringstream data;
      while (true) {
         if (_stop) return false;
         
         auto received = recv(server, buf, sizeof(buf), 0);
         if (received == -1) {
            if (errno == EAGAIN) {
               if (std::chrono::high_resolution_clock::now() > request_timeout_time) {
                  // Request max time reached.
                  return true;
               }
               std::this_thread::sleep_for(poll_wait);
               continue;                     
            }
            // Failed to receive data, close connection.
            return true;

         }
         data.write(buf, received);
         data.seekg(-4, data.end);
         data.read(buf, 4);
         if (strncmp("\r\n\r\n", buf, 4) == 0) {
            // We have end of http request, read is complete.
            break;
         }
      }

      // Parse and create response.
      std::string response = do_http(data);

      // Send response.
      while (response.size()) {
         if (_stop) return false;

         auto sent = send(server, response.c_str(), response.size(), 0);
         if (sent == -1) {
            if (errno == EAGAIN) {
               if (std::chrono::high_resolution_clock::now() > request_timeout_time) {
                  // Request max time reached.
                  return true;
               }
               std::this_thread::sleep_for(poll_wait);
               continue;
            }
            // Failed to send data, close connection.
            return true;
         }
         response.erase(0, sent);
      }
      return true;
   }

   inline std::string error_response(const std::string& message)
   {
      return "HTTP/1.1 400 " + message + "\r\n\r\n";
   }

   std::string http_status_server::do_http(std::stringstream& request)
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
   
   template<typename Rep, typename Period>
   void http_status_server::start(const std::chrono::duration<Rep, Period>& sleep_time)
   {
      std::lock_guard<std::mutex> lock(_mutex);
      if (!_server_thread) {
         _server_thread = std::make_unique<std::thread>([this, sleep_time](){ this->serve_forever(sleep_time); });
      }
   }

   void http_status_server::stop()
   {
      _stop = true;
      
      std::lock_guard<std::mutex> lock(_mutex);
      if (_server_thread) {
         _server_thread->join();
         _server_thread.reset();
      }
   }
}
