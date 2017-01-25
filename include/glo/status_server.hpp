#pragma once

#include <netinet/in.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <glo/common.hpp>


namespace glo {

   //
   // The group is the class where status values are added. This way the status server (which is alos a group) can know
   // what values to serve. A group can also contain other groups.
   //
   // The group also have two additional features. It can collect status values under a key prefix and the group can be
   // put in a group wich also can have a prefix, and thus it is simple to create a hirearchy.
   //
   // The second feature is that if provided with a mutex it will lock the mutex while reading all status values,
   // preventing the need for callbacks and one lock per status value (if you can't use atomic).
   //
   struct group
   {
      group() {}
      group(std::string key_prefix) : _key_prefix(key_prefix) {};
      group(std::shared_ptr<std::mutex> mutex) : _value_mutex(mutex) {};
      group(std::string key_prefix, std::shared_ptr<std::mutex> mutex) : _key_prefix(key_prefix), _value_mutex(mutex) {}

      // TODO How JsonFormatter works is not optimal, maybe want to provide optional format function instead.
      
      // Add a value of type V to be returned by status call. Any types are accepted as long as there is a json
      // formatter for it. If providing a mutex the default implementation will format values to json directly while
      // holding the mutex. Raw pointer, std::ref or std::shared_ptr to fundamental types will be copied while holding
      // the mutex and then formatted afterwards trying to minimize the lock time.
      template<typename V, typename JsonFormatter = json_formatter<V> >
      void add(V val, std::string key, glo::tags_t tags, glo::level_t level, std::string desc);

      // // Provide a callback for value V.
      // template<typename V, typename JsonFormatter = json_formatter<V> >
      // void add_cb(std::function<V()> cb, glo::spec spec) {}
      
      // Add a group to this group, optionally providing a key prefix for all keys in the group.
      inline void add_group(std::shared_ptr<group> group, std::string key_prefix);
      inline void add_group(std::shared_ptr<group> group);
      
      // Read values and format items in this group into the stream. Each key will have key_prefix prepended when
      // formatting. Each item will be formatted as comma separated json dicts but no enclosing [] or ,.
      // TODO Make private.
      inline void json_format_items(std::ostream& os, const std::string key_prefix, const char*& delimiter);

   private:

      // Internal base class for referring values. When getting values locked_prepare will be called once while the
      // optionally provided mutex is locked, then json_format will be called when the mutex is relased.
      struct value;

      // Subclass template for object values, several Implementations, see below.
      template<typename V, typename JsonFormatter, typename Enable = void> struct object_value;

      // Remove and check for shared_ptr.
      template<typename T> struct remove_shared_ptr { };
      template<typename T> struct remove_shared_ptr<std::shared_ptr<T>> { using type = T; };

      // Remove and check for reference_wrapper.
      template<typename T> struct remove_reference_wrapper { };
      template<typename T> struct remove_reference_wrapper<std::reference_wrapper<T>> { using type = T; };
      
      // Json format everything static in the item, from the known end of the key until the : before the item value.
      inline std::string format_item_spec(std::string key, glo::tags_t tags, glo::level_t level, std::string desc);
      
      std::string _key_prefix;

      // TODO Make template to optionally use raw pointer instead of shared? Also for grops, really don't like the world
      // of forced heap allocation.
      
      // Optional mutex for values, shared with application code.
      std::shared_ptr<std::mutex> _value_mutex;

      // Vector with all added values in this group.
      std::vector<std::unique_ptr<value>> _values;

      // Vector with <prefix string, child group>.
      std::vector<std::pair<std::string, std::shared_ptr<group>>> _groups;

   protected:
      // Mutex for internal data structures.
      std::mutex _mutex;
   };

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
      status_server() : _port(0) { bind(); }
      status_server(uint16_t port) : _port(port) { bind(); }
      status_server(std::string key_prefix, uint16_t port) : group(key_prefix), _port(port) { bind(); }
      status_server(std::shared_ptr<std::mutex> mutex, uint16_t port) : group(mutex), _port(port) { bind(); }
      status_server(std::string key_prefix, std::shared_ptr<std::mutex> mutex, uint16_t port)
         : group(key_prefix, mutex), _port(port) { bind(); }

      // Get the actual port used if not manually set, returns 0 if failed to bind on (any) port.
      inline uint16_t port();
      
      // Serve one request and return or return after timeout seconds (0 for no timeout) or an unspecified time after
      // stop is called.
      inline void serve_once(double timeout=0); // TODO
      
      // Serve requests forever. Only returning an unspecified time after stop is called. Sleep sleep_time seconds
      // between each request, this works as a throttling mecanism.
      inline void serve_forever(double sleep_time=0.05);
      
      // Start a new thread responding to requests. The thread will be stopped and joined if stop is called.
      inline void start(double sleep_time=0.05);
      
      // Stop serving. If a thread was started with start it will block until the thread is stopped, otherwise it will
      // return immediately.
      inline void stop(); // TODO

      // TODO Make sure everything can be closed nicely in descrutor or method.

      // TODO Add status_server glo statistics, meta!
      
   private:

      inline void bind();
      
      inline std::string do_http(std::stringstream& request);

