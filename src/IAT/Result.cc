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
// Description: File serves as the primary implementation for the Result
//              class, which represents a single result generated by the runner
//              script.
//

#include <string>

#include "./Result.h"
#include "./Analyzer.h"

// Constructor
Result::Result() {
  this->operation_name_.clear();
  this->addressing_mode_.clear();
  this->events_per_instruction_ = 0;
  this->raw_instruction_count_ = 0;
}

// Destructor
Result::~Result() {
  // TODO(caseyburkhardt): Implement Destructor
}

void Result::set_operation_name(string data) {
  this->operation_name_ = data;
}

string Result::operation_name() {
  return this->operation_name_;
}

void Result::set_addressing_mode(string data) {
  this->addressing_mode_ = data;
}

string Result::addressing_mode() {
  return this->addressing_mode_;
}

void Result::set_events_per_instruction(int value) {
  this->events_per_instruction_ = value;
}

int Result::events_per_instruction() {
  return this->events_per_instruction_;
}

void Result::set_raw_event_count(long int value) {
  this->raw_instruction_count_ = value;
}

long int Result::raw_event_count() {
  return this->raw_instruction_count_;
}

string Result::OutputString() {
  char charValue[kMaxBufferSize];

  string result = "Operation Name: " + this->operation_name_ + "\n";
  result = result + "Addressing Mode: " + this->addressing_mode_ + "\n";
  sprintf(charValue, "%d", this->events_per_instruction_);
  result = result + "Events Per Instruction: " + charValue + "\n";
  sprintf(charValue, "%ld", this->raw_instruction_count_);
  result = result + "Raw Event Count: " + charValue;
  return result;
}
