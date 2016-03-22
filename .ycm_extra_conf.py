import os
from itertools import chain

def GetAbsPath(p):
  if p.startswith('/'):
    return p
  working_directory = os.path.dirname(os.path.abspath(__file__))
  return os.path.join(working_directory, p)

flags = [
'-Werror=all',
'-Werror=extra',
'-Werror=pedantic',
'-pedantic',
'-pedantic-errors',
'-std=c++11'
]

includes = [
'.',
'./rethread',
'./thirdparty/googletest/googletest/include',
'./thirdparty/googletest/googlemock/include'
]

def FlagsForFile( filename, **kwargs ):
  return {
    'flags': flags + list(chain.from_iterable(map(lambda f: ['-I', GetAbsPath(f)], includes))),
    'do_cache': True
  }
