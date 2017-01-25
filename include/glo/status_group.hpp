#pragma once

#include <mutex>

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
   // Implementation.
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

   
}