      int _socket;
      uint16_t _port;
      std::unique_ptr<std::thread> _server_thread;
   };

   //
   // Implementations.
   //

   //
   // group
   //
   
   struct group::value
   {
      value(std::string item_spec) : item_spec(item_spec) {}
      
      virtual void locked_prepare() const {}
      
      virtual void json_format(std::ostream& os) const {}
      
      virtual ~value() {}
      
      std::string item_spec;
   };

   // Fallback implementation for storing any kind of value, formatting when locked.
   template<typename V, typename JsonFormatter, typename Enable>
   struct group::object_value : public group::value
   {
      object_value(V val, std::string item_spec) : value(item_spec), _val(val) {}
      
      virtual void locked_prepare() const override
      {
         _formatter(_prepared, _val);
      }
      
      virtual void json_format(std::ostream& os) const override
      {
         os << _prepared.str();
         _prepared.str(std::string());
         _prepared.clear();
         _prepared << std::setprecision(19);
      }
         
      virtual ~object_value() {}

      JsonFormatter _formatter;
      V _val;
      mutable std::stringstream _prepared;
   };

   // V is a pointer to fundamanetal type specialization, copying when locked, formatting
   // when unlocked.
   template<typename V, typename JsonFormatter> struct group::object_value
   <V, JsonFormatter, typename std::enable_if<std::is_fundamental<typename std::remove_pointer<V>::type>::value>::type>
      : public group::value
   {
      object_value(V val, std::string item_spec) : value(item_spec), _val(val) {}
      
      virtual void locked_prepare() const override {
         _copy = *_val;
      }
      
      virtual void json_format(std::ostream& os) const override
      {
         os << std::setprecision(19);
         _formatter(os, &_copy);
      }
         
      virtual ~object_value() {}
   
      JsonFormatter _formatter;
      V _val;
      mutable typename std::remove_pointer<V>::type _copy;
   };

   // V is a shared_ptr to fundamanetal type specialization, copying when locked, formatting when unlocked.
   template<typename V, typename JsonFormatter> struct group::object_value
   <V, JsonFormatter, typename std::enable_if<std::is_fundamental<typename group::remove_shared_ptr<V>::type>::value>::type>
      : public group::value
   {
      object_value(V val, std::string item_spec) :
         value(item_spec), _val(val), _copy(std::make_shared<typename V::element_type>()) {}
      
      virtual void locked_prepare() const override
      {
         *_copy = *_val;
      }
      
      virtual void json_format(std::ostream& os) const override
      {
         os << std::setprecision(19);
         _formatter(os, _copy);
      }
         
      virtual ~object_value() {}
   
      JsonFormatter _formatter;
      V _val;
      mutable V _copy;
   };

   // V is a reference_wrapper to fundamanetal type specialization, copying when locked, formatting when unlocked.
   template<typename V, typename JsonFormatter> struct group::object_value
   <V, JsonFormatter, typename std::enable_if<std::is_fundamental<typename group::remove_reference_wrapper<V>::type>::value>::type>
      : public group::value
   {
      object_value(V val, std::string item_spec) :
         value(item_spec), _val(val), _copy(), _ref(_copy) {}
      
      virtual void locked_prepare() const override
      {
         _copy = _val;
      }
      
      virtual void json_format(std::ostream& os) const override
      {
         os << std::setprecision(19);
         _formatter(os, _ref);
      }
         
      virtual ~object_value() {}
   
      JsonFormatter _formatter;
      V _val;
      mutable typename std::remove_const<typename V::type>::type _copy;
      mutable V _ref;
   };
   
   template<typename V, typename JsonFormatter>
   void group::add(V val, std::string key, glo::tags_t tags, glo::level_t level, std::string desc)
   {
      std::lock_guard<std::mutex> lock(_mutex);
      auto item_spec = format_item_spec(key, tags, level, desc);
      _values.emplace_back(std::make_unique<object_value<V, JsonFormatter>>(val, item_spec));
   }
   
   void group::add_group(std::shared_ptr<group> group)
   {
      add_group(group, "");
   }
   
   void group::add_group(std::shared_ptr<group> group, std::string key_prefix)
   {
      std::lock_guard<std::mutex> lock(_mutex);
      _groups.emplace_back(make_pair(key_prefix, group));
   }
   
   std::string group::format_item_spec(std::string key, glo::tags_t tags, glo::level_t level, std::string desc)
   {
      std::stringstream ss;
      ss << escape_json(_key_prefix) << escape_json(key) << ":";
      const char* delimiter = "";
      for (auto tag : tags) {
         ss << delimiter << tag;
         delimiter = "-";
      }
      ss << "\",\"level\":" << level << ",\"desc\":\"" << escape_json(desc) << "\",\"value\":";
      return ss.str();
   }
   
   void group::json_format_items(std::ostream& os, const std::string key_prefix, const char*& delimiter)
   {
      std::lock_guard<std::mutex> lock(_mutex);
      {
         std::unique_lock<std::mutex> value_lock;
         if (_value_mutex) {
            value_lock = std::unique_lock<std::mutex>(*_value_mutex);
         }

         for (auto& value : _values) {
            value->locked_prepare();
         }
      }
      
      auto escaped_key_prefix = escape_json(key_prefix);

      for (auto& value : _values) {
         os << delimiter << "{\"key\":\"" << escaped_key_prefix << value->item_spec;
         value->json_format(os);
         os << "}";
         delimiter = ",";
      }

      for (auto& p : _groups) {
         p.second->json_format_items(os, key_prefix + _key_prefix + p.first, delimiter);
      }
   }  

   //
   // status_server
   //
   
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

   inline std::string error_response(std::string message)
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
