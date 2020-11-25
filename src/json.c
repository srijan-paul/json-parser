#include "json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void printValue(JSONValue value) {
  switch (value.tag) {
  case JSON_DOUBLE: printf("%f", JSON_AS_DOUBLE(value)); break;
  case JSON_BOOL: printf("%s", JSON_AS_BOOL(value) ? "true" : "false"); break;
  case JSON_INTEGER: printf("%d", JSON_AS_INT(value)); break;
  case JSON_NULL: printf("null");
  case JSON_STRING: printf("%s", JSON_AS_STRING(value)); break;
  }
}

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

JSONToken JSONScanToken(JSONTokenizer* tokenizer) {
  if (IS_EOF()) {
    tokenizer->eof = true;
    return makeToken(tokenizer, JSON_TOKEN_EOF);
  }

  skipWhiteSpace(tokenizer);
  tokenizer->lexeme_begin = tokenizer->current_pos;

  const char c = tokenizer->source[tokenizer->current_pos++];

  if (c == '{') {
    return TOKEN(LBRAC);
  } else if (c == '}') {
    return TOKEN(RBRAC);
  } else if (c == '[') {
    return TOKEN(LSQBRACE);
  } else if (c == ']') {
    return TOKEN(RSQBRACE);
  } else if (c == ':') {
    return TOKEN(COLON);
  } else if (c == ',') {
    return TOKEN(COMMA);
  } else if (isdigit(c)) { // integer or double
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

  return TOKEN(ERROR);
}

#undef TOKEN
#undef ADVANCE
#undef PEEK
#undef IS_EOF

#define ALLOCATE(type, size) ((type*)(malloc(sizeof(type*) * (size))))

static JsonObj* allocateObj() {
  JsonObj* obj = ALLOCATE(JsonObj, 1);
  obj->next = NULL;
  obj->prev = NULL;
  return obj;
}

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
  char* key = (char*)malloc(sizeof(char*) * parser->current.length);
  memcpy(key, parser->current.start + 1, parser->current.length - 2);
  return key;
}

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
  default: error(parser, "unexpected JSON Value."); return JSON_VAL_NULL;
  }
}

static JsonObj* parseObject(JSONParser* parser) {
  expect(parser, JSON_TOKEN_LBRAC, "Expected '{'.");

  JsonObj* objHead = allocateObj();
  JsonObj* current = objHead;
  while (!parser->tokenizer.eof && !check(parser, JSON_TOKEN_RBRAC)) {
    expect(parser, JSON_TOKEN_STRING, "Expected string literal as object key.");

    char* key = ALLOCATE(char, parser->current.length - 1);
    memcpy(key, parser->current.start + 1, parser->current.length - 2);
    key[parser->current.length - 2] = '\0';

    expect(parser, JSON_TOKEN_COLON, "Expected ':'.");

    current->key = key;
    current->value = parseValue(parser);

    if (check(parser, JSON_TOKEN_RBRAC)) break;

    expect(parser, JSON_TOKEN_COMMA, "Expected ',' after key-value pair.");
    current->next = allocateObj();
    current = current->next;
  }
  expect(parser, JSON_TOKEN_RBRAC, "Expected '}' for closing object literal");
  return objHead;
}

JsonObj* JSONParse(JSONParser* parser) {
  return parseObject(parser);
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

    if (t.eof) break;
  }
}

void tokenizer_test() {
  JSONTokenType expect[] = {JSON_TOKEN_LBRAC, JSON_TOKEN_DOUBLE,
                            JSON_TOKEN_RBRAC, JSON_TOKEN_EOF};
  compare_tokens("{123.12}", expect, 4);

  JSONTokenType s_expected[] = {
      JSON_TOKEN_LBRAC,  JSON_TOKEN_STRING, JSON_TOKEN_COLON,
      JSON_TOKEN_DOUBLE, JSON_TOKEN_RBRAC,  JSON_TOKEN_EOF,
  };

  compare_tokens("{ \"foo\": 12.5 }", s_expected, 6);
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
  JSONParserInit(&parser, "{\"foo\": 123.5, \"bar\": 456.12}");
  JsonObj* obj = JSONParse(&parser);
  printObject(obj);
}

int main() {
  tokenizer_test();
  parser_test();
  return 0;
}