#include "json_parser.h"

#include <cstdarg>
#include <cstdio>
#include <array>
#include <string>
#include <fstream>
#include <iostream>
#include <cctype>

namespace knowson {

	namespace
	{
    /**
     * @brief Describes a piece of data from a source location
     */
		struct ParseBlock
		{
			ParseBlock() : size(0), next(nullptr) {};

			std::array<char, 1024> data;
			uint32_t size;
			ParseBlock *next;
		};

    /**
     * @brief Describes a selection from the source data. The selection can span multiple 
     *  ParseBlocks.
     */
		struct Selection
		{
			Selection() : start(0), end(0), startBlock(nullptr), endBlock(nullptr), line(0), column(0) {}

			uint32_t start, end;
			ParseBlock *startBlock;
			ParseBlock *endBlock;
			uint32_t line, column;

      /// Exctracts the data from the selection in the form of a string
      std::string data() const
      {
        // Determine the size of the selection
        uint32_t size = 0;
        ParseBlock const* current = startBlock;
        while (current != endBlock)
        {
          size += current == startBlock ? current->size - start : current->size;
          current = current->next;
        }
        uint32_t endOffset = (endBlock == startBlock ? start : 0);
        size += end - endOffset;

        // Capture the result in a string
        std::string result;
        result.reserve(size);
        current = startBlock;
        size = 0;
        while (current != endBlock)
        {
          uint32_t copyStart = current == startBlock ? start : 0;
          result.append(current->data.data() + copyStart, current->size - copyStart);
          current = current->next;
        }
        result.append(endBlock->data.data() + endOffset, end - endOffset);
        return result;
      }
		};

		//-----------------------------------------------------------------------------------------------
		enum class TokenType
		{
			kIdentifier,
			kString,
			kNumber,
			kCurlyLeft,
			kCurlyRight,
			kSeperator,
			kBraceLeft,
			kBraceRight,
			kComma,
			kComment,
			kEOF,
      kTrue,
      kFalse,
      kNull,
		};

    const char *token_to_string(TokenType t)
    {
      switch (t)
      {
      case TokenType::kIdentifier:
        return "identifier";
      case TokenType::kString:
        return "string";
      case TokenType::kNumber:
        return "number";
      case TokenType::kCurlyLeft:
        return "{";
      case TokenType::kCurlyRight:
        return "}";
      case TokenType::kSeperator:
        return "=";
      case TokenType::kBraceLeft:
        return "[";
      case TokenType::kBraceRight:
        return "]";
      case TokenType::kComma:
        return ",";
      case TokenType::kComment:
        return "comment";
      default:
      case TokenType::kEOF:
        return "EOF";
      }
    }

		//-----------------------------------------------------------------------------------------------
		struct Token
		{
			TokenType type;
			Selection selection;
		};

		//-----------------------------------------------------------------------------------------------
		class ParseContext
		{
		public:
      /// Default constructor
			ParseContext(IJsonParserLog *l, IJsonParserSource *s, JsonDocumentType t) :
        log(l), source(s), documentType(t),
				line(1), column(0),
				firstEmptyBlock(nullptr),
        currentBlock(nullptr) {}

      /// Default destructor
      virtual ~ParseContext()
      {
        /// Delete the parse block that are owned by the curren token
        ParseBlock *block = currentToken.selection.startBlock;
        while (block != currentBlock)
        {
          ParseBlock *next = block->next;
          delete block;
          block = next;
        }

        /// Delete the parse block that is currently being used
        /// It may happen that there is no token yet so that's why this is here
        if (currentBlock)
        {
          delete currentBlock;
          currentBlock = nullptr;
        }

        /// Delete the all the free blocks
        block = firstEmptyBlock;
        while (block != nullptr)
        {
          ParseBlock *next = block->next;
          delete block;
          block = next;
        }
      }

      /// Registers an error with the error log
			bool error(const char *format, ...)
			{
				if (log == nullptr)
					return false;

				char formattedText[512];
				va_list args;
				va_start(args, format);
				vsprintf(formattedText, format, args);
				va_end(args);

				log->Error(formattedText, line, column);

				return false;
			}

