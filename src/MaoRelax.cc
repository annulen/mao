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


#include <stdio.h>
#include <stdlib.h>

#include "MaoRelax.h"
#include "tc-i386-helper.h"

/* TODO(nvachhar): Unparsed directives that end fragments:
   s_fill <- ".fill"
   do_org <- s_org <- ".org"
          <- assign_symbol <- s_set <- ".equ", ".equiv", ".eqv", ".set"
                           <- equals <- read_a_source_file <- ??
   bss_alloc <- ??
   do_align <- read_a_source_file <- ??
*/

extern "C" {
  extern bfd *stdoutput;
  int relax_segment(struct frag *segment_frag_root, void *segT, int pass);
  void convert_to_bignum(expressionS *exp);
  int sizeof_leb128(valueT value, int sign);
  int output_big_leb128 (char *p, LITTLENUM_TYPE *bignum, int size, int sign);
}

void MaoRelaxer::Relax() {
  struct frag *fragments = BuildFragments();
  asection *section = bfd_get_section_by_name(stdoutput, section_->name());
  for (int change = 1, pass = 0; change; pass++)
    change = relax_segment(fragments, section, pass);

  FreeFragments(fragments);
}

struct frag *MaoRelaxer::BuildFragments() {
  struct frag *fragments, *frag;
  fragments = frag = NewFragment();

  bool is_text = !strcmp(section_->name(), ".text");

  for (SectionEntryIterator iter = section_->EntryBegin(mao_);
       iter != section_->EntryEnd(mao_); ++iter) {
    MaoUnitEntryBase *entry = *iter;
    switch (entry->Type()) {
      case MaoUnitEntryBase::INSTRUCTION: {
        InstructionEntry *ientry = static_cast<InstructionEntry*>(entry);
        X86InstructionSizeHelper size_helper(
            ientry->instruction()->instruction());
        std::pair<int, bool> size_pair = size_helper.SizeOfInstruction();
        frag->fr_fix += size_pair.first;

        if (size_pair.second)
          frag = EndFragmentInstruction(ientry, frag, true);

        break;
      }
      case MaoUnitEntryBase::DIRECTIVE: {
        DirectiveEntry *dentry = static_cast<DirectiveEntry*>(entry);
        switch (dentry->op()) {
          case DirectiveEntry::P2ALIGN:
          case DirectiveEntry::P2ALIGNW:
          case DirectiveEntry::P2ALIGNL: {
            MAO_ASSERT(dentry->NumOperands() == 3);
            const DirectiveEntry::Operand *alignment = dentry->GetOperand(0);
            const DirectiveEntry::Operand *max = dentry->GetOperand(2);

            MAO_ASSERT(alignment->type == DirectiveEntry::INT);
            MAO_ASSERT(max->type == DirectiveEntry::INT);

            frag = EndFragmentAlign(is_text, alignment->data.i,
                                    max->data.i, frag, true);
            break;
          }
          case DirectiveEntry::SLEB128:
          case DirectiveEntry::ULEB128: {
            bool is_signed = dentry->op() == DirectiveEntry::SLEB128;
            MAO_ASSERT(dentry->NumOperands() == 1);
            const DirectiveEntry::Operand *value = dentry->GetOperand(0);
            MAO_ASSERT(value->type == DirectiveEntry::EXPRESSION);
            expressionS *expr = value->data.expr;

            if (expr->X_op == O_constant && is_signed &&
                (expr->X_add_number < 0) != !expr->X_unsigned) {
              // TODO(nvachhar): Should we assert instead of changing the IR?
              // We're outputting a signed leb128 and the sign of X_add_number
              // doesn't reflect the sign of the original value.  Convert EXP
              // to a correctly-extended bignum instead.
              convert_to_bignum (expr);
            }

            if (expr->X_op == O_constant) {
              // If we've got a constant, compute its size right now
              int size =
                  sizeof_leb128 (expr->X_add_number, is_signed ? 1 : 0);
              frag->fr_fix += size;
            } else if (expr->X_op == O_big) {
              // O_big is a different sort of constant.
              int size =
                  output_big_leb128 (NULL, generic_bignum,
                                     expr->X_add_number, is_signed ? 1 : 0);
              frag->fr_fix += size;
            } else
              // Otherwise, end the fragment
              frag = EndFragmentLeb128(value, is_signed, frag, true);
            break;
          }
          case DirectiveEntry::BYTE:
            frag->fr_fix++;
            break;
          case DirectiveEntry::WORD:
            frag->fr_fix += 2;
            break;
          case DirectiveEntry::RVA:
          case DirectiveEntry::LONG:
            frag->fr_fix += 4;
            break;
          case DirectiveEntry::QUAD:
            frag->fr_fix += 8;
            break;
          case DirectiveEntry::ASCII:
            frag->fr_fix += StringSize(dentry, 1, false);
            break;
          case DirectiveEntry::STRING8:
            frag->fr_fix += StringSize(dentry, 1, true);
            break;
          case DirectiveEntry::STRING16:
            frag->fr_fix += StringSize(dentry, 2, true);
            break;
          case DirectiveEntry::STRING32:
            frag->fr_fix += StringSize(dentry, 4, true);
            break;
          case DirectiveEntry::STRING64:
            frag->fr_fix += StringSize(dentry, 8, true);
            break;
          case DirectiveEntry::SPACE:
            frag = HandleSpace(dentry, 0, frag, true);
            break;
          case DirectiveEntry::DS_B:
            frag = HandleSpace(dentry, 1, frag, true);
            break;
          case DirectiveEntry::DS_W:
            frag = HandleSpace(dentry, 2, frag, true);
            break;
          case DirectiveEntry::DS_L:
            frag = HandleSpace(dentry, 4, frag, true);
            break;
          case DirectiveEntry::DS_D:
            frag = HandleSpace(dentry, 8, frag, true);
            break;
          case DirectiveEntry::DS_X:
            frag = HandleSpace(dentry, 12, frag, true);
            break;
          case DirectiveEntry::FILE:
          case DirectiveEntry::SECTION:
          case DirectiveEntry::GLOBAL:
          case DirectiveEntry::LOCAL:
          case DirectiveEntry::WEAK:
          case DirectiveEntry::TYPE:
          case DirectiveEntry::SIZE:
          case DirectiveEntry::NUM_OPCODES:
            // Nothing to do
            break;
        }
        break;
      }
      case MaoUnitEntryBase::LABEL:
      case MaoUnitEntryBase::DEBUG:
        // Nothing to do
        break;
      case MaoUnitEntryBase::UNDEFINED:
      default:
        MAO_ASSERT(0);
    }
  }

