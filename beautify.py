import json
import os
import sys
import argparse
import html

parser = argparse.ArgumentParser()
parser.add_argument("-o", dest='out', type=str, default="stdout")
parser.add_argument("-i", dest='input', type=str, default="report.json")
parser.add_argument("-p", dest='max_points', type=float, default=-1)
args = parser.parse_args()

default_format = "list"

template_location = sys.path[0] + "/templates/"
template_filenames = {
    "vertical": "vertical.html", 
    "horizontal": "horizontal.html",
    "main": "main.html",
    "testbody": "testbody.html"
    }
report_filename = args.input
output_filename = args.out
max_points = args.max_points
templates = {}

def sanitize_replace(original, substr, replacor):
    replacor = html.escape(replacor)
    return original.replace(substr, html.escape(replacor))

def differences(correct, answer):
    """Returns a tuple with two lists of tuples with the difference locations and lengths"""
    
    def split(s):
        res = []
        pos = 0
        pos2 = 0
        while len(s) > pos2:
            while len(s) > pos2 and not s[pos2].isspace():
                pos2 += 1
            if pos != pos2:
                res.append(s[pos:pos2])
            pos = pos2
            while len(s) > pos2 and s[pos2].isspace():
                pos2 += 1
            if pos != pos2:
                res.append(s[pos:pos2])
            pos = pos2
        return res

    c_words = split(correct)
    a_words = split(answer)

    n = 0
    while n < len(c_words) and n < len(a_words) and c_words[n] == a_words[n]:
        n+=1
    start = c_words[:n]
    
    n = 0
    while n < len(c_words) and n < len(a_words) and c_words[-n-1] == a_words[-n-1]:
        n+=1
    if n != 0:
        end = c_words[-n:]
        c_words = c_words[len(start):-len(end)]
        a_words = a_words[len(start):-len(end)]
    else:
        end = []
        c_words = c_words[len(start):]
        a_words = a_words[len(start):]

    def lcslengths(b,a):
        lengths = [[0] * (len(a)+1) for _ in range(len(b)+1)]
        for i, x in enumerate(b, 1):
            for j, y in enumerate(a, 1):
                if x == y:
                    lengths[i][j] = lengths[i-1][j-1] + 1
                else:
                    lengths[i][j] = max(lengths[i][j-1], lengths[i-1][j])
        return lengths

    def get_indices(arr, res, pos):
        li = 0
        tres = []
        for k in res:
            while li < k:
              pos += len(arr[li])
              li+=1
            tres.append((pos, len(arr[li])))
        return tres

    def diffs(arr1, arr2):
        lengths = lcslengths(arr1,arr2)
        res1 = []
        res2 = []
        i = len(arr1)
        j = len(arr2)
        while i != 0 or j != 0:
            if i == 0:
                j -= 1
                res2.append(j)
            elif j == 0:
                i -= 1
                res1.append(i)
            elif arr1[i-1] == arr2[j-1]:
                i -= 1
                j -= 1
            elif lengths[i][j-1] > lengths[i-1][j]:
                j -= 1
                res2.append(j)
            else:
                i -= 1
                res1.append(i)
        res1.reverse()
        res2.reverse()
        return res1, res2

    dc, da = diffs(c_words, a_words)
    offset = sum([len(s) for s in start])
    '''
    if len(da) != 0 and len(da) == len(dc):
        ds = [diffs(c_words[dc[i]], a_words[da[i]]) for i in range(len(da))]
        print(ds)
        dc2, da2 = list(zip(*ds))
        print(dc2, da2)
    '''
    
    return get_indices(c_words, dc, offset), get_indices(a_words, da, offset)

def mark_differences(correct, answer):
    """Returns a tuple the differences between the two strings highlighted with html and css"""
    def mark(string, positions, start_marker, end_marker):
        offset = 0
        marker_length = len(start_marker) + len(end_marker)
        for pos, length in positions:
            hl = string[pos+offset:pos+offset+length]
            string = string[:pos+offset] + start_marker + hl.replace('\n',' \n') + end_marker + string[pos+offset+length:]
            offset += marker_length + hl.count('\n')
        return string

    cor_diff, ans_diff = differences(correct, answer)
    return mark(correct, cor_diff, "<span style=\"background-color: red\">", "</span>"), mark(answer, ans_diff, "<span style=\"background-color: red\">", "</span>")

def replace_entries(template, input_data, correct_data, output_data):
    """Returns the template with the input, output and correct tags replaced"""
    correct, output = mark_differences(str(correct_data), str(output_data))
    if isinstance(input_data, str):
        input = input_data
    elif len(input_data) == 1: # no brackets when only item
        input = str(input_data[0])
    else:
        input = [s if isinstance(s, str) else str(s) for s in input_data]
        input = str(type(input_data)) + "[" + ", ".join(input) + "]"
    if len(input) > 0 and input[-1] == '\n': # html pre tags need two newlines at the end to place a newline
        input += '\n'
    if len(output) > 0 and output[-1] == '\n':
        output += '\n'
    if len(correct) > 0 and correct[-1] == '\n':
        correct += '\n'
    templ = template.replace("{{{input}}}", input)
    templ = templ.replace("{{{output}}}", output)
    templ = templ.replace("{{{correct}}}", correct)
    return templ