      /// Called to advance to the next token. Returns false if there are no more tokens left.
			bool next()
			{
        do
        {
          release_current_token();

          char c;
          bool result;
          while ((result = next_char(c, false)) && std::isspace(c))
            swallow_char();
          if (!result)
          {
            currentToken.type = TokenType::kEOF;
            return false;
          }

          currentToken.selection.startBlock = currentBlock;
          currentToken.selection.start = position;

          if (c == '{')
          {
            currentToken.type = TokenType::kCurlyLeft;
            swallow_char();
          }
          else if (c == '}')
          {
            currentToken.type = TokenType::kCurlyRight;
            swallow_char();
          }
          else if (c == '[')
          {
            currentToken.type = TokenType::kBraceLeft;
            swallow_char();
          }
          else if (c == ']')
          {
            currentToken.type = TokenType::kBraceRight;
            swallow_char();
          }
          else if (is_seperator(c))
          {
            currentToken.type = TokenType::kSeperator;
            swallow_char();
          }
          else if (c == ',')
          {
            currentToken.type = TokenType::kComma;
            swallow_char();
          }
          else if (c == '-')
          {
            swallow_char();

            char a;
            if (!next_char(a, false))
              return unexpected_eof();

            if (a == '-')
            {
              currentToken.type = TokenType::kComment;
              select_line();
            }
            else
              select_number(true);
          }
          else if (c == '+')
          {
            swallow_char();
            select_number(true);
          }
          else if (std::isdigit(c))
          {
            select_number(false);
          }
          else if (c == '/')
          {
            swallow_char();

            char a;
            if (!next_char(a))
              return unexpected_eof();

            if (a == '/')
            {
              currentToken.type = TokenType::kComment;
              select_line();
            }
            else
            {
              select_identifier();
            }
          }
          else if (c == '\"')
            select_string();
          else
            select_identifier();
        } while (currentToken.type == TokenType::kComment);

        return true;
			}

      /// Returns the current token
			const Token& token() const
			{
				return currentToken;
			}

      /// Returns the document type
      JsonDocumentType document_type() const
      {
        return documentType;
      }

      /// Set the document type
      void set_document_type(JsonDocumentType t)
      {
        documentType = t;
      }

		private:
      /// Returns true if the given character is considered a seperator according to the current 
      /// document type.
      bool is_seperator(char c)
      {
        return c == ':' || (c == '=' && documentType != JsonDocumentType::kNormal);
      }

      /// Allocates a new block either from the free list or from system memory and initializes it 
      /// with content from the source. If the source is drained it returns 0.
			ParseBlock* allocate_next_block()
			{
				std::unique_ptr<ParseBlock> block;
				if(firstEmptyBlock != nullptr)
				{
					block.reset(firstEmptyBlock);
					firstEmptyBlock = firstEmptyBlock->next;
				}
				else
					block.reset(new ParseBlock);

				block->next = nullptr;
        block->size = 0;

				while(block->size < block->data.size())
				{
					uint32_t bytesRead = source->Read(block->data.data() + block->size, static_cast<uint32_t>(block->data.size() - block->size));
					if(bytesRead == 0)
						return (ParseBlock *) (block->size == 0 ? nullptr : block.release());

					block->size += bytesRead;
				}

				return block.release();
			}

      /// Called to get the current character from the source, optionally moving to the next character.
			bool next_char(char &c, bool moveCursor = true)
			{
				while(currentBlock == nullptr || position >= currentBlock->size)
				{
					std::unique_ptr<ParseBlock> block(allocate_next_block());
					if(!block)
						return false;

					position = currentBlock ? position - currentBlock->size : 0;
          if (currentBlock)
					  currentBlock->next = block.get();
					currentBlock = block.release();
				}

				c = currentBlock->data[position];
        if (moveCursor) swallow_char();
				return true;
			}

      /// Frees all excess memory used by the current token. 
			void release_current_token()
			{
				ParseBlock *previousBlock = currentToken.selection.startBlock;
				while(previousBlock != currentBlock)
				{
					ParseBlock *next = previousBlock->next;
					previousBlock->next = firstEmptyBlock;
					firstEmptyBlock = previousBlock;
					previousBlock = next;
				}
			}

      /// Moves the cursor until either no more content is left or a newline is hit.
			void select_line()
			{
				char c;
				while(next_char(c) && c != '\n');
				currentToken.selection.end = position;
				currentToken.selection.endBlock = currentBlock;
			}

      /// Moves the cursor to include the entirty of a number. However if an unexpected character
      /// is found the token is turned into an identifier.
			void select_number(bool hadDecimal)
			{
				currentToken.type = TokenType::kNumber;

				bool hadE = false;
				bool hadSign = true;
				char c;
				while(next_char(c, false))
				{
          if (std::isdigit(c))
          {
            swallow_char();
            continue;
          }
					else if(c == '.' && !hadDecimal)
					{
            swallow_char();
						hadDecimal = true;
						continue;
					}
					else if((c == '-' || c == '+') && !hadSign)
					{
            swallow_char();
						hadSign = true;
						continue;
					}
					else if((c == 'E' || c == 'e') && !hadE)
					{
            swallow_char();
						hadSign = false;
						hadE = true;
						hadDecimal = true;
						continue;
					}
          else if (std::isspace(c) || 
            is_seperator(c) ||
            c == '}' ||
            c == '{' ||
            c == '[' ||
            c == ']' ||
            c == ',')
          {
            break;
          }

					select_identifier();
					return;
				}

				currentToken.selection.end = position;
				currentToken.selection.endBlock = currentBlock;
			}

