#!/usr/bin/env python3

import json
import os
import sys
import argparse
import html
from jinja2 import Environment, FileSystemLoader, evalcontextfilter, Markup
from report_parser import Report, Test, Result, Type, UserObject

default_format = "vertical"

template_filenames = {
    "vertical": "vertical.html",
    "horizontal": "horizontal.html",
    "main": "main.html",
    "testbody": "testbody.html"
    }

if "__file__" in globals():
    default_templates = os.path.split(__file__)[0]
else:
    default_templates = os.path.join(sys.path[0], "../templates/")


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
    while n + len(start) < len(c_words) and n + len(start) < len(a_words) and c_words[-n-1] == a_words[-n-1]:
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

    return get_indices(c_words, dc, offset), get_indices(a_words, da, offset)

class Diff:
    def __init__(self, string, positions = None):
        if positions is None:
            self.diff = ["", str(string)]
        else:
            lastpos = 0
            self.diff = []
            for pos, length in positions:
                self.diff.append(string[lastpos:pos])
                self.diff.append(string[pos:pos+length])
                lastpos = pos+length
            self.diff.append(string[lastpos:])


@evalcontextfilter
def diff_filter(eval_ctx, value, start, end, add_space = True):
    if isinstance(value, Diff):
        res = ""
        for index, val in enumerate(value.diff):
            if index % 2 == 0:
                res = res + val
            elif add_space:
                res = res + Markup(start) + val.replace("\n", " \n") + Markup(end)
            else:
                res = res + Markup(start) + val + Markup(end)
        return res
    else:
        return value


def mark_differences(correct, answer):
    """Returns a tuple the differences between the two strings highlighted with html and css"""
    if isinstance(correct, UserObject):
        correct = correct.string
    if isinstance(answer, UserObject):
        answer = answer.string
    if answer is None or correct is None:
        return correct, answer
    if isinstance(correct, str):
        cor_diff, ans_diff = differences(correct, answer)
        return Diff(correct, cor_diff), Diff(answer, ans_diff)
    else:
        if correct == answer:
            return correct, answer
        else:
            return Diff(correct), Diff(answer)


class Beautify:
    def __init__(self, report, template_paths = None):
        if template_paths is None:
            template_paths = [default_templates]
        else:
            if not isinstance(template_paths, list):
                template_paths = [template_paths]
            template_paths.append(default_templates)
        self.env = Environment(loader=FileSystemLoader(template_paths), autoescape=True)
        self.env.filters['diff'] = diff_filter
        self.report = report
        self.templates = template_filenames

    def add_template(self, name, filename):
        self.templates[name] = filename

    def stdio(self):
        stdio = ""
        for test in self.report.tests:
            if test.stdout != "" or test.stderr != "":
                stdio += "Test: " + test.get_name() + "\n"
                if (test.stdout != ""):
                    stdio += "Stdout: " + test.stdout + "\n"
                if (test.stderr != ""):
                    stdio += "Stderr: " + test.stderr + "\n"
                stdio += "--------------------------------------------------------------\n"
        return stdio

    def render_main(self):
        return self.render(self.templates["main"], render_test=self.render_test, render_stdio=self.render_stdio, tests=self.report.tests)

    def render_stdio(self):
        return self.stdio()

    def render_test(self, test: Test):
        format_name = default_format if not test.format else test.format
        return self.render(self.templates["testbody"], obj=test, format=format_name, render_result=self.render_result, suite=test.suite, test=test.test, points=test.points, max_points=test.max_points, results=test.results)

    def render_result(self, result: Result, format):
        if result.type == Type.TC:
            rows = []
            for case in result.cases:
                rows.append(["correct" if case.result else "incorrect", case.input.string, case.arguments.string, *mark_differences(case.output.string, case.output_expected.string)])
            return self.render(self.templates[format], headers=["Result", "Input", "Arguments", "Output", "Should be"], rows=rows)
        elif result.type == Type.EE:
            rows = [["correct" if result.result else "incorrect", result.descriptor, *mark_differences(result.output, result.output_expected)]]
            return self.render(self.templates[format], headers=["Result", "Condition", "Value (Output)", "Should be"], rows=rows)
        elif result.type == Type.ET or result.type == Type.EF:
            rows = [["correct" if result.result else "incorrect", result.descriptor, *mark_differences(result.value, True)]]
            return self.render(self.templates[format], headers=["Result", "Condition", "Value (Output)", "Should be"], rows=rows)
        elif result.type == Type.FC:
            all_keys = ["run_time", "max_run_time",
                    "object", "object_after", "object_after_expected",
                    "arguments", "arguments_after", "arguments_after_expected",
                    "input", "output", "output_expected", "error", "error_expected",
                    "return_value", "return_value_expected"]
            keys = set()
            for case in result.cases:
                keys.update(key for key in all_keys if getattr(case, key) is not None)
            if "run_time" not in keys or "max_run_time" not in keys:
                keys.discard("run_time")
                keys.discard("max_run_time")
            keys = [key for key in all_keys if key in keys]
            header_dict = {"run_time": "Run time", "max_run_time": "Max run time",
                    "object": "Object", "object_after": "Object afterwards", "object_after_expected": "Expected object afterwards",
                    "arguments": "Arguments", "arguments_after": "Arguments afterwards", "arguments_after_expected": "Expected arguments afterwards",
                    "input": "Standard input", "output": "Standard output", "output_expected": "Expected standard output", "error": "Standard error", "error_expected": "Expected standard error",
                    "return_value": "Return value", "return_value_expected": "Expected return value"}
            headers = ["Result"] + [header_dict[key] for key in keys]
            diff_pairs = [("object_after", "object_after_expected"), ("arguments_after", "arguments_after_expected"),
                    ("output", "output_expected"), ("error", "error_expected"), ("return_value", "return_value_expected")]
            rows = []
            for case in result.cases:
                if case.timed_out:
                    rows.append(f"Timed out (max time: {case.timeout})")
                else:
                    data = {d[0]: d[1] for p in diff_pairs for d in zip(p, mark_differences(getattr(case, p[0]), getattr(case, p[1])))}
                    data.update({key: getattr(case, key) for key in keys if key not in data})
                    row = ["correct" if case.result else "incorrect"] + [data[key] for key in keys]
                    row = [r.string if isinstance(r, UserObject) else r for r in row]
                    rows.append(row)
            return self.render(self.templates[format], headers=headers, rows=rows)

    def render(self, template_name, **kwargs):
        return self.env.get_template(template_name).render(render=self.render, **kwargs)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("-o", dest='out', type=str, default="stdout")
    parser.add_argument("-i", dest='input', type=str, default="report.json")
    parser.add_argument("-p", dest='max_points', type=float, default=-1)
    parser.add_argument("-t", dest='template_dir', type=str, default=default_templates)
    args = parser.parse_args()

    template_location = args.template_dir
    report_filename = args.input
    output_filename = args.out
    max_points = args.max_points

    report = Report(report_filename)
    beautify = Beautify(report, template_location)

    main = beautify.render_main()

    if output_filename == "stdout":
        print(main)
    else:
        with open(output_filename, 'w') as f:
            f.write(main)