  EndFragmentAlign(is_text, 0, 0, frag, false);

  return fragments;
}


struct frag *MaoRelaxer::EndFragmentInstruction(InstructionEntry *entry,
                                                struct frag *frag,
                                                bool new_frag) {
/* Types.  */
#define UNCOND_JUMP 0
#define COND_JUMP 1
#define COND_JUMP86 2

/* Sizes.  */
#define CODE16	1
#define SMALL	0
#define SMALL16 (SMALL | CODE16)
#define BIG	2
#define BIG16	(BIG | CODE16)

#define ENCODE_RELAX_STATE(type, size) \
  ((relax_substateT) (((type) << 2) | (size)))

  i386_insn *insn = entry->instruction()->instruction();

  // Only jumps should end fragments
  MAO_ASSERT (insn->tm.opcode_modifier.jump);

  int code16 = 0;
  if (flag_code == CODE_16BIT)
    code16 = CODE16;

  if (insn->prefix[X86InstructionSizeHelper::DATA_PREFIX] != 0)
    code16 ^= CODE16;

  relax_substateT subtype;
  if (insn->tm.base_opcode == JUMP_PC_RELATIVE)
    subtype = ENCODE_RELAX_STATE (UNCOND_JUMP, SMALL);
  else if (cpu_arch_flags.bitfield.cpui386)
    subtype = ENCODE_RELAX_STATE (COND_JUMP, SMALL);
  else
    subtype = ENCODE_RELAX_STATE (COND_JUMP86, SMALL);
  subtype |= code16;

  symbolS *sym = insn->op[0].disps->X_add_symbol;
  offsetT off = insn->op[0].disps->X_add_number;

  if (insn->op[0].disps->X_op != O_constant &&
      insn->op[0].disps->X_op != O_symbol) {
    /* Handle complex expressions.  */
    sym = make_expr_symbol (insn->op[0].disps);
    off = 0;
  }

  return FragVar(rs_machine_dependent, insn->reloc[0],
                 subtype, sym, off,
                 reinterpret_cast<char*>(&insn->tm.base_opcode),
                 frag, new_frag);

#undef UNCOND_JUMP
#undef COND_JUMP
#undef COND_JUMP86

#undef CODE16
#undef SMALL
#undef SMALL16
#undef BIG
#undef BIG16

#undef ENCODE_RELAX_STATE
}


