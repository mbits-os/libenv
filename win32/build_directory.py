import sys, os, re

__dir__ = os.path.dirname(__file__)
parent = os.path.dirname(os.path.realpath(os.path.join(__dir__, "..", "..", ".."))) + "\\"

include = os.path.join(__dir__, "..", "includes", "__file__.win32.hpp")

def generate__(file, parent):
    print >>file, """#ifndef __FILE___WIN32_HPP__
#define __FILE___WIN32_HPP__

#ifdef WIN32
"""
    print >>file, "#define BUILD_DIR \"%s\"" % parent.replace('\\', '\\\\')
    print >>file, "#define BUILD_DIR_LEN", len(parent)
    print >>file, """
#endif // WIN32

#endif // __FILE___WIN32_HPP__
"""

def generate(include, parent):
    print "Regenerating", include
    f = open(include, "w")
    generate__(f, parent)
    
def check_and_replace(include, parent):
    f = open(include)
    in_file = None
    in_file_len = None

    for line in f:
        match = re.match("#define BUILD_DIR \"([^\"]+)\"", line)
        if match:
            in_file = match.group(1)
        else:
            match = re.match("#define BUILD_DIR_LEN ([0-9]+)", line)
            if match:
                in_file_len = int(match.group(1))
    f.close()

    result = in_file is None or in_file_len is None
    if not result:
        result = in_file_len != len(parent)
    if not result:
        result = parent.replace('\\', '\\\\') != in_file

    if not result:
        return

    generate(include, parent)

if os.path.exists(include):
    check_and_replace(include, parent)
else:
    generate(include, parent)
