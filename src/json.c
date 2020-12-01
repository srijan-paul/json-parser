#include "json.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void JSONTokenizerInit(JSONTokenizer* tokenizer, const char* source) {
  tokenizer->source = source;
  tokenizer->lexeme_begin = 0;
  tokenizer->current_pos = 0;
  tokenizer->line = 1;
  tokenizer->eof = false;
}

static JSONToken makeToken(JSONTokenizer* tokenizer, JSONTokenType type) {
  JSONToken token;
  token.start = tokenizer->source + tokenizer->lexeme_begin;
  token.length = tokenizer->current_pos - tokenizer->lexeme_begin;
  token.type = type;
  return token;
}

#define TOKEN(t) makeToken(tokenizer, JSON_TOKEN_##t)
#define STATE(s) JSON_STATE_##s
#define MATCH(c)                                                               \
  ((tokenizer->source[tokenizer->current_pos] == c)                            \
       ? (tokenizer->current_pos++, true)                                      \
       : false)
#define PEEK      (tokenizer->source[tokenizer->current_pos])
#define ADVANCE() (++tokenizer->current_pos)
#define IS_EOF()  (tokenizer->source[tokenizer->current_pos] == '\0')

static void skipWhiteSpace(JSONTokenizer* tokenizer) {
  while (true) {
    switch (PEEK) {
    case '\t':
    case ' ':
    case '\r': ADVANCE(); break;
    case '\n':
      ADVANCE();
      tokenizer->line++;
      break;
    default: return;
    }
  }
}

static bool matchKeyword(JSONTokenizer* tokenizer, const char* kw) {
  while (isalpha(PEEK)) ADVANCE();

  for (int i = 0; kw[i]; i++) {
    if (kw[i] != tokenizer->source[tokenizer->lexeme_begin + i]) return false;
  }
  return true;
}

JSONToken JSONScanToken(JSONTokenizer* tokenizer) {
  if (IS_EOF()) {
    tokenizer->eof = true;
    return makeToken(tokenizer, JSON_TOKEN_EOF);
  }

  skipWhiteSpace(tokenizer);
  tokenizer->lexeme_begin = tokenizer->current_pos;

  const char c = tokenizer->source[tokenizer->current_pos++];
  switch (c) {
  case '{': return TOKEN(LBRAC);
  case '}': return TOKEN(RBRAC);
  case '[': return TOKEN(LSQBRACE);
  case ']': return TOKEN(RSQBRACE);
  case ':': return TOKEN(COLON);
  case ',': return TOKEN(COMMA);
  case 't':
    if (matchKeyword(tokenizer, "true")) return TOKEN(TRUE);
    return TOKEN(ERROR);
  case 'f':
    if (matchKeyword(tokenizer, "false")) return TOKEN(FALSE);
    return TOKEN(ERROR);
  case 'n':
    if (matchKeyword(tokenizer, "null")) return TOKEN(NULL);
    return TOKEN(ERROR);
  default:
    if (isdigit(c)) { // integer or double
      JSONTokenType type = JSON_TOKEN_INTEGER;

      while (isdigit(PEEK)) ADVANCE();

      if (MATCH('.')) {
        type = JSON_TOKEN_DOUBLE;
        while (isdigit(PEEK)) ADVANCE();
      }
      return makeToken(tokenizer, type);
    } else if (c == '"') {
      // TODO: escape chars
      while (!(IS_EOF() || PEEK == '"')) {
        ADVANCE();
      }
      if (PEEK == '"') ADVANCE();
      return TOKEN(STRING);
      // TODO: unterminated string literal error.
    }
  }
  return TOKEN(ERROR);
}

#undef TOKEN
#undef ADVANCE
#undef PEEK
#undef IS_EOF

// --- JSONValue ---

#define ALLOCATE(type, count) ((type*)(malloc(sizeof(type) * (count))))
#define REALLOC(block, type, count)                                            \
  ((type*)realloc(block, sizeof(type) * count))

void JSONValuePrint(JSONValue value) {
  switch (value.tag) {
  case JSON_DOUBLE: printf("%f", JSON_AS_DOUBLE(value)); break;
  case JSON_BOOL: printf("%s", JSON_AS_BOOL(value) ? "true" : "false"); break;
  case JSON_INTEGER: printf("%d", JSON_AS_INT(value)); break;
  case JSON_NULL: printf("null"); break;
  case JSON_STRING: printf("\"%s\"", JSON_AS_STRING(value)); break;
  case JSON_OBJECT: printf("[JSON Object]"); break;
  case JSON_ARRAY: {
    JSONArray* array = JSON_AS_ARRAY(value);
    printf("[");
    for (int i = 0; i < array->count; i++) {
      JSONValuePrint(array->values[i]);
      if (i < array->count - 1) printf(", ");
    }
    printf("]");
  } break;
  }
}

static JSONObject* allocateObj() {
  JSONObject* obj = ALLOCATE(JSONObject, 1);
  obj->next = NULL;
  obj->prev = NULL;
  return obj;
}

static void JSONObjectFree(JSONObject* obj) {
  JSONObject* current = obj;
  while (current != NULL) {
    JSONValueFree(current->value);
    current = current->next;
  }
}

void JSONArrayInit(JSONArray* array) {
  array->capacity = JSON_ARRAY_DEFAULT_SIZE;
  array->count = 0;
  array->values = ALLOCATE(JSONValue, JSON_ARRAY_DEFAULT_SIZE);
}

static JSONArray* allocateArray() {
  JSONArray* array = ALLOCATE(JSONArray, 1);
  JSONArrayInit(array);
  return array;
}

static void ensureCapacity(JSONArray* array) {
  if (array->count >= array->capacity) {
    array->capacity *= 2;
    array->values = REALLOC(array->values, JSONValue, array->capacity);
  }
}

