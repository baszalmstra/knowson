#pragma once

#include "json.h"

#include <cstdint>

namespace knowson {

	enum class JsonDocumentType
	{
		kUnknown,
		kNormal,
		kSimplified,
	};

	struct IJsonParserLog
	{
	public:
		/// Called when an error ocurred while parsing a document.
		virtual void Error(const char* msg, uint32_t line, uint32_t column) = 0;
	};

	struct IJsonParserSource
	{
	public:
		/// Reads character data from the source
		virtual uint32_t Read(char* buffer, uint32_t length) = 0;
	};

	template<typename S>
	struct StreamJsonParserSource : public IJsonParserSource
	{
	public:
		StreamJsonParserSource(S &stream) : stream_(stream) {}

		/// Reads character data from the source
		uint32_t Read(char* buffer, uint32_t length) override
		{
			stream_.read(buffer, length);
			return static_cast<uint32_t>(stream_.gcount());
		};

	private:
		S& stream_;
	};

	/// Parses a json document
	bool parse_json(IJsonParserSource *source, std::unique_ptr<JsonValue>& document, IJsonParserLog *log = nullptr, JsonDocumentType documentType = JsonDocumentType::kUnknown);
}