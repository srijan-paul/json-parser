#include "json.h"
#include <assert.h>
#include <ctype.h>
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

#define ALLOCATE(type, size) ((type*)(malloc(sizeof(type*) * (size))))

void printValue(JSONValue value) {
  switch (value.tag) {
  case JSON_DOUBLE: printf("%f", JSON_AS_DOUBLE(value)); break;
  case JSON_BOOL: printf("%s", JSON_AS_BOOL(value) ? "true" : "false"); break;
  case JSON_INTEGER: printf("%d", JSON_AS_INT(value)); break;
  case JSON_NULL: printf("null");
  case JSON_STRING: printf("\"%s\"", JSON_AS_STRING(value)); break;
  case JSON_OBJECT: printf("[JSON Object]"); break;
  case JSON_ARRAY: {
    JSONArray* array = JSON_AS_ARRAY(value);
    printf("[");
    for (int i = 0; i < array->count; i++) {
      printValue(array->values[i]);
      if (i != array->count) printf(", ");
    }
    printf("]");
  }
  }
}

static JsonObj* allocateObj() {
  JsonObj* obj = ALLOCATE(JsonObj, 1);
  obj->next = NULL;
  obj->prev = NULL;
  return obj;
}

static void JSONObjFree(JsonObj* obj) {
  JsonObj* current = obj;
  while (current != NULL) {
    freeValue(current->value);
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
    array->values = realloc(array->values, sizeof(JSONValue) * array->capacity);
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

void freeValue(JSONValue value) {
  switch (value.tag) {
  case JSON_ARRAY: JSONArrayFree(JSON_AS_ARRAY(value)); break;
  case JSON_OBJECT: JSONObjFree(JSON_AS_OBJECT(value)); break;
  default: break;
  }
}

// --- JSONParser ---

void JSONParserInit(JSONParser* parser, const char* source) {
  parser->source = source;
  JSONTokenizerInit(&parser->tokenizer, source);
  parser->lookahead = JSONScanToken(&parser->tokenizer);
}

static void error(JSONParser* parser, const char* message) {
  printf("Error at line %zu: %s\n", parser->tokenizer.line, message);
}

static void advance(JSONParser* parser) {
  parser->current = parser->lookahead;
  parser->lookahead = JSONScanToken(&parser->tokenizer);
}

static inline bool check(JSONParser* parser, JSONTokenType type) {
  return (parser->lookahead.type == type);
}

static bool expect(JSONParser* parser, JSONTokenType type, const char* errMsg) {
  if (parser->lookahead.type == type) {
    advance(parser);
    return true;
  }
  error(parser, errMsg);
  return false;
}

static bool match(JSONParser* parser, JSONTokenType type) {
  if (check(parser, type)) {
    advance(parser);
    return true;
  }
  return false;
}

static char* parseString(JSONParser* parser) {
  char* raw = ALLOCATE(char, parser->current.length - 2);
  memcpy(raw, parser->current.start + 1, parser->current.length - 2);
  raw[parser->current.length - 2] = '\0';
  return raw;
}

static JSONArray* parseArray(JSONParser* parser);
static JsonObj* parseObject(JSONParser* parser);

static JSONValue parseValue(JSONParser* parser) {
  advance(parser);
  switch (parser->current.type) {
  case JSON_TOKEN_DOUBLE: {
    char* raw = ALLOCATE(char, parser->current.length + 1);
    raw[parser->current.length] = '\0';
    memcpy(raw, parser->current.start, parser->current.length);
    return JSON_NEW_DOUBLE(strtod(raw, NULL));
  }
  case JSON_TOKEN_STRING: return JSON_NEW_STRING(parseString(parser));
  case JSON_TOKEN_TRUE: return JSON_NEW_BOOL(true);
  case JSON_TOKEN_FALSE: return JSON_NEW_BOOL(false);
  case JSON_TOKEN_NULL: return JSON_VAL_NULL;
  case JSON_TOKEN_LSQBRACE: return JSON_NEW_ARRAY(parseArray(parser));
  case JSON_TOKEN_LBRAC: return JSON_NEW_OBJECT(parseObject(parser));
  default: error(parser, "unexpected JSON Value."); return JSON_VAL_NULL;
  }
}

static JsonObj* parseObject(JSONParser* parser) {
  JsonObj* objHead = allocateObj();
  JsonObj* prev = NULL;
  JsonObj* current = objHead;

  while (!parser->tokenizer.eof && !check(parser, JSON_TOKEN_RBRAC)) {
    expect(parser, JSON_TOKEN_STRING, "Expected string literal as object key.");

    current->key = parseString(parser);
    expect(parser, JSON_TOKEN_COLON, "Expected ':'.");
    current->value = parseValue(parser);

    current->prev = prev;
    prev = current;
    if (check(parser, JSON_TOKEN_RBRAC)) break;

    expect(parser, JSON_TOKEN_COMMA, "Expected ',' after key-value pair.");

    current->next = allocateObj();
    current = current->next;
  }
  expect(parser, JSON_TOKEN_RBRAC, "Expected '}' for closing object literal");
  return objHead;
}

JSONValue JSONParse(JSONParser* parser) {
  return parseValue(parser);
}

static JSONArray* parseArray(JSONParser* parser) {
  JSONArray* array = allocateArray();
  while (!parser->tokenizer.eof && !check(parser, JSON_TOKEN_RSQBRACE)) {
    JSONArrayPush(array, parseValue(parser));
    if (!match(parser, JSON_TOKEN_COMMA)) break;
  }
  expect(parser, JSON_TOKEN_RSQBRACE, "Expected ']' at array end.");
  return array;
}

/// Tests
void compare_tokens(const char* code, JSONTokenType* expected, size_t size) {
  JSONTokenizer t;
  JSONTokenizerInit(&t, code);

  for (int i = 0; i < size; i++) {
    JSONToken token = JSONScanToken(&t);
    if (expected[i] != token.type) {
      printf("Expected Syntax kind %d but got %d", expected[i], token.type);
      break;
    }
    assert(expected[i] == token.type);
    if (t.eof) break;
  }
}

void tokenizer_test() {
  JSONTokenType expect[] = {JSON_TOKEN_LBRAC, JSON_TOKEN_DOUBLE,
                            JSON_TOKEN_RBRAC, JSON_TOKEN_EOF};
  compare_tokens("{123.12}", expect, 4);

  JSONTokenType s_expected[] = {
      JSON_TOKEN_LBRAC, JSON_TOKEN_STRING, JSON_TOKEN_COLON,
      JSON_TOKEN_TRUE,  JSON_TOKEN_RBRAC,  JSON_TOKEN_EOF,
  };

  compare_tokens("{ \"foo\": true }", s_expected, 6);
}

static void printObject(const JsonObj* const object) {
  const JsonObj* current = object;
  while (current != NULL) {
    printf("%s", current->key);
    printf(": ");
    printValue(current->value);
    printf(", \n");
    current = current->next;
  }
}

static void parser_test() {
  JSONParser parser;
  JSONParserInit(&parser, "{\"string-key\": \"trueee\"}");
  JSONValue obj = JSONParse(&parser);
  printObject(JSON_AS_OBJECT(obj));
}

int main() {
  tokenizer_test();
  parser_test();
  return 0;
}