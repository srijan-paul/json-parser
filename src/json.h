#ifndef JSON_H
#define JSON_H
#include <stdbool.h>
#include <stddef.h>

#define JSON_ARRAY_DEFAULT_SIZE 8

typedef enum {
  JSON_INTEGER,
  JSON_DOUBLE,
  JSON_STRING,
  JSON_ARRAY,
  JSON_OBJECT,
  JSON_BOOL,
  JSON_NULL
} JSONValueType;

typedef struct sJSONObject JSONObject;
typedef struct sJSONValue JSONValue;
typedef const char* JSONString;
typedef struct sJSONArray JSONArray;

struct sJSONValue {
  JSONValueType tag;
  union {
    JSONString string;
    int integer;
    double json_double;
    bool boolean;
    JSONArray* array;
    JSONObject* obj;
  } as;
};

void JSONValuePrint(JSONValue value);
void JSONValueFree(JSONValue value);

#define JSON_IS_NUMERIC(o) ((o).tag == JSON_INTEGER || (o).tag == JSON_DOUBLE)
#define JSON_IS_DOUBLE(o)  ((o).tag == JSON_DOUBLE)
#define JSON_IS_INT(o)     ((o).tag == JSON_INTEGER)
#define JSON_IS_ARRAY(o)   ((o).tag == JSON_ARRAY)
#define JSON_IS_BOOL(o)    ((o).tag == JSON_BOOL)
#define JSON_IS_STRING(o)  ((o).tag == JSON_STRING)
#define JSON_IS_OBJECT(o)  ((o).tag == JSON_OBJECT)
#define JSON_IS_NULL(o)    ((o).tag == JSON_NULL)

#define JSON_AS_INT(v)    ((v).as.integer)
#define JSON_AS_DOUBLE(v) ((v).as.json_double)
#define JSON_AS_BOOL(v)   ((v).as.boolean)
#define JSON_AS_STRING(v) ((v).as.string)
#define JSON_AS_ARRAY(v)  ((v).as.array)
#define JSON_AS_OBJECT(v) ((v).as.obj)

#define JSON_NEW_INT(n)    ((JSONValue){JSON_INTEGER, {.integer = n}})
#define JSON_NEW_DOUBLE(n) ((JSONValue){JSON_DOUBLE, {.json_double = n}})
#define JSON_NEW_STRING(s) ((JSONValue){JSON_STRING, {.string = s}})
#define JSON_VAL_NULL      ((JSONValue){JSON_NULL, {.integer = 0}})
#define JSON_NEW_BOOL(v)   ((JSONValue){JSON_BOOL, {.boolean = v}})
#define JSON_NEW_OBJECT(v) ((JSONValue){JSON_OBJECT, {.obj = v}})
#define JSON_NEW_ARRAY(v)  ((JSONValue){JSON_ARRAY, {.array = v}})

struct sJSONArray {
  JSONValue* values;
  size_t count;
  size_t capacity;
};

void JSONArrayInit(JSONArray* array);
void JSONArrayPush(JSONArray* array, JSONValue value);
void JSONArrayFree(JSONArray* array);

struct sJSONObject {
  JSONObject* next;
  JSONObject* prev;
  JSONString key;
  JSONValue value;
};

// JSON Tokenizer that emits tokens.
// A Token is the smallest parseable unit of a Json file.

typedef struct {
  const char* source;
  size_t current_pos;
  size_t lexeme_begin;
  size_t line;
  bool eof;
} JSONTokenizer;

typedef enum {
  JSON_TOKEN_STRING,   // ".*"
  JSON_TOKEN_LSQBRACE, // [
  JSON_TOKEN_RSQBRACE, // ]
  JSON_TOKEN_TRUE,     // true
  JSON_TOKEN_FALSE,    // false
  JSON_TOKEN_INTEGER,  // \d+
  JSON_TOKEN_DOUBLE,   //\d+(.\d+)
  JSON_TOKEN_LBRAC,    // {
  JSON_TOKEN_RBRAC,    // }
  JSON_TOKEN_COLON,    // :
  JSON_TOKEN_COMMA,    //,
  JSON_TOKEN_NULL,     // null
  JSON_TOKEN_EOF,
  JSON_TOKEN_ERROR
} JSONTokenType;

typedef struct {
  JSONTokenType type;
  const char* start;
  size_t length;
} JSONToken;

JSONToken JSONScanToken(JSONTokenizer* tokenizer);
void JSONTokenizerInit(JSONTokenizer* tokenizer, const char* source);

typedef enum {
  JSON_ERROR_SYNTAX,
  JSON_ERROR_FILE_READ,
  JSON_SUCCESS,
} JSONCode;

typedef struct {
  JSONCode code;
  char* message;
} JSONStatus;

#define JSON_MAKE_STATUS(type, message) ((JSONStatus){type, message})
JSONStatus JSONStatusNew(JSONCode code, char* message);
void JSONStatusFree(JSONStatus* status);

typedef struct {
  const char* source;
  JSONTokenizer tokenizer;
  JSONToken current;
  JSONToken lookahead;
  bool hasError;
  char* errMsg;
} JSONParser;

void JSONParserInit(JSONParser* parser, const char* source);
JSONStatus JSONParse(JSONParser* parser, JSONValue* value);
JSONStatus JSONParseString(const char* source, JSONValue* value);
JSONStatus JSONParseFile(const char* fileName, JSONValue* value);

#endif