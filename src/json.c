#include "json.h"
#include <ctype.h>
#include <stdio.h>

void JSONTokenizerInit(JSONTokenizer* tokenizer, const char* source) {
  tokenizer->source = source;
  tokenizer->lexeme_begin = 0;
  tokenizer->current_pos = 0;
  tokenizer->eof = false;
}

static JSONToken makeToken(JSONTokenizer* tokenizer, JSONTokenType type) {
  JSONToken token;
  token.start = tokenizer->lexeme_begin;
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
#define PEEK      tokenizer->source[tokenizer->current_pos]
#define ADVANCE() ++tokenizer->current_pos;
#define IS_EOF()  tokenizer->source[tokenizer->current_pos] == '\0'

JSONToken JSONScanToken(JSONTokenizer* tokenizer) {
  if (IS_EOF()) {
    tokenizer->eof = true;
    return makeToken(tokenizer, JSON_TOKEN_EOF);
  }

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
  } else if (isdigit(c)) { // integer or double
    JSONTokenType type = JSON_TOKEN_INTEGER;

    while (isdigit(PEEK)) ADVANCE();

    if (MATCH('.')) {
      type = JSON_TOKEN_DOUBLE;
      while (isdigit(PEEK)) ADVANCE();
    }
    return makeToken(tokenizer, type);
  }

  return TOKEN(ERROR);
}

#undef TOKEN

int main() {
  JSONTokenizer t;
  JSONTokenizerInit(&t, "{123.12}");
  while (!t.eof) printf("%d\t", JSONScanToken(&t).type);
  return 0;
}