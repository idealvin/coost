#pragma once

#include "rapidjson/allocators.h"
#include "rapidjson/document.h"
#include "rapidjson/rapidjson.h"
#include "rapidjson/reader.h"

#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "rapidjson/prettywriter.h"

namespace rapidjson {

inline std::string str(const Document& doc) {
    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    doc.Accept(writer);
    return buffer.GetString();
}

inline std::string pretty(const Document& doc) {
    StringBuffer buffer;
    PrettyWriter<StringBuffer> writer(buffer);
    doc.Accept(writer);
    return buffer.GetString();
}

inline bool parse(const char* s, Document& doc) {
    doc.Parse(s);
    return !doc.HasParseError();
}

inline bool parse(const std::string& s, Document& doc) {
    doc.Parse(s.data(), s.size());
    return !doc.HasParseError();
}

}
