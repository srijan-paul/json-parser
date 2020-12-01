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
  printf("--- Parser tests ---\n");

  JSONValue test;
  const char* filename = "../test/1.json";
  JSONStatus status = JSONParseFile(filename, &test);
  if (status.code == JSON_SUCCESS) {
    printObject(JSON_AS_OBJECT(test));
    JSONValueFree(test);
    printf("\n");
  } else {
    printf("Error on file test '%s': %s\n", filename, status.message);
  }

  JSONValue strtest;
  status = JSONParseString(
      "[1, [1, 2, \"Actually a string\"], {\"key\": [1, true]}]", &strtest);
  JSONValuePrint(strtest);
  printf("\n");
  JSONValueFree(strtest);

  printf("\n------------------\n");
}

static void arrayTest() {
  printf("--- Array tests ---\n");

  JSONArray array;
  JSONArrayInit(&array);
  JSONArrayPush(&array, JSON_NEW_BOOL(true));
  JSONArrayPush(&array, JSON_NEW_DOUBLE(13.5));
  for (int i = 0; i < 8; i++) {
    JSONArrayPush(&array, JSON_NEW_INT(i));
  }
  JSONValuePrint(JSON_NEW_ARRAY(&array));
  JSONValueFree(JSON_NEW_ARRAY(&array));

  printf("\n------------------\n");
}

void errorTest() {
  printf("--- Error and status test ---\n");
  const char* str = "{\"key\": 1";
  JSONValue v;
  JSONStatus status = JSONParseString(str, &v);
  printf("%s", status.message);
  printf("\n-----------------------------\n");
}

int main() {
  arrayTest();
  tokenizerTest();
  parserTest();
  errorTest();
  return 0;
}