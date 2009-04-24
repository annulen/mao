//
// Copyright 2008 Google Inc.
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
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, 5th Floor, Boston, MA  02110-1301, USA.



#include <cstring>
#include <fstream>
#include <iostream>

#include <stdio.h>

#include "ir-gas.h"
#include "irlink.h"
#include "MaoDebug.h"
#include "MaoOptions.h"
#include "MaoUnit.h"

extern "C" const char *S_GET_NAME(symbolS *s);
extern "C" void   as_where(char **, unsigned int *);

// Reference to a the mao_unit.
static MaoUnit *maounit_;
static enum bfd_reloc_code_real reloc_ = _dummy_first_bfd_reloc_code_real;

struct link_context_s {
  unsigned int line_number;
  char *filename;
};

static std::string quote_c_string(const char *c_str) {
  std::string str;
  str.append("\"");
  str.append(c_str);
  str.append("\"");
  return str;
}

static std::string quote_string_piece(const MaoStringPiece &piece) {
  std::string str;
  str.append("\"");
  str.append(piece.data, piece.length);
  str.append("\"");
  return str;
}

struct link_context_s get_link_context() {
  struct link_context_s link_context;
  unsigned int line_no;
  char *file;
  as_where(&file, &line_no);
  link_context.line_number = line_no;
  link_context.filename = file;
  return link_context;
}

static void link_directive_tail(
    DirectiveEntry::Opcode opcode,
    DirectiveEntry::OperandVector operands) {
  struct link_context_s link_context = get_link_context();
  DirectiveEntry *directive =
      new DirectiveEntry(opcode, operands,
                         link_context.line_number, NULL, maounit_);
  maounit_->AddEntry(directive, false);
  // This makes sure that we only catch relocs that happen in the current entry.
  reloc_ = _dummy_first_bfd_reloc_code_real;
}

void link_insn(i386_insn *insn, size_t SizeOfInsn, int code_flag,
               const char *line_verbatim) {
  MAO_ASSERT(sizeof(i386_insn) == SizeOfInsn);
  i386_insn *inst = insn;

  struct link_context_s link_context = get_link_context();
  MAO_ASSERT(maounit_);
  maounit_->AddEntry(new InstructionEntry(inst, (enum flag_code)code_flag,
                                          link_context.line_number,
                                          line_verbatim, maounit_), true);
}

void link_label(const char *name, const char *line_verbatim) {
  // Add the label to the symbol table.
  SymbolTable *symbol_table = maounit_->GetSymbolTable();
  MAO_ASSERT(symbol_table);
  struct link_context_s link_context = get_link_context();
  MAO_ASSERT(maounit_);
  maounit_->AddEntry(new LabelEntry(name, link_context.line_number,
                                    line_verbatim, maounit_), true);
}

void link_symbol(const char *name, enum SymbolVisibility symbol_visibility,
                 const char *line_verbatim) {
  // one of two things happens here.
  // If the symbol exists, then update its visibility.
  // If a symbol does not exists, then create a symbol, but
  // with an undefined section

  // Get table
  MAO_ASSERT(maounit_);
  SymbolTable *symbol_table = maounit_->GetSymbolTable();
  MAO_ASSERT(symbol_table);
  // Create symbol if needed
  if (!symbol_table->Exists(name)) {
    maounit_->AddSymbol(name);
  }
  // Get symbol
  Symbol *symbol = symbol_table->Find(name);
  MAO_ASSERT(symbol);

  // Update symbol
  symbol->set_symbol_visibility(symbol_visibility);
}

void link_comm(const char *name, unsigned int common_size,
               unsigned int common_align, const char *line_verbatim) {
  MAO_ASSERT(name);
  MAO_ASSERT(maounit_);
  maounit_->AddCommSymbol(name, common_size, common_align);

  DirectiveEntry::OperandVector operands;
  operands.push_back(new DirectiveEntry::Operand(name));
  operands.push_back(new DirectiveEntry::Operand(common_size));
  operands.push_back(new DirectiveEntry::Operand(common_align));
  link_directive_tail(DirectiveEntry::COMM, operands);
}

char is_whitespace(char c) {
  return (c == ' ' || c == '\t');
}

