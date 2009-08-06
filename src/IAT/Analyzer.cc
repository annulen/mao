//
// Copyright 2009 and later Google Inc.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the
//   Free Software Foundation Inc.,
//   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
// Author: Casey Burkhardt (caseyburkhardt)
// Description: File serves as the main operational source for the analyzer.
//              It parses command line arguments, reads input data, generates
//              result objects, performs per instruction calculations, and
//              writes the output files to the file system.

#include <math.h>
#include <string>

#include "./Analyzer.h"

int main(int argc, char* argv[]) {
  // Variables used for array size declaration and target referencing
  int number_iterations = 0;
  int number_instructions = 0;
  int number_results = 0;
  string target_directory;

  // Variables used for file processing and array indexing
  string file_name;
  FILE *file_in, *file_out;
  char line[kMaxBufferSize];
  int results_processed = 0;

  // Variables used for event tracking and baseline correction
  string event_name;
  long int baseline_raw_event_count = 0;

  // Command Line Argument Parsing
  // TODO(caseyburkahrdt): Add argument to display help menu
  // TODO(caseyburkhardt): Add argument to trigger verbose mode
  for (int i = 1; i < argc; ++i) {
    if (strncmp(argv[i], kTargetDirectoryFlag,
                strlen(kTargetDirectoryFlag)) == 0) {
      target_directory = ParseCommandLineString(argv[i], kTargetDirectoryFlag);
    } else if (strncmp(argv[i], kInstructionCountFlag,
                       strlen(kInstructionCountFlag)) == 0) {
      number_instructions = ParseCommandLineInt(argv[i], kInstructionCountFlag);
    } else if (strncmp(argv[i], kIterationCountFlag,
                       strlen(kIterationCountFlag)) == 0) {
      number_iterations = ParseCommandLineInt(argv[i], kIterationCountFlag);
    } else {
      printf("Ignoring Unknown Command Line Argument: %s\n", argv[i]);
    }
  }

  // Check (and replace if necessary) Command Line Argument Values
  if (target_directory == "") {
    printf("Results Target Directory Flag Missing or Invalid\n");
    exit(1);
  }

  // Determine the instruction and iteration count for the test set if it wasn't
  // already passed in as a command line argument.
  if (number_instructions <= 0 && number_iterations <= 0) {
    file_name = target_directory + "/" + kTestSetDataFile;
    if ((file_in = fopen(file_name.c_str(), "r")) == 0) {
      perror("Unable to open test set data file");
      exit(1);
    }
    while (fgets(line, sizeof(line), file_in) != NULL) {
      if (line[0] != kFileCommentCharacter) {
        if (strncmp(line, kInstructionCountFlag,
                    strlen(kInstructionCountFlag)) == 0) {
          number_instructions = ParseCommandLineInt(line,
                                                    kInstructionCountFlag);
        } else if (strncmp(line, kIterationCountFlag,
                           strlen(kIterationCountFlag)) == 0) {
          number_iterations = ParseCommandLineInt(line, kIterationCountFlag);
        }
      }
    }
    if (fclose(file_in) != 0) {
      perror("Unable to close test set data file");
      exit(1);
    }
    if (number_instructions <= 0 || number_iterations <= 0) {
      printf("Test set data file contained invalid data\n");
      exit(1);
    }
  }

  // Determine the raw baseline CPU event count and event name
  file_name = target_directory + "/" + kResultFileNamePrefix
              + kBaselineResultFileNameBody + kResultFileNameSuffix;
  if ((file_in = fopen(file_name.c_str(), "r")) == 0) {
    perror("Unable to open baseline test results file");
    exit(1);
  }
  if (fgets(line, sizeof(line), file_in) == NULL) {
    printf("Baseline test results file is empty.\n");
    exit(1);
  } else {
    baseline_raw_event_count = atoi(strtok(line, " "));
    event_name = strtok(NULL, " ");
  }
  if (fclose(file_in) != 0) {
    perror("Unable to close test set data file");
    exit(1);
  }

  if (baseline_raw_event_count <= 0 || event_name == "") {
    printf("Baseline test results is invalid.\n");
    exit(1);
  }

  number_results = CountUncommentedLines(target_directory + "/"
                                         + kResultIndexFileName,
                                         kFileCommentCharacter);

  // Holds the instances of Results Objects
  Result results[number_results];

  // Iterate over all files in the successfully compiled index and calculate
  // actual event counts per instruction and populate Result object data
  // members.
  file_name = target_directory + "/" + kResultIndexFileName;
  if ((file_in = fopen(file_name.c_str(), "r")) == 0) {
    perror("Unable to open test results index file");
    exit(1);
  }
  while (fgets(line, sizeof(line), file_in) != NULL) {
    results[results_processed] = GenerateResult(line, target_directory,
                                                baseline_raw_event_count,
                                                number_iterations,
                                                number_instructions);
    ++results_processed;
  }
  if (fclose(file_in) != 0) {
    perror("Unable to close test results index file");
    exit(1);
  }

  // Output results to a result file
  file_name = target_directory + "/" + kTestSetResultFileName;
  if ((file_out = fopen(file_name.c_str(), "w")) == 0) {
    perror("Unable to open results output file.\n");
    exit(1);
  }
  fprintf(file_out, "%c %s", kFileCommentCharacter, event_name.c_str());
  for (int i = 0; i < results_processed; ++i) {
    if (results[i].addressing_mode() != "") {
    fprintf(file_out, "%d, %s, %s\n", results[i].events_per_instruction(),
            results[i].operation_name().c_str(),
            results[i].addressing_mode().c_str());
    } else {
      fprintf(file_out, "%d, %s\n", results[i].events_per_instruction(),
              results[i].operation_name().c_str());
    }
  }
  if (fclose(file_out) != 0) {
    perror("Unable to close results output file.\n");
    exit(1);
  }
}

