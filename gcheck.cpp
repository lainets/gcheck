#include "gcheck.h"

#include <iostream>
#include <fstream>
#include <algorithm>

#include "argument.h"
#include "redirectors.h"
#include "console_writer.h"

namespace gcheck {
namespace {
    /*
        Static class for keeping track of and logging test results.
    */
    class Formatter {
        typedef std::vector<std::pair<std::string, TestData>> TestVector;
        static std::vector<std::pair<std::string, TestVector>> suites_;

        static double total_points_;
        static double total_max_points_;

        static std::string default_format_;

        Formatter() {}; //Disallows instantiation of this class

    public:

        static bool pretty_;
        static std::string filename_;

        static void SetTestData(std::string suite, std::string test, TestData& data);
        static void SetTotal(double points, double max_points);
        // Writes the test results to stdout
        static void WriteFinalReport(bool ran_all);
        // Writes the test results to stdout
        static void WriteReport(std::string suite = "", std::string test = "");
    };

    double Formatter::total_points_ = 0;
    double Formatter::total_max_points_ = 0;
    bool Formatter::pretty_ = true;
    std::string Formatter::filename_ = "";
    std::string Formatter::default_format_ = "horizontal";
    std::vector<std::pair<std::string, Formatter::TestVector>> Formatter::suites_;
    
    
    void Formatter::WriteFinalReport(bool ran_all) {
        std::fstream file;
        auto& out = filename_ == "" ? std::cout : (file = std::fstream(filename_, std::ios_base::out));

        if(!pretty_) { //if not pretty, print out the whole json
            std::vector<std::pair<std::string, JSON>> output;
            output.push_back({"test_results", toJSON(suites_)});
            output.push_back({"points", toJSON(total_points_)});
            output.push_back({"max_points", toJSON(total_max_points_)});
            
            if(!ran_all)
                output.push_back({"WARNING", "\"Some tests weren't run because prerequisite tests weren't passed first.\""});
            
            out << toJSON(output) << std::endl << std::endl;
        } else {
            ConsoleWriter writer;
            writer.WriteSeparator();
            out << "Total: ";
            writer.SetColor(total_points_ == total_max_points_ ? ConsoleWriter::Green : ConsoleWriter::Red);
            out << total_points_ << " / " << total_max_points_;
            writer.SetColor(ConsoleWriter::Black);
            out << std::endl;
            
            if(!ran_all)
                out << "WARNING: Some tests weren't run because prerequisite tests weren't passed first." << std::endl;
            // Wait for user confirmation
            out << std::endl << "Press enter to exit." << std::endl;
            std::cin.get();
        }
    }
    
