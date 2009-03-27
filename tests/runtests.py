#!/usr/bin/python2.4
# -*- mode: python -*-

"""Verification script used to test MAO. The script takes assembly file as input
with special comments used to determine PASS or FAIL. For each assembly file
tested, the script will print the name of the file tested followed by either
PASS or FAIL. The scripts accepts filenames as input, or use the flag -f to give
file which includes a list of filenames to test. The script assumes that
mao exists and is exeutable in the same directory as the script.

Format for special comments in assembly file to be tested:
#Option:  <Options to pass to mao>
#grep <Pattern> <Expected Number Of Matches>

e.g.:
#Option:  -mao:RELAX=stat[1]
#grep MaoRelax.*foo.*2 1
"""

import os
import re
import subprocess
import sys
import getopt

class RunError(Exception):
  def __init__(self, returncode, command, root):
    Exception.__init__(self)
    self.returncode = returncode
    self.command = command
    self.root = root

def _RunCheck(command, stdin=None, stdout=None, stderr=None, env=None):
  try:
    child = subprocess.Popen(command,
                             stdin=stdin, stdout=stdout, stderr=stderr,
                             env=env)
    (output, outputstderr) = child.communicate()
    err = child.wait()
    if err != 0:
      raise RunError(err, command, None)
    return output + outputstderr
  except OSError, e:
    raise RunError(1, command, e)

# Runs mao on the inputfile. and return the output from both
# standard error and standard output as a big string.
def RunMao(maocommand, inputfile, options):
    cmd = [maocommand, options ,inputfile]
    output = _RunCheck(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    return output

# Reads the input file and returns a list of patterns to grep for, and
# the expected number of matches. [(pattern, num_matches), ....]
def GetPatterns(inputfile):
    # Get the patterns to search for
    f = open(inputfile, 'r')
    patterns = []
    for line in f:
      match = re.search(r'#grep:? ?(.*) ([0-9]+)', line)
      if match:
          patterns.append( (match.group(1).strip(), match.group(2).strip()) );
    return  patterns

# Returns the number of matches of the pattern in text.
def GetNumberOfMatches(pattern, text):
    matches = re.findall(pattern, text)
    if matches == None:
        return 0
    else:
        return len(matches)

# Reads the options specified in the input file
# and returns them as a string. If no options
# are found, None is returned.
def GetOptions(inputfile):
    # Get the patterns to search for
    f = open(inputfile, 'r')
    for line in f:
      match = re.search(r'#Option: (.*)', line)
      if match:
        return match.group(1).strip()
    return None

def main(argv):
  # Check for -f <filename> options
  opts, args = getopt.getopt(argv[1:], "f:")
  for (flag, value) in opts:
    if flag == "-f":
      f = open(value, 'r')
      for line in f:
        args.append(line.strip())
    else:
      print "Unsupported flag: %(flag)s" % {'flag' : flag}

  # Loop over input filenames
  for inputfile in args:
    options = GetOptions(inputfile)
    # Make sure we find the mao options
    if options == None:
      print "Unable to find options in input file: %(file)s" % \
          {'file' : inputfile}
      continue

    # Run mao and get the output in mao_output
    mao_output = RunMao(os.path.join(os.path.dirname(argv[0]), "mao"), \
                            inputfile, options)

    # Run pattern checker
    num_patterns = 0
    num_patterns_passed = 0
    patterns = GetPatterns(inputfile)
    error_msgs = []
    for pattern in patterns:
      num_patterns += 1
      num_matches = GetNumberOfMatches(pattern[0], mao_output)
      if num_matches != int(pattern[1]):
        error_msgs.append("Found " + str(num_matches) + " instances of " \
                         + pattern[0] + " Should have been " + str(pattern[1]))
      else:
        num_patterns_passed += 1

    # Print out the status line:
    print '%(f)-20s' % {'f' : inputfile},
    if len(error_msgs) == 0:
      print "PASS",
    else:
      print "FAIL",
    print "(%(n1)d/%(n2)d)" % \
      { 'n1': num_patterns_passed, 'n2': num_patterns },
    print ' - '.join(error_msgs)

if __name__ == "__main__":
  main(sys.argv)