void link_section(int push,
                 const char *section_name,
                 MaoStringPiece arguments) {
  MAO_ASSERT(section_name);
  MAO_ASSERT(maounit_);

  if (push) {
    maounit_->PushCurrentSubSection();
  }

  DirectiveEntry::OperandVector operands;
  operands.push_back(new DirectiveEntry::Operand(
                         section_name));
  if (arguments.length)
    operands.push_back(new DirectiveEntry::Operand(arguments));
  link_directive_tail(DirectiveEntry::SECTION, operands);
}

void link_type(symbolS *symbol, SymbolType symbol_type,
               const char *line_verbatim) {
  MAO_ASSERT(maounit_);
  SymbolTable *symbol_table = maounit_->GetSymbolTable();
  MAO_ASSERT(symbol_table);
  Symbol *mao_symbol = maounit_->FindOrCreateAndFindSymbol(symbol->bsym->name);

  MAO_ASSERT(mao_symbol);
  mao_symbol->set_symbol_type(symbol_type);

  DirectiveEntry::OperandVector operands;
  operands.push_back(new DirectiveEntry::Operand(symbol));
  const char *type_name;
  switch (symbol_type) {
    case OBJECT_SYMBOL: type_name = "@object"; break;
    case FUNCTION_SYMBOL: type_name = "@function"; break;
    case NOTYPE_SYMBOL: type_name = "@notype"; break;
    case TLS_SYMBOL: type_name = "@tls_object"; break;
    case COMMON_SYMBOL: type_name = "@common"; break;
    case FILE_SYMBOL: MAO_ASSERT(false); break;
    case SECTION_SYMBOL: MAO_ASSERT(false); break;
    default: MAO_ASSERT(false); break;
  }
  operands.push_back(new DirectiveEntry::Operand(type_name));

  link_directive_tail(DirectiveEntry::TYPE, operands);
}

void link_size(const char *name, unsigned int size, const char *line_verbatim) {
  MAO_ASSERT(name);
  MAO_ASSERT(maounit_);
  SymbolTable *symbol_table = maounit_->GetSymbolTable();
  MAO_ASSERT(symbol_table);
  Symbol *symbol = maounit_->FindOrCreateAndFindSymbol(name);
  MAO_ASSERT(symbol);
  symbol->set_size(size);
  return;
}

void link_file_directive(const char *name, const int *filenum) {
  std::string quoted_name = quote_c_string(name);

  DirectiveEntry::OperandVector operands;
  if (filenum != NULL) {
    operands.push_back(new DirectiveEntry::Operand(*filenum));
  }
  operands.push_back(new DirectiveEntry::Operand(quoted_name));
  link_directive_tail(DirectiveEntry::FILE, operands);
}

void link_global_directive(symbolS *symbol) {
  DirectiveEntry::OperandVector operands;
  operands.push_back(new DirectiveEntry::Operand(symbol));
  link_directive_tail(DirectiveEntry::GLOBAL, operands);
}

void link_local_directive(symbolS *symbol) {
  DirectiveEntry::OperandVector operands;
  operands.push_back(new DirectiveEntry::Operand(symbol));
  link_directive_tail(DirectiveEntry::LOCAL, operands);
}

void link_weak_directive(symbolS *symbol) {
  DirectiveEntry::OperandVector operands;
  operands.push_back(new DirectiveEntry::Operand(symbol));
  link_directive_tail(DirectiveEntry::WEAK, operands);
}

void link_size_directive(symbolS *symbol, expressionS *expr) {
  DirectiveEntry::OperandVector operands;
  operands.push_back(new DirectiveEntry::Operand(symbol));
  operands.push_back(new DirectiveEntry::Operand(expr));
  link_directive_tail(DirectiveEntry::SIZE, operands);
}

void link_dc_directive(int size, int rva, expressionS *expr) {
  DirectiveEntry::OperandVector operands;
  if (reloc_ != _dummy_first_bfd_reloc_code_real) {
    operands.push_back(new DirectiveEntry::Operand(expr, reloc_));
    reloc_ = _dummy_first_bfd_reloc_code_real;
  } else {
    operands.push_back(new DirectiveEntry::Operand(expr));
  }

  DirectiveEntry::Opcode opcode;
  if (rva) {
    MAO_ASSERT(size == 4);
    opcode = DirectiveEntry::RVA;
  } else {
    switch (size) {
      case 1: opcode = DirectiveEntry::BYTE; break;
      case 2: opcode = DirectiveEntry::WORD; break;
      case 4: opcode = DirectiveEntry::LONG; break;
      case 8: opcode = DirectiveEntry::QUAD; break;
      default: MAO_ASSERT(false);
    }
  }
  link_directive_tail(opcode, operands);
}