    void Formatter::WriteReport(std::string suite, std::string test) {

        std::fstream file;
        auto& out = filename_ == "" ? std::cout : (file = std::fstream(filename_, std::ios_base::out));

        if(!pretty_) { //if not pretty, print out the whole json
            std::vector<std::pair<std::string, JSON>> output;
            output.push_back({"test_results", toJSON(suites_)});
            output.push_back({"points", toJSON(total_points_)});
            output.push_back({"max_points", toJSON(total_max_points_)});
            
            output.push_back({"ERROR", "\"Crashed before finished all the tests. (Possible segmentation fault)\""});
            
            out << toJSON(output) << std::endl << std::endl;
        } else { //if pretty and not finished, print out only current test

            auto it = std::find_if(suites_.begin(), suites_.end(), 
                    [suite](std::pair<std::string, TestVector> a){ return a.first == suite; }
                );
            
            auto it2 = std::find_if(it->second.begin(), it->second.end(),
                    [test](const std::pair<std::string, TestData>& a){ 
                        return a.first == test; 
                    }
                );
            
            if(it2 == it->second.end()) {
                out << "error" << std::endl; //TODO actual error processing
                return;
            }

            ConsoleWriter writer;
            writer.WriteSeparator();
            
            TestData& test_data = it2->second;
            
            writer.SetColor(test_data.points == test_data.max_points ? ConsoleWriter::Green : ConsoleWriter::Red);
            out << test_data.points << " / " << test_data.max_points << "  suite: " << suite << ", test: " << test << std::endl;
            writer.SetColor(ConsoleWriter::Black);
            
            for(auto it = test_data.reports.begin(); it != test_data.reports.end(); it++) {
                std::vector<std::vector<std::string>> cells;
                if(const auto d = std::get_if<TestReport::EqualsData>(&it->data)) {
                    cells.push_back({});
                    auto& row = cells[cells.size()-1];
                    row.push_back(d->result ? "correct" : "incorrect");
                    row.push_back(d->descriptor);
                    row.push_back(d->left.string());
                    row.push_back(d->right.string());
                    row.push_back(it->info_stream->str());
                    
                    writer.SetHeaders({"Result", "Condition", "Left", "Right", "Info"});
                } else if(const auto d = std::get_if<TestReport::TrueData>(&it->data)) {
                    
                    cells.push_back({});
                    auto& row = cells[cells.size()-1];
                    row.push_back(d->result ? "correct" : "incorrect");
                    row.push_back(d->descriptor);
                    row.push_back(d->result ? "true" : "false");
                    row.push_back(it->info_stream->str());
                    
                    writer.SetHeaders({"Result", "Condition", "Value", "Info"});
                } else if(const auto d = std::get_if<TestReport::FalseData>(&it->data)) {
                    
                    cells.push_back({});
                    auto& row = cells[cells.size()-1];
                    row.push_back(d->result ? "correct" : "incorrect");
                    row.push_back(d->descriptor);
                    row.push_back(d->result ? "true" : "false");
                    row.push_back(it->info_stream->str());
                    
                    writer.SetHeaders({"Result", "Condition", "Value", "Info"});
                } else if(const auto d = std::get_if<TestReport::CaseData>(&it->data)) {
                    
                    for(auto it2 = d->begin(); it2 != d->end(); it2++) {
                        cells.push_back({});
                        auto& row = cells[cells.size()-1];
                        row.push_back(it2->result ? "correct" : "incorrect");
                        row.push_back(it2->input.Unescape());
                        row.push_back(it2->correct);
                        row.push_back(it2->output);
                        
                        writer.SetHeaders({"Result", "Input", "Correct", "Output"});
                    }
                } else if(const auto d = std::get_if<TestReport::EqualsData>(&it->data)) {
                    
                    cells.push_back({});
                    auto& row = cells[cells.size()-1];
                    row.push_back("Error: report type None");
                    break;
                }
                
                writer.WriteRows(cells);
            }
        }

        if(filename_ != "")
            file.close();
    }

    void Formatter::SetTestData(std::string suite, std::string test, TestData& data) {

        auto it = std::find_if(suites_.begin(), suites_.end(), [&suite](const std::pair<std::string, TestVector>& item){ return suite == item.first; });
        if(it == suites_.end()) {
            suites_.push_back({ suite, TestVector() });
            it = suites_.end()-1;
        }
        
        auto it2 = std::find_if(it->second.begin(), it->second.end(), [&test](const std::pair<std::string, TestData>& p) { return p.first == test; });
        if(it2 == it->second.end()) {
            it->second.push_back({test, data});
        } else {
            it2->second = data;
        }
        
        if(!pretty_ || data.finished)
            WriteReport(suite, test);
    }

