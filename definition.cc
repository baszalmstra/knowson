#include "definition.h"

namespace knowson
{
	//--------------------------------------------------------------------------------------------------------------------
	bool ObjectDefinition::has(const std::string &name) const
	{
    return members_.find(name) != members_.end();
	}

	//--------------------------------------------------------------------------------------------------------------------
	bool ObjectDefinition::try_get(const std::string &name, ValueDefinition*& definition)
	{
    auto it = members_.find(name);
		if(it == members_.end())
			return false;

    definition = it->second.get();
		return true;
	}

	//--------------------------------------------------------------------------------------------------------------------
  bool ObjectDefinition::try_get(const std::string &name, ValueDefinition const*& definition) const
	{
    auto it = members_.find(name);
    if (it == members_.end())
      return false;

    definition = it->second.get();
    return true;
	}

  //--------------------------------------------------------------------------------------------------------------------
  bool ObjectDefinition::insert(const std::string& name, std::unique_ptr<ValueDefinition> value)
  {
    /// Returns false on a duplicate key
    if (has(name))
      return false;

    /// Insert the element
    members_.emplace(std::map<std::string, std::unique_ptr<ValueDefinition>>::value_type(name, std::move(value)));
    return true;
  }

  //--------------------------------------------------------------------------------------------------------------------
  void ArrayDefinition::push_back(std::unique_ptr<ValueDefinition> value)
  {
    elements_.emplace_back(std::move(value));
  }
}