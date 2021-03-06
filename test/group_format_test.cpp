#include <atomic>
#include <boost/test/unit_test.hpp>
#include <boost/throw_exception.hpp>

#include <glo.hpp>

using namespace glo;
using namespace std;


BOOST_AUTO_TEST_CASE(test_format_pointer_to_updating_string)
{
   string val = "str";
   group g;
   g.add(&val, "a_str", {tag::COUNT}, 0, "A string.");
   {
      stringstream ss;
      const char* delimiter = "";
      g.json_format_items(ss, "", delimiter);
      BOOST_CHECK_EQUAL(R""({"key":"a_str:count","level":0,"desc":"A string.","value":"str"})"", ss.str());
   }
   val = "123";
   {
      stringstream ss;
      const char* delimiter = "";
      g.json_format_items(ss, "", delimiter);
      BOOST_CHECK_EQUAL(R""({"key":"a_str:count","level":0,"desc":"A string.","value":"123"})"", ss.str());
   }
}

BOOST_AUTO_TEST_CASE(test_format_pointer_to_updating_uint32)
{
   uint32_t val = 12;
   group g;
   g.add(&val, "an_int", {tag::COUNT}, 0, "An int.");
   {
      stringstream ss;
      const char* delimiter = "";
      g.json_format_items(ss, "", delimiter);
      BOOST_CHECK_EQUAL(R""({"key":"an_int:count","level":0,"desc":"An int.","value":12})"", ss.str());
   }
   val = 123;
   {
      stringstream ss;
      const char* delimiter = "";
      g.json_format_items(ss, "", delimiter);
      BOOST_CHECK_EQUAL(R""({"key":"an_int:count","level":0,"desc":"An int.","value":123})"", ss.str());
   }
}

BOOST_AUTO_TEST_CASE(test_format_ref_to_updating_int64)
{
   int64_t val = -12;
   group g;
   g.add(ref(val), "neg_int", {tag::LAST}, level::LOW, "Negative int.");
   {
      stringstream ss;
      const char* delimiter = "";
      g.json_format_items(ss, "", delimiter);
      BOOST_CHECK_EQUAL(R""({"key":"neg_int:last","level":3,"desc":"Negative int.","value":-12})"", ss.str());
   }
   val = -123;
   {
      stringstream ss;
      const char* delimiter = "";
      g.json_format_items(ss, "", delimiter);
      BOOST_CHECK_EQUAL(R""({"key":"neg_int:last","level":3,"desc":"Negative int.","value":-123})"", ss.str());
   }
}

BOOST_AUTO_TEST_CASE(test_format_cref_to_updating_bool)
{
   bool val = false;
   group g;
   g.add(cref(val), "bool", {tag::LAST}, level::LOW, "Bool.");
   {
      stringstream ss;
      const char* delimiter = "";
      g.json_format_items(ss, "", delimiter);
      BOOST_CHECK_EQUAL(R""({"key":"bool:last","level":3,"desc":"Bool.","value":false})"", ss.str());
   }
   val = true;
   {
      stringstream ss;
      const char* delimiter = "";
      g.json_format_items(ss, "", delimiter);
      BOOST_CHECK_EQUAL(R""({"key":"bool:last","level":3,"desc":"Bool.","value":true})"", ss.str());
   }
}

BOOST_AUTO_TEST_CASE(test_format_pointer_to_updating_atomic_uint8)
{
   atomic<uint8_t> val(12);
   group g;
   g.add(&val, "atomic", {tag::COUNT}, 0, "Atomic.");
   {
      stringstream ss;
      const char* delimiter = "";
      g.json_format_items(ss, "", delimiter);
      BOOST_CHECK_EQUAL(R""({"key":"atomic:count","level":0,"desc":"Atomic.","value":12})"", ss.str());
   }
   val = 123;
   {
      stringstream ss;
      const char* delimiter = "";
      g.json_format_items(ss, "", delimiter);
      BOOST_CHECK_EQUAL(R""({"key":"atomic:count","level":0,"desc":"Atomic.","value":123})"", ss.str());
   }
}

BOOST_AUTO_TEST_CASE(test_format_shared_ptr_to_updating_int8)
{
   auto val = make_shared<int8_t>(-12);
   group g;
   g.add(val, "shared", {tag::COUNT}, 0, "Shared.");
   {
      stringstream ss;
      const char* delimiter = "";
      g.json_format_items(ss, "", delimiter);
      BOOST_CHECK_EQUAL(R""({"key":"shared:count","level":0,"desc":"Shared.","value":-12})"", ss.str());
   }
   *val = 123;
   {
      stringstream ss;
      const char* delimiter = "";
      g.json_format_items(ss, "", delimiter);
      BOOST_CHECK_EQUAL(R""({"key":"shared:count","level":0,"desc":"Shared.","value":123})"", ss.str());
   }
}

BOOST_AUTO_TEST_CASE(test_format_json_escaping)
{
   BOOST_CHECK_EQUAL("abc", escape_json("abc"));
   BOOST_CHECK_EQUAL("\\u0022", escape_json("\""));
   BOOST_CHECK_EQUAL("\\u000a", escape_json("\n"));
   BOOST_CHECK_EQUAL("ä", escape_json("ä"));
}

BOOST_AUTO_TEST_CASE(test_json_format)
{
   {
      stringstream ss;
      json_format(ss, "abc"s);
      BOOST_CHECK_EQUAL("\"abc\"", ss.str());
   }

   {
      stringstream ss;
      json_format(ss, "abc");
      BOOST_CHECK_EQUAL("\"abc\"", ss.str());
   }

   {
      stringstream ss;
      json_format(ss, 'a');
      BOOST_CHECK_EQUAL("\"a\"", ss.str());
   }

   {
      stringstream ss;
      json_format(ss, false);
      BOOST_CHECK_EQUAL("false", ss.str());
   }
}