    void Formatter::SetTotal(double points, double max_points) {
        total_max_points_ = max_points;
        total_points_ = points;
    }
}


double Test::default_points_ = 1;

Test::Test(std::string suite, std::string test, double points, int priority) : suite_(suite), test_(test), priority_(priority) {
    data_.max_points = points;
    test_list_().push_back(this);
}

double Test::RunTest() {
    
    Formatter::SetTestData(suite_, test_, data_);
    
    StdoutCapturer tout;
    StderrCapturer terr;
    
    ActualTest();
    
    tout.Restore();
    terr.Restore();
    data_.sout = tout.str();
    data_.serr = terr.str();
    
    data_.CalculatePoints();
    
    Formatter::SetTestData(suite_, test_, data_);

    return data_.points;
}

TestReport& Test::AddReport(TestReport& report) {
    
    report.test_class = typeid(*this).name(); 
    
    auto increment_correct = [this](bool b) { 
        b ? data_.correct++ : data_.incorrect++;
    };
    
    if(const auto d = std::get_if<TestReport::EqualsData>(&report.data)) {
        increment_correct(d->result);
    } else if(const auto d = std::get_if<TestReport::TrueData>(&report.data)) {
        increment_correct(d->result);
    } else if(const auto d = std::get_if<TestReport::FalseData>(&report.data)) {
        increment_correct(d->result);
    } else if(const auto cases = std::get_if<TestReport::CaseData>(&report.data)) {
        for(auto it = cases->begin(); it != cases->end(); it++) {
            increment_correct(it->result);
        }
    }
    data_.reports.push_back(report);
    
    return data_.reports[data_.reports.size()-1];
}

void Test::GradingMethod(gcheck::GradingMethod method) {
    data_.grading_method = method; 
}

void Test::OutputFormat(std::string format) { 
    data_.output_format = format; 
}
    
std::stringstream& Test::ExpectTrue(bool b, std::string descriptor) {

    TestReport report = TestReport::Make<TestReport::TrueData>();
    auto& data = report.Get<TestReport::TrueData>();
    
    data.value = b;
    data.descriptor = descriptor;
    data.result = b;
    
    return *AddReport(report).info_stream;
}

std::stringstream& Test::ExpectFalse(bool b, std::string descriptor) {
    
    TestReport report = TestReport::Make<TestReport::FalseData>();
    auto& data = report.Get<TestReport::FalseData>();
    
    data.value = b;
    data.descriptor = descriptor;
    data.result = !b;
    
    return *AddReport(report).info_stream;
}

bool Test::RunTests() {
    
    auto& test_list = test_list_();
    std::sort(test_list.begin(), test_list.end(), [](const Test* a, const Test* b){ return a->priority_ < b->priority_; });

    double total_max = 0;
    for(auto it = test_list_().begin(); it != test_list_().end(); it++) {
        total_max += (*it)->data_.max_points;
    }
    Formatter::SetTotal(0, total_max);

    double total = 0;
    double max_so_far = 0;
    int c_priority = -1;
    for(auto it = test_list_().begin(); it != test_list_().end(); it++) {
        if(c_priority != (*it)->priority_) {
            if(max_so_far != total)
                return false; // stop if last prerequisite priority level didnt gain full points
            
            c_priority = (*it)->priority_;
        }
        total += (*it)->RunTest();
        max_so_far += (*it)->data_.max_points;
        Formatter::SetTotal(total, total_max);
    }
    
    return true;
}

template std::stringstream& Test::ExpectEqual(unsigned int left, unsigned int right, std::string descriptor);
template std::stringstream& Test::ExpectEqual(int left, int right, std::string descriptor);
template std::stringstream& Test::ExpectEqual(float left, float right, std::string descriptor);
template std::stringstream& Test::ExpectEqual(double left, double right, std::string descriptor);
template std::stringstream& Test::ExpectEqual(std::string left, std::string right, std::string descriptor);
template std::stringstream& Test::ExpectEqual(std::string left, const char* right, std::string descriptor);
template std::stringstream& Test::ExpectEqual(const char* left, std::string right, std::string descriptor);

}

int main(int argc, char** argv) {

    using namespace gcheck;

    for(int i = 1; i < argc; ++i) {
        if(argv[i] == std::string("--json")) Formatter::pretty_ = false;
        else Formatter::filename_ = argv[i];
    }
    if(!Formatter::pretty_ && Formatter::filename_ == "") Formatter::filename_ = "report.json";

    //printf(" \r");
    //std::cout << " \r";
    bool ran_all = Test::RunTests();

    Formatter::WriteFinalReport(ran_all);

    return 0;
}