#pragma once

#include <type_traits>
#include <map>
#include <string>
#include <vector>
#include <memory>

namespace knowson {

	/// Describes the type of a json value
	enum class JsonType
	{
		kObject,
		kArray,
		kBoolean,
		kString,
		kNumber,
		kNull,
	};

	class JsonValue;
	class JsonObject;
	class JsonArray;
	class JsonBoolean;
	class JsonString;
	class JsonNumber;
	class JsonNull;

	namespace detail {
		template<typename T> struct JsonTypeConversion : public std::integral_constant<JsonType, JsonType::kNull> {};
		template<> struct JsonTypeConversion<JsonObject> : public std::integral_constant<JsonType, JsonType::kObject> {};
		template<> struct JsonTypeConversion<JsonArray> : public std::integral_constant<JsonType, JsonType::kArray> {};
		template<> struct JsonTypeConversion<JsonBoolean> : public std::integral_constant<JsonType, JsonType::kBoolean> {};
		template<> struct JsonTypeConversion<JsonString> : public std::integral_constant<JsonType, JsonType::kString> {};
		template<> struct JsonTypeConversion<JsonNumber> : public std::integral_constant<JsonType, JsonType::kNumber> {};
		template<> struct JsonTypeConversion<JsonNull> : public std::integral_constant<JsonType, JsonType::kNull> {};
	};

	class JsonValue
	{
	protected:
		JsonValue(JsonType type) : type_(type) {};

	public:
		virtual ~JsonValue(){};

		/// Returns the type of the json value
		JsonType type() const { return type_; }

		/// Returns true if this JsonValue is in fact
		template<typename T> bool is() const { return detail::JsonTypeConversion<T>::value == type(); }

		/// Converts this object to the specified json type
		template<typename T> const T& as() const { return *dynamic_cast<T*>(this); }
		template<typename T> T& as() const { return *dynamic_cast<T*>(this); }

	private:
		JsonType type_;
	};

	class JsonObject final : public JsonValue
	{
	public:
		typedef std::map<std::string, std::unique_ptr<JsonValue>> MemberMap;
		typedef MemberMap::iterator iterator;
		typedef MemberMap::const_iterator const_iterator;

	public:
		/// Default constructor
		JsonObject() : JsonValue(JsonType::kObject) {};

		/// Returns true if the given key exists in this instance
		bool has(const std::string& key) const { return members_.find(key) != members_.end(); }

		/// Returns the value with the given key
		const JsonValue& at(const std::string& key) const { return *members_.find(key)->second; }

		/// Tries the get the value with the given key
		bool try_get(const std::string& key, JsonValue const*& value) const;

    /// Insersts an item into the object
    void insert(const std::string& key, std::unique_ptr<JsonValue> value);

		/// Returns an iterator to the first element
		iterator begin() { return members_.begin(); }
		const_iterator begin() const { return members_.begin(); }

		/// Returns an iterator to the past the last element
		iterator end() { return members_.end(); }
		const_iterator end() const { return members_.end(); }

	private:
		MemberMap members_;
	};

	class JsonArray final : public JsonValue
	{
	public:
		typedef std::vector<std::unique_ptr<JsonValue>> ElementList;
		typedef ElementList::iterator iterator;
		typedef ElementList::const_iterator const_iterator;

	public:
		/// Default constructor
		JsonArray() : JsonValue(JsonType::kArray) {}

    /// Insert an element into the list
    void emplace_back(std::unique_ptr<JsonValue> &&element) { elements_.emplace_back(std::forward<std::unique_ptr<JsonValue>>(element)); }

		/// Returns an iterator to the first element
		iterator begin() { return elements_.begin(); }
		const_iterator begin() const { return elements_.begin(); }

		/// Returns an iterator to the past the last element
		iterator end() { return elements_.end(); }
		const_iterator end() const { return elements_.end(); }

	private:
		ElementList elements_;
	};

	class JsonBoolean : public JsonValue
	{
	public:
		/// Default constructor
		explicit JsonBoolean(bool value = false) : JsonValue(JsonType::kBoolean), value_(value) {}

		/// Returns the boolean value
		bool value() const { return value_; }

		/// Sets the value
		void set_value(bool value) { value_ = value; }

	private:
		bool value_;
	};

	class JsonNumber : public JsonValue
	{
	public:
		/// Default constructor
		explicit JsonNumber(double value = false) : JsonValue(JsonType::kNumber), value_(value) {}

		/// Returns the number value
		double value() const { return value_; }

		/// Sets the value
		void set_value(double value) { value_ = value; }

	private:
		double value_;
	};

	class JsonString : public JsonValue
	{
	public:
		/// Default constructor
		explicit JsonString(const std::string& value) : JsonValue(JsonType::kString), value_(value) {}

		/// Returns the string value
		const std::string& value() const { return value_; }

		/// Sets the value
		void set_value(const std::string& value) { value_ = value; }

	private:
		std::string value_;
	};

	class JsonNull : public JsonValue
	{
	public:
		/// Default constructor
		JsonNull() : JsonValue(JsonType::kNull) {}
	};
}