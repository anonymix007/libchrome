#!/usr/bin/env python

# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Generates wrapped include files to workaround -Wunused-parameter errors.

In Chrome repository, "-Wunused-parameter" is disabled, and several header
files in Chrome repository have actually unused-parameter.
One of the typical scenarios is; in Chrome, Observer class is often defined
as follows:

class Foo {
 public:
  class Observer {
   public:
    virtual void OnSomeEvent(EventArg arg) {}
    virtual void OnAnotherEvent(EventArg arg) {}
    ...
  };
  ...
};

Here, On...Event() methods do nothing by default, and subclasses will override
only necessary ones.
In this use case, argument names can also work as documentation, and overrides
can use these good interface-defined default names as a starting point for
their implementation.

On the other hand, in Android, -Wunused-parameter is enabled by default.
Thus, if such a project includes header files from libchrome, it could cause
a compile error (by the warning and "-Werror").

To avoid such a situation, libchrome exports include files wrapped by the
pragmas as follows.

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
${actual_include_file_content}
#pragma GCC diagnostic pop

so, the unused-parameter warning generated by the libchrome include headers
will be ignored.
Note that these GCC pragmas are also supported by clang for compatibility. cf)
https://clang.llvm.org/docs/UsersManual.html#controlling-diagnostics-via-pragmas

Usage: include_generator.py $(in) $(out)
"""

import sys


def _generate(input_path, output_path):
    """Generates a include file wrapped by pragmas.

    Reads the file at |input_path| and output the content with wrapping by
    #pragma to ignore unused-parameter warning into the file at |output_path|.
    If the parent directories of |output_path| do not exist, creates them.

    Args:
        input_path: Path to the source file. Expected this is a chrome's header
            file.
        output_path: Path to the output file.
    """
    with open(input_path, 'r') as f:
        content = f.read()

    with open(output_path, 'w') as f:
        f.writelines([
            '// Generated by %s\n' % sys.argv[0],
            '#pragma GCC diagnostic push\n'
            '#pragma GCC diagnostic ignored "-Wunused-parameter"\n',
            content,
            '#pragma GCC diagnostic pop\n'])


def main():
    _generate(*sys.argv[1:])


if __name__ == '__main__':
    main()