      /// Selects the entirty of an identifier
			bool select_identifier()
			{
				currentToken.type = TokenType::kIdentifier;

        static const char keywordTrue[] = "true";
        static const char keywordFalse[] = "false";
        static const char keywordNull[] = "null";

				char c;
        bool result;
        uint32_t i = 0;
        bool isTrue = true, isFalse = true, isNull = true;
        while ((result = next_char(c, false)) && !std::isspace(c) && 
          !is_seperator(c) &&
          c != '}' && 
          c != '{' &&
          c != '[' &&
          c != ']' &&
          c != ',')
        {
          isTrue = isTrue && i < 4 && keywordTrue[i] == c;
          isFalse = isFalse && i < 5 && keywordFalse[i] == c;
          isNull = isNull && i < 4 && keywordNull[i] == c;
          swallow_char();
          i++;
        }

        currentToken.selection.end = position;
        currentToken.selection.endBlock = currentBlock;

        if (result)
        {
          if (isTrue && i == 4) currentToken.type = TokenType::kTrue;
          else if (isFalse && i == 5) currentToken.type = TokenType::kFalse;
          else if (isNull && i == 4) currentToken.type = TokenType::kNull;
        }
				
        if (!result)
          return unexpected_eof();

        return result;
			}

      /// Called when an unexpected end-of-file was encountered.
			bool unexpected_eof()
			{
				return error("Unexpected EOF");
			}

      /// Called to select the contents of a string for the current token.
      bool select_string()
      {
        currentToken.type = TokenType::kString;
        swallow_char();

        // Skip the " in the selection
        char c;
        if (!next_char(c, false))
          return false;

        currentToken.selection.start = position;
        currentToken.selection.startBlock = currentBlock;

        bool result;
        while ((result = next_char(c, false)) && c != '\"')
          swallow_char();

        currentToken.selection.end = position;
        currentToken.selection.endBlock = currentBlock;

        // If the last character was a " skip it
        if (result)
          swallow_char();

        if (!result)
          return unexpected_eof();

        return result;
      }

      /// Called to move the cursor by one character. Also counts columns and line numbers.
      /// 
      void swallow_char()
      {
        char c = currentBlock->data[position];
        if (c == '\n')
        {
          line++;
          column = 0;
        }
        else if (!std::iscntrl(c))
          column++;

        ++position;
      }

    private:
			IJsonParserLog *log;
			IJsonParserSource *source;
      JsonDocumentType documentType;

			uint32_t line;
			uint32_t column;

			ParseBlock *firstEmptyBlock;

			ParseBlock *currentBlock;
			uint32_t position;

			Token currentToken;
		};

		//-----------------------------------------------------------------------------------------------
		bool parse_value(ParseContext &context, std::unique_ptr<JsonValue> &value);

		//-----------------------------------------------------------------------------------------------
		template<TokenType ... Args>
		std::string token_type_concatenated_list()
		{
      std::initializer_list<TokenType> elements = { Args... };

      std::string result;
      uint32_t size = static_cast<uint32_t>(elements.size());
      uint32_t i = 0;
      for (auto it = elements.begin(); it != elements.begin(); ++it, ++i)
      {
        if (i == 0)
          result += token_to_string(*it);
        else if (i == size - 1)
          result += std::string(" or ") + token_to_string(*it);
        else
          result += std::string(", ") + token_to_string(*it);
      }

      return result;
		}

		//-----------------------------------------------------------------------------------------------
		template<TokenType ... Args>
		bool unexpected_token(ParseContext &context)
		{
      return context.error("Unexpected %s, expected %s", context.token().type, token_type_concatenated_list<Args...>().c_str());
		}
    
    //-----------------------------------------------------------------------------------------------
    template<TokenType ... T>
    bool expect(ParseContext &context, bool skip = true)
    {
      std::initializer_list<TokenType> items = { T... };

      if (std::find(items.begin(), items.end(), context.token().type) == items.end())
        return unexpected_token<T...>(context);

      if (skip) context.next();
      return true;
    }
    
    //-----------------------------------------------------------------------------------------------
    bool parse_array(ParseContext &context, std::unique_ptr<JsonValue> &value)
    {
      if (!expect<TokenType::kBraceLeft>(context))
        return false;

      std::unique_ptr<JsonArray> arr(new JsonArray);

      while (context.token().type != TokenType::kBraceRight)
      {
        // Parse a new value
        std::unique_ptr<JsonValue> value;
        if (!parse_value(context, value))
          return false;

        // Insert into the array
        arr->emplace_back(std::move(value));

        // Must be a comma present in normal json
        if (context.document_type() == JsonDocumentType::kNormal &&
          context.token().type != TokenType::kComma &&
          context.token().type != TokenType::kBraceRight)
          return unexpected_token<TokenType::kComma, TokenType::kBraceRight>(context);
        
        // Skip comma if present
        if (context.token().type == TokenType::kComma)
          context.next();
      }

      if(!expect<TokenType::kBraceRight>(context))
          return false;

      value = std::move(arr);
      return true;
    }