// This function returns the string value of a command line argument, given the
// actual argument and anticipated argument flag
string ParseCommandLineString(string arg, const string flag) {
  return arg.substr(strlen(flag.c_str()));
}

// This function returns the integer value of a command line argument, given the
// actual argument and anticipated argument flag
int ParseCommandLineInt(string arg, const string flag) {
  arg = arg.substr(flag.length());
  return atoi(arg.c_str());
}

// This function returns the number of uncommented lines within a file.  This is
// used to determine the size of the array used to hold the Result objects.
int CountUncommentedLines(string file_name, char comment_char) {
  int result = 0;
  FILE *file_in;
  char line[kMaxBufferSize];

  if ((file_in = fopen(file_name.c_str(), "r")) == 0) {
    perror("Unable to open data file");
    exit(1);
  }
  while (fgets(line, sizeof(line), file_in) != NULL) {
    if (line[0] != comment_char) {
      ++result;
    }
  }
  if (fclose(file_in) != 0) {
    perror("Unable to close file");
    exit(1);
  }
  return result;
}

// This function returns a fully populated Result object given the name of a
// result file, result directory, and other information needed to calculate
// actual event per instruction counts.
Result GenerateResult(string index_line, string result_directory,
                      long int baseline_event_count, int number_instructions,
                      int number_iterations) {
  // Object to populate and return
  Result result;

  // Variables related to file operation
  // Remove the '.s' extension from the filename and replace with result suffix.
  string file_name = index_line.substr(0, index_line.length() - 3);
  file_name = result_directory + "/" + kResultFileNamePrefix + file_name
              + kResultFileNameSuffix;
  FILE *file_in;
  string addressing_mode;
  char line[kMaxBufferSize];
  char holder[kMaxBufferSize];

  // Variables related to actual events per instruction calculation
  int events_per_instruction = -1;
  long int raw_events_per_result = 0;

  if ((file_in = fopen(file_name.c_str(), "r")) == 0) {
    perror("Unable to open test results file");
    printf("%s\n", file_name.c_str());
    exit(1);
  }
  if (fgets(line, sizeof(line), file_in) == NULL) {
    printf("Test results file is empty.\n");
    printf("%s\n", file_name.c_str());
    exit(1);
  } else {
    raw_events_per_result = atoi(strtok(line, " "));
  }
  if (fclose(file_in) != 0) {
    perror("Unable to close test results file");
    printf("%s\n", file_name.c_str());
    exit(1);
  }
  events_per_instruction = CalculateEventsPerInstruction(raw_events_per_result,
                                                         baseline_event_count,
                                                         number_instructions,
                                                         number_iterations);
  strcpy(holder, index_line.c_str());
  if (index_line.find('_') == -1) {
    // Test has no operands, addressing mode will be blank.
    result.set_operation_name(strtok(holder, "."));
    result.set_addressing_mode("");
    result.set_events_per_instruction(events_per_instruction);
    result.set_raw_event_count(raw_events_per_result);
  } else {
    result.set_operation_name(strtok(holder, "_"));
    addressing_mode = strtok(NULL, ".");
    result.set_addressing_mode(addressing_mode);
    result.set_events_per_instruction(events_per_instruction);
    result.set_raw_event_count(raw_events_per_result);
  }
  return result;
}

int CalculateEventsPerInstruction(long int raw_events, int baseline_events,
                                  int number_instructions,
                                  int number_iterations) {
  return round((double)(raw_events - baseline_events) / (number_instructions *
      number_iterations));
}