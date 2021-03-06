# Copyright 2011 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'All',
      'type': 'none',
      'dependencies': [
        '../samples/samples.gyp:*',
        '../src/d8.gyp:d8',
        '../test/cctest/cctest.gyp:*',
        '../test/unittests/unittests.gyp:*',
      ],
      'conditions': [
        ['component!="shared_library"', {
          'dependencies': [
            '../tools/parser-shell.gyp:parser-shell',
          ],
        }],
        ['test_isolation_mode != "noop"', {
          'dependencies': [
            '../test/default.gyp:*',
            '../test/intl/intl.gyp:*',
            '../test/message/message.gyp:*',
            '../test/mjsunit/mjsunit.gyp:*',
            '../test/preparser/preparser.gyp:*',
            '../test/test262/test262.gyp:*',
          ],
        }],
      ]
    }
  ]
}
