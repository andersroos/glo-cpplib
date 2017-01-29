#include <atomic>
#include <boost/test/unit_test.hpp>
#include <boost/throw_exception.hpp>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>


#include "glo.hpp"


using namespace glo;
using namespace std;
namespace json =  rapidjson;


struct http_response {

   string raw;
   
   http_response(string raw) : raw(raw) {}

   string status()
   {
      return raw.substr(0, raw.find("\r\n"));
   }

   string data()
   {
      return raw.substr(raw.find("\r\n\r\n") + 4);
   }

   json::Document json()
   {
      json::Document d;
      d.Parse(data().c_str());
      return d;
   }
   
};


http_response request(uint16_t port, std::string data)
{
    boost::system::error_code ec;
    using namespace boost::asio;

    io_service svc;
    ip::tcp::socket sock(svc);
    sock.connect({ {}, port });

    // send request
    sock.send(buffer(data));

    // read response
    std::string response;

    do {
        char buf[1024];
        auto recieved = sock.receive(buffer(buf), {}, ec);
        if (!ec) response.append(buf, buf + recieved);
    } while (!ec);

    return http_response(response);
}


BOOST_AUTO_TEST_CASE(basic_server_test)
{
   uint16_t var = 1;
   http_status_server server;
   server.add(&var, "/val", {tag::LAST}, 0, "A value.");

   std::thread t([&server](){ server.serve_once(10s); });
   
   auto response = request(server.port(), "GET / HTTP/1.1\r\n\r\n");

   t.join();   
   
   BOOST_CHECK_EQUAL("HTTP/1.1 200 OK", response.status());

   auto json = response.json();

   // Check that the timestamp is at least remotely correct.
   auto timestamp = json["timestamp"].GetDouble();
   BOOST_CHECK((2017. - 1970) * 365 * 24 * 3600 < timestamp);
   BOOST_CHECK(timestamp < (2018. - 1970) * 365 * 24 * 3600);
   
}

