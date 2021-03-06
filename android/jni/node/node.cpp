// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#include <strings.h>
#include <assert.h>
#include "node.h"

using namespace v8;

namespace node {

void Run(const char* jsString, int len, const char* origin) {
  HandleScope scope;

  // Allocates a new string from either utf-8 encoded or ascii data.
  // The second parameter 'length' gives the buffer length.
  // If the data is utf-8 encoded, the caller must be careful to supply the
  // length parameter. If it is not given, the function calls 'strlen' to
  // determine the buffer length, it might be wrong if 'data' contains a null
  // character.
  Handle<String> source = String::New(jsString, len);

  Handle<Script> script;
  if (origin == NULL)
    script = Script::Compile(source);
  else
    script = Script::Compile(source, String::New(origin));
  script->Run();
}

void Run(unsigned char* jsBytes, int len, const char* origin) {
  Run((char*) jsBytes, len, origin);
}

Handle<Script> Compile(unsigned char* jsBytes, int len, const char* origin) {
  HandleScope scope;

  // Allocates a new string from either utf-8 encoded or ascii data.
  // The second parameter 'length' gives the buffer length.
  // If the data is utf-8 encoded, the caller must be careful to supply the
  // length parameter. If it is not given, the function calls 'strlen' to
  // determine the buffer length, it might be wrong if 'data' contains a null
  // character.
  Handle<String> source = String::New((char*) jsBytes, len);

  Handle<Script> script;
  if (origin == NULL)
    script = Script::Compile(source);
  else
    script = Script::Compile(source, String::New(origin));
  return scope.Close(script);
}

enum encoding ParseEncoding(Handle<Value> encoding_v, enum encoding _default) {
  HandleScope scope;

  if (!encoding_v->IsString()) return _default;

  String::Utf8Value encoding(encoding_v);

  if (strcasecmp(*encoding, "utf8") == 0) {
    return UTF8;
  } else if (strcasecmp(*encoding, "utf-8") == 0) {
    return UTF8;
  } else if (strcasecmp(*encoding, "ascii") == 0) {
    return ASCII;
  } else if (strcasecmp(*encoding, "base64") == 0) {
    return BASE64;
  } else if (strcasecmp(*encoding, "ucs2") == 0) {
    return UCS2;
  } else if (strcasecmp(*encoding, "ucs-2") == 0) {
    return UCS2;
  } else if (strcasecmp(*encoding, "binary") == 0) {
    return BINARY;
  } else if (strcasecmp(*encoding, "hex") == 0) {
    return HEX;
  } else if (strcasecmp(*encoding, "raw") == 0) {
    fprintf(stderr, "'raw' (array of integers) has been removed. "
                    "Use 'binary'.\n");
    return BINARY;
  } else if (strcasecmp(*encoding, "raws") == 0) {
    fprintf(stderr, "'raws' encoding has been renamed to 'binary'. "
                    "Please update your code.\n");
    return BINARY;
  } else {
    return _default;
  }
}

Local<Value> Encode(const void *buf, size_t len, enum encoding encoding) {
  HandleScope scope;

  if (!len) return scope.Close(String::Empty());

  if (encoding == BINARY) {
    const unsigned char *cbuf = static_cast<const unsigned char*>(buf);
    uint16_t * twobytebuf = new uint16_t[len];
    for (size_t i = 0; i < len; i++) {
      // XXX is the following line platform independent?
      twobytebuf[i] = cbuf[i];
    }
    Local<String> chunk = String::New(twobytebuf, len);
    delete [] twobytebuf; // TODO use ExternalTwoByteString?
    return scope.Close(chunk);
  }

  // utf8 or ascii encoding
  Local<String> chunk = String::New((const char*)buf, len);
  return scope.Close(chunk);
}

// Returns number of bytes written.
ssize_t DecodeWrite(char *buf,
                    size_t buflen,
                    Handle<Value> val,
                    enum encoding encoding) {
  HandleScope scope;

  // XXX
  // A lot of improvement can be made here. See:
  // http://code.google.com/p/v8/issues/detail?id=270
  // http://groups.google.com/group/v8-dev/browse_thread/thread/dba28a81d9215291/ece2b50a3b4022c
  // http://groups.google.com/group/v8-users/browse_thread/thread/1f83b0ba1f0a611

  if (val->IsArray()) {
    fprintf(stderr, "'raw' encoding (array of integers) has been removed. "
                    "Use 'binary'.\n");
    assert(0);
    return -1;
  }

  Local<String> str = val->ToString();

  if (encoding == UTF8) {
    str->WriteUtf8(buf, buflen, NULL, String::HINT_MANY_WRITES_EXPECTED);
    return buflen;
  }

  if (encoding == ASCII) {
    str->WriteAscii(buf, 0, buflen, String::HINT_MANY_WRITES_EXPECTED);
    return buflen;
  }

  // THIS IS AWFUL!!! FIXME

  assert(encoding == BINARY);

  uint16_t * twobytebuf = new uint16_t[buflen];

  str->Write(twobytebuf, 0, buflen, String::HINT_MANY_WRITES_EXPECTED);

  for (size_t i = 0; i < buflen; i++) {
    unsigned char *b = reinterpret_cast<unsigned char*>(&twobytebuf[i]);
    buf[i] = b[0];
  }

  delete [] twobytebuf;

  return buflen;
}

Handle<Value> FromConstructorTemplate(Persistent<FunctionTemplate>& t,
                                      const Arguments& args) {
  HandleScope scope;

  const int argc = args.Length();
  Local<Value>* argv = new Local<Value>[argc];

  for (int i = 0; i < argc; ++i) {
    argv[i] = args[i];
  }

  Local<Object> instance = t->GetFunction()->NewInstance(argc, argv);

  delete[] argv;

  return scope.Close(instance);
}

Handle<Value> ThrowError(const char* msg) {
  return ThrowException(Exception::Error(String::New(msg)));
}

Handle<Value> ThrowTypeError(const char* msg) {
  return ThrowException(Exception::TypeError(String::New(msg)));
}

Handle<Value> ThrowRangeError(const char* msg) {
  return ThrowException(Exception::RangeError(String::New(msg)));
}

}  // namespace node