void link_string_directive(int bitsize, int append_zero,
                           MaoStringPiece value) {
  std::string quoted_value = quote_string_piece(value);

  DirectiveEntry::OperandVector operands;
  operands.push_back(new DirectiveEntry::Operand(quoted_value));

  DirectiveEntry::Opcode opcode;
  if (!append_zero) {
    MAO_ASSERT(bitsize == 8);
    opcode = DirectiveEntry::ASCII;
  } else {
    switch (bitsize) {
      case 8: opcode = DirectiveEntry::STRING8; break;
      case 16: opcode = DirectiveEntry::STRING16; break;
      case 32: opcode = DirectiveEntry::STRING32; break;
      case 64: opcode = DirectiveEntry::STRING64; break;
      default: MAO_ASSERT(false);
    }
  }
  link_directive_tail(opcode, operands);
}

void link_leb128_directive(expressionS *expr, int sign) {
  DirectiveEntry::Opcode opcode = sign ? DirectiveEntry::SLEB128 :
      DirectiveEntry::ULEB128;
  DirectiveEntry::OperandVector operands;
  operands.push_back(new DirectiveEntry::Operand(expr));
  link_directive_tail(opcode, operands);
}

void link_align_directive(int align, int fill_len, int fill, int max) {
  DirectiveEntry::Opcode opcode;

  switch (fill_len) {
    case 0:
    case 1: opcode = DirectiveEntry::P2ALIGN; break;
    case 2: opcode = DirectiveEntry::P2ALIGNW; break;
    case 4: opcode = DirectiveEntry::P2ALIGNL; break;
    default: MAO_ASSERT(false);
  }

  DirectiveEntry::OperandVector operands;
  operands.push_back(new DirectiveEntry::Operand(align));
  if (fill_len)
    operands.push_back(new DirectiveEntry::Operand(fill));
  else
    operands.push_back(new DirectiveEntry::Operand());
  operands.push_back(new DirectiveEntry::Operand(max));
  link_directive_tail(opcode, operands);
}

void link_space_directive(expressionS *size, expressionS *fill, int mult) {
  DirectiveEntry::Opcode opcode;
  switch (mult) {
    case  0: opcode = DirectiveEntry::SPACE; break;
    case  1: opcode = DirectiveEntry::DS_B; break;
    case  2: opcode = DirectiveEntry::DS_W; break;
    case  4: opcode = DirectiveEntry::DS_L; break;
    case  8: opcode = DirectiveEntry::DS_D; break;
    case 12: opcode = DirectiveEntry::DS_X; break;
    default: MAO_ASSERT(false);
  }

  DirectiveEntry::OperandVector operands;
  operands.push_back(new DirectiveEntry::Operand(size));
  operands.push_back(new DirectiveEntry::Operand(fill));
  link_directive_tail(opcode, operands);
}

void register_mao_unit(MaoUnit *maounit) {
  MAO_ASSERT(maounit);
  maounit_ = maounit;
}

void link_ident_directive(MaoStringPiece value) {
  DirectiveEntry::OperandVector operands;
  std::string quoted_value = quote_string_piece(value);
  operands.push_back(new DirectiveEntry::Operand(quoted_value));

  link_directive_tail(DirectiveEntry::IDENT, operands);
}

void link_set_directive(symbolS *symbol, expressionS *expr) {
  DirectiveEntry::OperandVector operands;
  operands.push_back(new DirectiveEntry::Operand(symbol));
  operands.push_back(new DirectiveEntry::Operand(expr));
  link_directive_tail(DirectiveEntry::SET, operands);
}

void link_equiv_directive(symbolS *symbol, expressionS *expr) {
  DirectiveEntry::OperandVector operands;
  operands.push_back(new DirectiveEntry::Operand(symbol));
  operands.push_back(new DirectiveEntry::Operand(expr));
  link_directive_tail(DirectiveEntry::EQUIV, operands);
}

