#include "json.h"
#include <assert.h>
#include <stdio.h>

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

void tokenizerTest() {
  JSONTokenType expect[] = {JSON_TOKEN_LBRAC, JSON_TOKEN_DOUBLE,
                            JSON_TOKEN_RBRAC, JSON_TOKEN_EOF};
  compare_tokens("{123.12}", expect, 4);

  JSONTokenType sExpected[] = {
      JSON_TOKEN_LBRAC, JSON_TOKEN_STRING, JSON_TOKEN_COLON,
      JSON_TOKEN_TRUE,  JSON_TOKEN_RBRAC,  JSON_TOKEN_EOF,
  };

  compare_tokens("{ \"foo\": true }", sExpected, 6);
}

static void printObject(const JSONObject* const object) {
  const JSONObject* current = object;
  while (current != NULL) {
    printf("%s", current->key);
    printf(": ");
    JSONValuePrint(current->value);
    printf(", \n");
    current = current->next;
  }
}

static void parserTest() {
  JSONValue test = JSONParseFile("../test/1.json");
  printObject(JSON_AS_OBJECT(test));
  JSONValueFree(test);

  JSONValue obj = JSONParseString(
      "[1, [1, 2, \"Actually a string\"], {\"key\": [1, true]}]");
  JSONValuePrint(obj);
  JSONValueFree(obj);
}

int main() {
  tokenizerTest();
  parserTest();
  return 0;
}