def get_table(template, results, replace_func):
    """Fills the template with the data in results according to replace_func"""
    if len(results) == 0:
        return ""
    hor = template
    pos = 0
    entries = []
    while True: # find and copy the templates
        pos = hor.find("<!--ENTRY", pos)
        if pos == -1:
            break
        pos = hor.find("\n", pos)
        if pos == -1:
            break
        end = hor.find("-->", pos)
        if end == -1:
            break
        end = hor.rfind("\n", 0, end)
        if end == -1:
            break
        entries.append(hor[pos+1:end])
        pos = end

    pos = 0
    index = 0
    while True: # replace each template with a series of filled ones
        pos = hor.find("<!--ENTRY")
        if pos == -1:
            break
        end = hor.find("-->", pos)
        if end == -1:
            break

        content = ""
        for result in results:
            content += replace_func(entries[index], result)

        hor = hor[:pos] + content + hor[end+3:]

        pos = end
        index += 1
    
    return hor

def replace(templ, results, typ, func, input_text, output_text, correct_text):
    templ = sanitize_replace(templates[format_name], "{{{input_header}}}", input_text)
    templ = sanitize_replace(templ, "{{{output_header}}}", output_text)
    templ = sanitize_replace(templ, "{{{correct_header}}}", correct_text)
    return get_table(templ, [res for res in results if res["type"] == typ], func)

for key, file_name in template_filenames.items():
    with open(template_location + file_name, 'r') as f:
        templates[key] = f.read()

with open(report_filename, 'r') as f:
    report_data = json.load(f)
    test_results = report_data["test_results"]
    total_max_points = report_data["max_points"]
    
if max_points == -1:
    point_multiplier = 1
else:
    point_multiplier = max_points/total_max_points

#TODO: sanitize html
stdio = ""
tests = ""
for suite_name, suite_data in test_results.items():
    for test_name, test_data in suite_data.items():
        
        if ("stdout" in test_data and test_data["stdout"] != "") or ("stderr" in test_data and test_data["stderr"] != ""):
            stdio += "Test: " + test_name + "\n"
            if ("stdout" in test_data and test_data["stdout"] != ""):
                stdio += "Stdout: " + test_data["stdout"] + "\n"
            if ("stderr" in test_data and test_data["stderr"] != ""):
                stdio += "Stderr: " + test_data["stderr"] + "\n"
            stdio += "--------------------------------------------------------------\n"
        
        format_name = default_format if 'format' not in test_data else test_data['format']
        
        def ET_func(template, result):
            """Function for replacing condition case data"""
            return replace_entries(template, result["descriptor"], "True", result["result"])
            
        def EF_func(template, result):
            """Function for replacing condition case data"""
            return replace_entries(template, result["descriptor"], "True", result["result"])
        
        def EE_func(template, result):
            """Function for replacing condition case data"""
            return replace_entries(template, result["descriptor"], result["left"], result["right"])
            
        def TC_func(template, result):
            """Function for replacing test case data"""
            content = ""
            for case in result.get("cases", []):
                content += replace_entries(template, case["input"], case["correct"], case["output"])
            return content
        
        if test_data['finished']:
            content = replace(templates[format_name], test_data['results'], "ET", ET_func, "Condition", "Output", "Should be")
            content += replace(templates[format_name], test_data['results'], "EF", EF_func, "Condition", "Output", "Should be")
            content += replace(templates[format_name], test_data['results'], "EE", EE_func, "Condition", "Right (Output)", "Left (Correct)")
            content += replace(templates[format_name], test_data['results'], "TC", TC_func, "Input", "Output", "Correct")
        else:
            content = "Crashed or timed out while running this test."
        
        testbody = templates["testbody"].replace('{{{testname}}}', test_name)
        testbody = testbody.replace('{{{suitename}}}', suite_name)
        testbody = testbody.replace('{{{points}}}', str(test_data["points"]*point_multiplier))
        testbody = testbody.replace('{{{max_points}}}', str(test_data['max_points']*point_multiplier))
        testbody = testbody.replace('{{{point_state}}}', 
            "full-points" if test_data["points"] == test_data['max_points'] else 
            "zero-points" if test_data["points"] == 0 else 
            "partial-points")
        testbody = sanitize_replace(testbody, '{{{testid}}}', suite_name+test_name)
        testbody = testbody.replace('{{{testcontent}}}', content)

        tests += testbody

if "ERROR" in report_data:
    stdio += report_data["ERROR"]

main = sanitize_replace(templates["main"], '{{{stdio}}}', stdio)
main = main.replace('{{{tests}}}', tests)

if output_filename == "stdout":
    print(main)
else:
    with open(output_filename, 'w') as f:
        f.write(main)