void link_weakref_directive(struct MaoStringPiece alias,
                            struct MaoStringPiece target) {
  // TODO(martint): handle weakref for symboltable
  DirectiveEntry::OperandVector operands;
  operands.push_back(new DirectiveEntry::Operand(alias));
  operands.push_back(new DirectiveEntry::Operand(target));
  link_directive_tail(DirectiveEntry::WEAKREF, operands);
}

void link_arch_directive(struct MaoStringPiece description) {
  DirectiveEntry::OperandVector operands;
  operands.push_back(new DirectiveEntry::Operand(description));
  link_directive_tail(DirectiveEntry::ARCH, operands);
}

void link_linefile_directive(int line_number, struct MaoStringPiece filename,
                             int num_flags, int* flag) {
  DirectiveEntry::OperandVector operands;
  operands.push_back(new DirectiveEntry::Operand(line_number));
  operands.push_back(new DirectiveEntry::Operand(filename));
  for (int i = 0; i < num_flags; i++) {
    operands.push_back(new DirectiveEntry::Operand(flag[i]));
  }
  link_directive_tail(DirectiveEntry::LINEFILE, operands);
}


void link_loc_directive(int file_number, int line_number, int column,
                        struct MaoStringPiece options[], int num_options) {
  DirectiveEntry::OperandVector operands;
  operands.push_back(new DirectiveEntry::Operand(file_number));
  operands.push_back(new DirectiveEntry::Operand(line_number));
  operands.push_back(new DirectiveEntry::Operand(column));
  for (int i = 0; i < num_options; ++i) {
    operands.push_back(new DirectiveEntry::Operand(options[i]));
  }
  link_directive_tail(DirectiveEntry::LOC, operands);
}

void link_allow_index_reg_directive() {
  DirectiveEntry::OperandVector operands;
  link_directive_tail(DirectiveEntry::ALLOW_INDEX_REG, operands);
}

void link_disallow_index_reg_directive() {
  DirectiveEntry::OperandVector operands;
  link_directive_tail(DirectiveEntry::DISALLOW_INDEX_REG, operands);
}

void link_org_directive(expressionS *expr, int fill) {
  DirectiveEntry::OperandVector operands;
  operands.push_back(new DirectiveEntry::Operand(expr));
  operands.push_back(new DirectiveEntry::Operand(fill));
  link_directive_tail(DirectiveEntry::ORG, operands);
}

void link_float_directive(int float_type, struct MaoStringPiece value) {
  DirectiveEntry::OperandVector operands;
  operands.push_back(new DirectiveEntry::Operand(value));
  DirectiveEntry::Opcode opcode;
  switch (float_type) {
    case 'd': opcode = DirectiveEntry::DC_D; break;
    case 'f': opcode = DirectiveEntry::DC_S; break;
    case 'x': opcode = DirectiveEntry::DC_X; break;
    default: MAO_ASSERT(false);
  }
  link_directive_tail(opcode, operands);
}

void link_code_directive(int flag_code, char gcc) {
  DirectiveEntry::OperandVector operands;
  switch (flag_code) {
    case CODE_16BIT:
      if (gcc)
        link_directive_tail(DirectiveEntry::CODE16GCC, operands);
      else
        link_directive_tail(DirectiveEntry::CODE16, operands);
      break;
    case CODE_32BIT:
      MAO_ASSERT(!gcc);
      link_directive_tail(DirectiveEntry::CODE32, operands);
      break;
    case CODE_64BIT:
      MAO_ASSERT(!gcc);
      link_directive_tail(DirectiveEntry::CODE64, operands);
      break;
    default:
      MAO_ASSERT_MSG(false, "Unknown code-mode.");
      break;
  }
}

void link_popsection_directive() {
  struct link_context_s link_context = get_link_context();
  maounit_->PopSubSection(link_context.line_number);
}


void link_previous_directive() {
  struct link_context_s link_context = get_link_context();
  maounit_->SetPreviousSubSection(link_context.line_number);
}

// This code makes it possible to catch relocations
// found in cons directives (.long, .byte etc). The relocation is
// parsed in the machine dependent code (tc-i386.h) and not visible
// in read.c where we link the directive itself. To solve this, link_cons_reloc
// is called from tc-i386.c before link_dc_directive is called in read.c. This
// way we can check in link_dc_directive if we should include a relocation.
void link_cons_reloc(enum bfd_reloc_code_real reloc) {
  reloc_ = reloc;
}
