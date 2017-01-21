#pragma once

#include <sstream>
#include <iomanip>
#include <memory>

namespace glo {

   //
   // JSON helpers.
   //

   //
   // Escape chars that needs escaping (\, " and control chars). Assuming string is already utf-8 encoded, nothing more
   // should be needed since json is encoded utf-8 by default.
   //
   inline std::string escape_json(const std::string& str)
   {
      std::ostringstream ss;
      for (auto c : str) {
         if (c == '"' or c == '\\' or (0x00 <= c and c <= 0x1f)) {
            ss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << uint32_t(c);
         }
         else {
            ss << c;
         }
      }
      return ss.str();
   }

   // TODO Change implementations to template specializations or does it really matter?
   
   // JSON formatting functions used by the json_formatter class.
   inline void json_format(std::ostream& os, const std::string& value) { os << '"' << escape_json(value) << '"'; }
   inline void json_format(std::ostream& os, const char* value) { os << '"' << escape_json(value) << '"'; }
   inline void json_format(std::ostream& os, const uint64_t& value) { os << value; }
   inline void json_format(std::ostream& os, const uint32_t& value) { os << value; }
   inline void json_format(std::ostream& os, const uint16_t& value) { json_format(os, uint32_t(value)); }
   inline void json_format(std::ostream& os, const uint8_t& value) { json_format(os, uint32_t(value)); }
   inline void json_format(std::ostream& os, const int64_t& value) { os << value; }
   inline void json_format(std::ostream& os, const int32_t& value) { os << value; }
   inline void json_format(std::ostream& os, const int16_t& value) { json_format(os, int32_t(value)); }
   inline void json_format(std::ostream& os, const int8_t& value) { json_format(os, int32_t(value)); }
   inline void json_format(std::ostream& os, const char& value) { os << '"' << value << '"'; }
   inline void json_format(std::ostream& os, const bool& value) { os << (value ? "true" : "false"); };
   
   template<typename V> void json_format(std::ostream& os, const V* value) { json_format(os, *value); }
   template<typename V> void json_format(std::ostream& os, const std::shared_ptr<V> value) { json_format(os, *value); }
   template<typename V> void json_format(std::ostream& os, const std::reference_wrapper<V>& value) { json_format(os, value.get()); }
   
   // The default json formatter that just calls json_format for the type V.
   template<typename V> struct json_formatter
   {
      void operator()(std::ostream& os, const V& value) const { json_format(os, value); };
   };

   
}
