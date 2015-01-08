#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <map>
#include <vector>

namespace knowson {

  enum class ValueType
  {
    kString,
    kNumber,
    kArray,
    kObject
  };

  class ValueDefinition
  {
  protected:
    /// Default protected constructor
    ValueDefinition(const ValueType& type) : type_(type) {}
    ValueDefinition(const std::string& name, const ValueType& type) : name_(name), type_(type) {}      

  public:
    /// Returns the name of the definition
    const std::string& name() const { return name_; }

    /// Returns the type of the value
    ValueType base_type() const { return type_; };

  private:
    std::string name_;
    ValueType type_;
  };

  class CompoundValueDefinition : public ValueDefinition
  {
  protected:
    /// Default constructor
    CompoundValueDefinition(const ValueType& type) : ValueDefinition(type) {}
    CompoundValueDefinition(const std::string &name, const ValueType& type) : ValueDefinition(name, type) {}
  };

  class ObjectDefinition : public CompoundValueDefinition
  {
  public:
    /// Default constructor
    ObjectDefinition() : CompoundValueDefinition(ValueType::kObject) {}
    ObjectDefinition(const std::string &name) : CompoundValueDefinition(name, ValueType::kObject) {}

    /// Returns true if there is a member with the given name
    bool has(const std::string& name) const;

    /// Tries to return the member with the given name
    bool try_get(const std::string& name, ValueDefinition const*& definition) const;
    bool try_get(const std::string& name, ValueDefinition*& definition);

    /// Inserts a value with the given name into the object
    bool insert(const std::string& name, std::unique_ptr<ValueDefinition> value);

  private:
    std::map<std::string, std::unique_ptr<ValueDefinition>> members_;
  };

  class ArrayDefinition : public CompoundValueDefinition
  {
  public:
    /// Default constructor
    ArrayDefinition() : CompoundValueDefinition(ValueType::kArray) {}
    ArrayDefinition(const std::string &name) : CompoundValueDefinition(name, ValueType::kArray) {}

    /// Appends the given value
    void push_back(std::unique_ptr<ValueDefinition> value);

  private:
    std::vector<std::unique_ptr<ValueDefinition>> elements_;
  };
}
