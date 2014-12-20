#include "json.h"

namespace abbeycore {
namespace data {

	//-----------------------------------------------------------------------------------------------
	bool JsonObject::try_get(const std::string &key, JsonValue const *&value) const
	{
		auto it = members_.find(key);
		if(it == members_.end())
			return false;

		value = it->second.get();
		return true;
	}

  //-----------------------------------------------------------------------------------------------
  void JsonObject::insert(const std::string& key, std::unique_ptr<JsonValue> value)
  {
    members_.insert(std::make_pair(key, std::move(value)));
  }
}
}