void JSONArrayPush(JSONArray* array, JSONValue value) {
  ensureCapacity(array);
  array->values[array->count++] = value;
}

void JSONArrayFree(JSONArray* array) {
  free(array->values);
  array->count = 0;
  array->capacity = 0;
}

void JSONValueFree(JSONValue value) {
  switch (value.tag) {
  case JSON_ARRAY: JSONArrayFree(JSON_AS_ARRAY(value)); break;
  case JSON_OBJECT: JSONObjectFree(JSON_AS_OBJECT(value)); break;
  default: break;
  }
}

// --- JSONParser ---

void JSONParserInit(JSONParser* parser, const char* source) {
  parser->source = source;
  JSONTokenizerInit(&parser->tokenizer, source);
  parser->lookahead = JSONScanToken(&parser->tokenizer);
  parser->hasError = false;
  parser->errMsg = NULL;
}

static void error(JSONParser* parser) {
  parser->hasError = true;
}

static void advance(JSONParser* parser) {
  parser->current = parser->lookahead;
  parser->lookahead = JSONScanToken(&parser->tokenizer);
}

static inline bool check(JSONParser* parser, JSONTokenType type) {
  return (parser->lookahead.type == type);
}

static bool expect(JSONParser* parser, JSONTokenType type) {
  if (parser->lookahead.type == type) {
    advance(parser);
    return true;
  }
  error(parser);
  return false;
}

static bool match(JSONParser* parser, JSONTokenType type) {
  if (check(parser, type)) {
    advance(parser);
    return true;
  }
  return false;
}

static char* tok2String(JSONParser* parser) {
  char* raw = ALLOCATE(char, parser->current.length + 1);
  raw[parser->current.length] = '\0';
  memcpy(raw, parser->current.start, parser->current.length);
  return raw;
}

static char* parseString(JSONParser* parser) {
  char* raw = ALLOCATE(char, parser->current.length - 2);
  memcpy(raw, parser->current.start + 1, parser->current.length - 2);
  raw[parser->current.length - 2] = '\0';
  return raw;
}

static JSONArray* parseArray(JSONParser* parser);
static JSONObject* parseObject(JSONParser* parser);

static JSONValue parseValue(JSONParser* parser) {
  advance(parser);
  switch (parser->current.type) {
  case JSON_TOKEN_DOUBLE: {
    char* raw = tok2String(parser);
    return JSON_NEW_DOUBLE(strtod(raw, NULL));
  }
  case JSON_TOKEN_INTEGER: {
    char* raw = tok2String(parser);
    return JSON_NEW_INT(strtol(raw, NULL, 10));
  }
  case JSON_TOKEN_STRING: return JSON_NEW_STRING(parseString(parser));
  case JSON_TOKEN_TRUE: return JSON_NEW_BOOL(true);
  case JSON_TOKEN_FALSE: return JSON_NEW_BOOL(false);
  case JSON_TOKEN_NULL: return JSON_VAL_NULL;
  case JSON_TOKEN_LSQBRACE: return JSON_NEW_ARRAY(parseArray(parser));
  case JSON_TOKEN_LBRAC: return JSON_NEW_OBJECT(parseObject(parser));
  default: error(parser); return JSON_VAL_NULL;
  }
}

static JSONObject* parseObject(JSONParser* parser) {
  JSONObject* objHead = allocateObj();
  JSONObject* prev = NULL;
  JSONObject* current = objHead;

  while (!parser->tokenizer.eof && !check(parser, JSON_TOKEN_RBRAC)) {
    expect(parser, JSON_TOKEN_STRING);

    current->key = parseString(parser);
    expect(parser, JSON_TOKEN_COLON);
    current->value = parseValue(parser);

    current->prev = prev;
    prev = current;
    if (check(parser, JSON_TOKEN_RBRAC)) break;

    expect(parser, JSON_TOKEN_COMMA);

    current->next = allocateObj();
    current = current->next;
  }
  expect(parser, JSON_TOKEN_RBRAC);
  return objHead;
}

static JSONArray* parseArray(JSONParser* parser) {
  JSONArray* array = allocateArray();
  while (!parser->tokenizer.eof && !check(parser, JSON_TOKEN_RSQBRACE)) {
    JSONArrayPush(array, parseValue(parser));
    if (!match(parser, JSON_TOKEN_COMMA)) break;
  }
  expect(parser, JSON_TOKEN_RSQBRACE);
  return array;
}

JSONCode JSONParse(JSONParser* parser, JSONValue* value) {
  *value = parseValue(parser);

  if (parser->hasError) {
    return JSON_ERROR_SYNTAX;
  }

  return JSON_ERROR_NONE;
}

JSONCode JSONParseString(const char* source, JSONValue* value) {
  JSONParser parser;
  JSONParserInit(&parser, source);
  return JSONParse(&parser, value);
}

static const char* readFile(const char* fileName) {
  FILE* file;
  fopen_s(&file, fileName, "r");

  if (file == NULL) {
    printf("JSONC: File '%s' not found.\n", fileName);
    return NULL;
  }

  fseek(file, 0, SEEK_END);
  size_t fileSize = ftell(file);
  rewind(file);

  char* buf = ALLOCATE(char, fileSize + 1);

  if (buf == NULL) {
    printf("JSONC: Not enough memory to read file '%s'.\n", fileName);
    return NULL;
  }

  size_t bytesRead = fread(buf, sizeof(char), fileSize, file);
  buf[bytesRead] = '\0';

  fclose(file);
  return buf;
}

JSONCode JSONParseFile(const char* fileName, JSONValue* value) {
  const char* source = readFile(fileName);
  if (source == NULL) {
    return JSON_ERROR_FILE_READ;
  }
  return JSONParseString(source, value);
}