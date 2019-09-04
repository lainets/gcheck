#include "json.h"
#include "user_object.h"
#include "gcheck.h"

namespace gcheck {

JSON JSONEscape(std::string str) {
    
    std::string escapees = "\\\n\t\b\f\r\"";
    std::vector<std::string> replacees{"\\\\", "\\n", "\\t", "\\b", "\\f", "\\r", "\\\""};
    size_t pos = 0;
    int index = 0;
    size_t min = std::string::npos;
    for(unsigned int i = 0; i < escapees.length(); i++) {
        size_t p = str.find(escapees[i], pos);
        if(p < min) {
            min = p;
            index = i;
        }
    }
    pos = min;
    while(pos != std::string::npos) {
        str.replace(pos, 1, replacees[index]);
        pos += replacees[index].length();
        
        index = 0;
        min = std::string::npos;
        for(unsigned int i = 0; i < escapees.length(); i++) {
            size_t p = str.find(escapees[i], pos);
            if(p < min) {
                min = p;
                index = i;
            }
        }
        pos = min;
    }
    
    return str;
}

JSON toJSON(const std::string& key, const JSON& value) {
    return "\"" + key + "\":" + value;
}

JSON toJSON(const std::string& key, const char* value) {
    return toJSON(key, std::string(value));
}

JSON toJSON(const std::string& str) {
    return "\"" + JSONEscape(str) + "\"";
}

JSON toJSON(const JSON& str) {
    return str;
}

JSON toJSON(bool value) {
    return value ? "true" : "false";
}

JSON toJSON(const UserObject& o) {
    return o.json();
}

JSON toJSON(const TestReport::CaseEntry& e) {
    std::string out = "{";
    
    out += toJSON("output", e.output) + ',';
    out += toJSON("output_json", e.output_json) + ',';
    out += toJSON("result", e.result) + ',';
    out += toJSON("input", e.input) + ',';//"\"input\":" + toJSON(e.input) + ',';
    out += toJSON("input_args", toJSON(e.input_args)) + ',';
    out += toJSON("input_params", toJSON(e.input_params)) + ',';
    out += toJSON("output_params", toJSON(e.output_params)) + ',';
    out += toJSON("correct", e.correct);
    
    out += "}";
    return out;
}

JSON toJSON(const TestReport& r) {
    
    std::string out = "{";
    
    if(const auto d = std::get_if<TestReport::EqualsData>(&r.data)) {
        out += toJSON("type", "EE") + ',';
        
        out += toJSON("left", d->left.string()) + ',';
        out += toJSON("right", d->right.string()) + ',';
        out += toJSON("result", d->result) + ',';
        out += toJSON("descriptor", d->descriptor) + ',';
    } else if(const auto d = std::get_if<TestReport::TrueData>(&r.data)) {
        out += toJSON("type", "ET") + ',';
        
        out += toJSON("value", d->value) + ',';
        out += toJSON("result", d->result) + ',';
        out += toJSON("descriptor", d->descriptor) + ',';
    } else if(const auto d = std::get_if<TestReport::FalseData>(&r.data)) {
        out += toJSON("type", "EF") + ',';
        
        out += toJSON("value", d->value) + ',';
        out += toJSON("result", d->result) + ',';
        out += toJSON("descriptor", d->descriptor) + ',';
    } else if(const auto d = std::get_if<TestReport::CaseData>(&r.data)) {
        out += toJSON("type", "TC") + ',';
        
        out += "\"cases\":" + toJSON(*d) + ',';
    }
    
    out += toJSON("info", r.info_stream->str());
    
    out += "}";
    return out;
}

JSON toJSON(const TestData& data) {
    
    std::string out = "{";
    
    out += "\"results\":" + toJSON(data.reports) + ',';
    out += toJSON("grading_method", data.grading_method) + ',';
    out += toJSON("format", data.output_format) + ',';
    out += toJSON("points", data.points) + ',';
    out += toJSON("max_points", data.max_points) + ',';
    out += toJSON("stdout", data.sout) + ',';
    out += toJSON("stderr", data.serr) + ',';
    out += toJSON("correct", data.correct) + ',';
    out += toJSON("incorrect", data.incorrect);
    
    out += "}";
    return out;
}

}