struct frag *MaoRelaxer::EndFragmentAlign(bool code,
                                          int alignment, int max,
                                          struct frag *frag,
                                          bool new_frag) {
  relax_stateT state = code ? rs_align_code : rs_align_code;
  return FragVar(state, 1,
                 static_cast<relax_substateT>(max), NULL,
                 static_cast<offsetT>(alignment), NULL,
                 frag, new_frag);
}

struct frag *MaoRelaxer::EndFragmentLeb128(
    const DirectiveEntry::Operand *value,
    bool is_signed,
    struct frag *frag,
    bool new_frag) {
  // TODO(nvachhar): Ugh... we have to create a symbol
  // here to store in the fragment.  This means each
  // execution of relaxation allocates memory that will
  // never be freed.  Let's hope relaxation doesn't run
  // too often.
  return FragVar(rs_leb128, 0,
                 static_cast<relax_substateT>(is_signed),
                 make_expr_symbol(value->data.expr),
                 static_cast<offsetT>(0), NULL,
                 frag, new_frag);
}

int MaoRelaxer::StringSize(DirectiveEntry *entry, int multiplier,
                           bool null_terminate) {
  MAO_ASSERT(entry->NumOperands() == 1);
  const DirectiveEntry::Operand *value = entry->GetOperand(0);
  MAO_ASSERT(value->type == DirectiveEntry::STRING);

  // Subtract 2 for the quotes, add the null terminator if necessary
  // and then multiply by the character size.
  return multiplier * ((value->data.str->length() - 2) +
                       (null_terminate ? 1 : 0));
}

struct frag *MaoRelaxer::HandleSpace(DirectiveEntry *entry,
                                     int mult,
                                     struct frag *frag,
                                     bool new_frag) {
  MAO_ASSERT(entry->NumOperands() == 2);
  const DirectiveEntry::Operand *size_opnd = entry->GetOperand(0);
  MAO_ASSERT(size_opnd->type == DirectiveEntry::EXPRESSION);
  expressionS *size = size_opnd->data.expr;

  if (size->X_op == O_constant) {
    int increment = size->X_add_number * (mult ? mult : 1);
    MAO_ASSERT(increment > 0);
    frag->fr_fix += increment;
  } else {
    MAO_ASSERT(mult == 0 || mult == 1);
    // TODO(nvachhar): Ugh... we have to create a symbol
    // here to store in the fragment.  This means each
    // execution of relaxation allocates memory that will
    // never be freed.  Let's hope relaxation doesn't run
    // too often.
    frag = FragVar(rs_space, 1, (relax_substateT) 0, make_expr_symbol (size),
                   (offsetT) 0, NULL, frag, new_frag);
  }

  return frag;
}

struct frag *MaoRelaxer::FragVar(relax_stateT type, int var,
                                 relax_substateT subtype,
                                 symbolS *symbol, offsetT offset,
                                 char *opcode, struct frag *frag,
                                 bool new_frag) {
  frag->fr_var = var;
  frag->fr_type = type;
  frag->fr_subtype = subtype;
  frag->fr_symbol = symbol;
  frag->fr_offset = offset;
  frag->fr_opcode = opcode;
  FragInitOther(frag);

  if (new_frag)
    frag->fr_next = NewFragment();
  return frag->fr_next;
}


void MaoRelaxer::FragInitOther(struct frag *frag) {
#ifdef USING_CGEN
  frag->fr_cgen.insn = 0;
  frag->fr_cgen.opindex = 0;
  frag->fr_cgen.opinfo = 0;
#endif
#ifdef TC_FRAG_INIT
  TC_FRAG_INIT (frag);
#endif
}
