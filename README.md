# JSONC

A lightweight, fast and simple JSON parser written in C.

# Build from source.

I've personally set up this project to be built with CMake (version 3.13 or higher). But you don't necessarily need
CMake for since `json.c` is a self contained library. To build with CMake, follow the build instructions here.

The build script and compiler I used are `ninja` and `clang`, but you can use any other build tool you like.

```
mkdir bin
cd bin
cmake -G Ninja -DCMAKE_C_COMPILER=clang
```

That will generate the build files in the newly created `bin` directory.
To run compile and run the project, simply invoke the build script. In this case,
`ninja`. Now run the tests by simply entering `./json` into the terminal.

# Usage

Here is a simple C program to parse a JSON string representing an array:

```cpp
#include "json.h"

int main() {
    JSONValue v = JSONParseString("[1, true, false, null, 45, {\"foo\": 12.3}]");
    JSONValuePrint(v); // [1, true, false, null, 45, [JSON Object]]
    return 0;
}

```

Parsing JSON files is just as easy:

```cpp
const JSONValue myJson = JSONParseFile("path/to/file.json");
```

# Implementation Details

JSONC uses the following struct to repesent all JSON values:

```cpp
struct sJSONValue {
  JSONValueType tag;
  union {
    JSONString string;
    int integer;
    double json_double;
    bool boolean;
    JSONArray* array;
    JsonObj* obj;
  } as;
};
```

The struct contains two data members, a type `tag` representing the data-type of the value, and a union containing
the possible value fields. `tag` is a member of the following enum:

```cpp
typedef enum {
  JSON_INTEGER,
  JSON_DOUBLE,
  JSON_STRING,
  JSON_ARRAY,
  JSON_OBJECT,
  JSON_BOOL,
  JSON_NULL
} JSONValueType;
```

There are a couple of helper macros to ease the process of creating values.
e.g - use `JSON_NEW_STRING("foo")` to create a value of the shape `((JSONValue){JSON_STRING, {.string = "foo"}})`.

JSON Objects (key-value pairs) are represented using linked lists, wherein each node of the
linked list is a an item containing a `key` and a `value`. A hashtable was avoided to keep this implementation
simple and straightforward.

```cpp
struct sJSONObject {
  JsonObj* next;
  JsonObj* prev;
  JSONString key;
  JSONValue value;
};
```

note that `JSONString` is just a typedef for `const char *`.

Arrays are simply a wrapper around a pointer to heap allocated series of JSONValues.
a `JSONArray` implements a dynamic array data structure.

```cpp
typedef struct sJSONArray JSONArray;

struct sJSONArray {
  JSONValue* values; // pointer to the first value stored in this array.
  size_t count; // number of items currently present in the array
  size_t capacity; // total capacity of this array.
};
```

The function `JSONArrayPush(JSONArray* array, JSONValue v)` adds values to the array
and `JSONArrayFree(JSONArray* array)` is used to free the array after use.