    //-----------------------------------------------------------------------------------------------
    bool parse_object(ParseContext &context, std::unique_ptr<JsonValue> &value, bool root = false)
    {
      if ((context.document_type() == JsonDocumentType::kNormal || !root) &&
        !expect<TokenType::kCurlyLeft>(context))
        return false;       

      std::unique_ptr<JsonObject> object(new JsonObject);

      while (!(context.token().type == TokenType::kCurlyRight ||
              (root && context.token().type == TokenType::kEOF)))
      {
        if ((context.document_type() == JsonDocumentType::kNormal && !expect<TokenType::kString>(context, false)) ||
          (context.document_type() == JsonDocumentType::kSimplified && !expect<TokenType::kString, TokenType::kIdentifier>(context, false)))
          return false;

        // Get the key text
        std::string key(context.token().selection.data());

        // Skip the key
        context.next();

        // Seperator is next
        if (!expect<TokenType::kSeperator>(context))
          return false;

        // Next up is a value
        std::unique_ptr<JsonValue> value;
        if (!parse_value(context, value))
          return false;

        // Insert into the object
        object->insert(key, std::move(value));

        // Must be a comma present in normal json
        if (context.document_type() == JsonDocumentType::kNormal &&
            context.token().type != TokenType::kComma && 
            context.token().type != TokenType::kCurlyRight)
            return unexpected_token<TokenType::kComma, TokenType::kCurlyRight>(context);

        // Skip comma if present
        if (context.token().type == TokenType::kComma)
          context.next();
      }
      
      if ((context.document_type() == JsonDocumentType::kNormal || !root)
        && !expect<TokenType::kCurlyRight>(context))
        return false;

      value = std::move(object);
      return true;
    }

		//-----------------------------------------------------------------------------------------------
		bool parse_document_root(ParseContext &context, std::unique_ptr<JsonValue> &document)
		{
			// Parse the first token
      context.next();

      // Determine content type
      if (context.document_type() == JsonDocumentType::kUnknown)
      {
        if (context.token().type == TokenType::kCurlyLeft ||
          context.token().type == TokenType::kBraceLeft)
          context.set_document_type(JsonDocumentType::kNormal);
        else
          context.set_document_type(JsonDocumentType::kSimplified);
      }

			// Parse the root object
      if (context.document_type() == JsonDocumentType::kSimplified)
				return parse_object(context, document, true);
			
      // Normal json then
			if(context.token().type == TokenType::kCurlyLeft)
				return parse_object(context, document);
			else if(context.token().type == TokenType::kBraceLeft)
				return parse_array(context, document);
			else
        return unexpected_token<TokenType::kCurlyLeft, TokenType::kBraceLeft>(context);
		}

    //-----------------------------------------------------------------------------------------------
    bool parse_value(ParseContext &context, std::unique_ptr<JsonValue> &value)
    {
      switch (context.token().type)
      {
      case TokenType::kBraceLeft: 
        return parse_array(context, value);
      case TokenType::kCurlyLeft: 
        return parse_object(context, value);
      case TokenType::kNumber:
      {
        double result;
        std::ios::iostate state;
        std::string data(context.token().selection.data());
        std::use_facet<std::num_get<char, std::string::iterator> >(std::locale::classic()).get
          (data.begin(), data.end(), std::cin, state, result);
        value.reset(new JsonNumber(result));
        context.next();
        return true;
      }
      case TokenType::kString:
      {
        value.reset(new JsonString(context.token().selection.data()));
        context.next(); 
        return true;
      }
      case TokenType::kTrue:
      {
        value.reset(new JsonBoolean(true));
        context.next();
        return true;
      }
      case TokenType::kFalse:
      {
        value.reset(new JsonBoolean(false));
        context.next();
        return true;
      }
      case TokenType::kNull:
      {
        value.reset(new JsonNull());
        context.next();
        return true;
      }
      }

      return false;
    }
	}

	//-----------------------------------------------------------------------------------------------
	bool parse_json(IJsonParserSource *source, std::unique_ptr<JsonValue> &document, IJsonParserLog *log,
		JsonDocumentType documentType)
	{
    ParseContext context(log, source, documentType);
		return parse_document_root(context, document);
	}
}


//-----------------------------------------------------------------------------------------------
int main()
{
	using namespace knowson;

  std::ifstream file("test.json");
  if (!file.is_open())
    return 1;

	StreamJsonParserSource<std::ifstream> reader(file);
	std::unique_ptr<JsonValue> value;
	bool result = parse_json(&reader, value);

	return 0;
}