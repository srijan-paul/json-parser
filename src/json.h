#ifndef JSON_H
#define JSON_H
#include <stdbool.h>
#include <stddef.h>

typedef enum {
  JSON_INTEGER,
  JSON_DOUBLE,
  JSON_STRING,
  JSON_ARRAY,
  JSON_OBJECT,
  JSON_BOOL,
  JSON_NULL
} JSONValueType;

typedef struct sJsonObj JsonObj;
typedef struct sJSONValue JSONValue;
typedef const char* JSONString;
typedef struct sJSONArray JSONArray;

struct sJSONValue {
  JSONValueType tag;
  union {
    JSONString string;
    int integer;
    double json_double;
    JSONArray* array;
    JsonObj* obj;
  } as;
};

#define JSON_IS_NUMERIC(o) ((o).tag == JSON_INTEGER || (o).tag == JSON_DOUBLE)
#define JSON_IS_DOUBLE(o)  ((o).tag == JSON_DOUBLE)
#define JSON_IS_INT(o)     ((o).tag == JSON_INTEGER)
#define JSON_IS_ARRAY(o)   ((o).tag == JSON_ARRAY)
#define JSON_IS_BOOL(o)    ((o).tag == JSON_BOOL)
#define JSON_IS_STRING(o)  ((o).tag == JSON_STRING)
#define JSON_IS_OBJECT(o)  ((o).tag == JSON_OBJECT)
#define JSON_IS_NULL(o)    ((o).tag == JSON_NULL)

#define JSON_AS_INT(v)    ((v).as.integer)
#define JSON_AS_STRING(v) ((v).as.string)

#define JSON_NEW_INT(n)    ((JSONValue){JSON_INTEGER, {.integer = n}})
#define JSON_NEW_STRING(s) ((JSONValue){JSON_INTEGER, {.string = s}})

struct sJSONArray {
  JSONValue* values;
  size_t count;
};

struct sJsonObj {
  JSONString key;
  JSONValue value;
};

// JSON Tokenizer that emits tokens.
// A Token is the smallest parseable unit of a Json file.
typedef enum {
  JSON_STATE_INTEGER,
  JSON_STATE_DOUBLE,
  JSON_STATE_STRING
} JSONTokenizerState;

typedef struct {
  const char* source;
  size_t current_pos;
  size_t lexeme_begin;
  JSONTokenizerState state;
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
  JSON_TOKEN_EOF,
  JSON_TOKEN_ERROR
} JSONTokenType;

typedef struct {
  JSONTokenType type;
  size_t start;
  size_t length;
} JSONToken;

JSONToken JSONScanToken(JSONTokenizer* tokenizer);
void JSONTokenizerInit(JSONTokenizer* tokenizer, const char* source);

JSONValue* JSONParse(const char* string);

#endif