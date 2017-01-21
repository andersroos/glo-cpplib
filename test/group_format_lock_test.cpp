#include <atomic>
#include <boost/test/unit_test.hpp>
#include <boost/throw_exception.hpp>
#include "glo/status.hpp"

using namespace glo;
using namespace std;



auto group_lock_test_mutex = make_shared<std::mutex>();


struct locked_exception {};

// Same as glo::json_formatter but raises exception if lock is taken.
template<typename V> struct locking_formatter
{
   void operator()(std::ostream& os, const V& value) const {
      if (group_lock_test_mutex->try_lock()) {
         group_lock_test_mutex->unlock();
         json_format(os, value);
      }
      else {
         throw locked_exception();
      }
   };
};


BOOST_AUTO_TEST_CASE(test_pointer_to_string_uses_format_while_locked_implementatino)
{
   string val = "str";
   group g(group_lock_test_mutex);
   g.add<decltype(&val), locking_formatter<decltype(&val)>>(&val, "", {}, 0, "");
   stringstream ss;
   BOOST_CHECK_THROW(g.json_format_items(ss, ""), locked_exception);
}

BOOST_AUTO_TEST_CASE(test_pointer_to_uint32_uses_copy_while_locked_implementation)
{
   uint32_t val = 10;
   group g(group_lock_test_mutex);
   g.add<decltype(&val), locking_formatter<decltype(&val)>>(&val, "", {}, 0, "");
   stringstream ss;
   g.json_format_items(ss, "");
}

BOOST_AUTO_TEST_CASE(test_ref_to_int64_uses_copy_while_locked_implementation)
{
   int64_t val = 10;
   group g(group_lock_test_mutex);
   g.add<decltype(ref(val)), locking_formatter<decltype(ref(val))>>(ref(val), "", {}, 0, "");
   stringstream ss;
   g.json_format_items(ss, "");
}

BOOST_AUTO_TEST_CASE(test_cref_to_bool_uses_copy_while_locked_implementation)
{
   bool val = true;
   group g(group_lock_test_mutex);
   g.add<decltype(cref(val)), locking_formatter<decltype(cref(val))>>(cref(val), "", {}, 0, "");
   stringstream ss;
   g.json_format_items(ss, "");
}

BOOST_AUTO_TEST_CASE(test_pointer_to_atomic_uint8_uses_formaat_while_locked_implementation)
{
   atomic<uint8_t> val(10);
   group g(group_lock_test_mutex);
   g.add<decltype(&val), locking_formatter<decltype(&val)>>(&val, "", {}, 0, "");
   stringstream ss;
   BOOST_CHECK_THROW(g.json_format_items(ss, ""), locked_exception);
}
 
BOOST_AUTO_TEST_CASE(test_shared_ptr_to_int8_uses_copy_while_locked_implementation)
{
   auto val = make_shared<int8_t>(-10);
   group g(group_lock_test_mutex);
   g.add<decltype(val), locking_formatter<decltype(val)>>(val, "", {}, 0, "");
   stringstream ss;
   g.json_format_items(ss, "");
}



