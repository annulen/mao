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
// Foundation, Inc., 51 Franklin Street, 5th Floor, Boston, MA 02110-1301, USA.

// Bitmasks for operands

#define  DEF_OP0      (1 << 0)
#define  DEF_OP1      (1 << 1)
#define  DEF_OP2      (1 << 2)
#define  DEF_OP3      (1 << 3)
#define  DEF_OP4      (1 << 4)
#define  DEF_OP5      (1 << 5)

// Bitmasks for registers

// 8-bit
#define   REG_AL      (1 <<  0)
#define   REG_CL      (1 <<  1)
#define   REG_DL      (1 <<  2)
#define   REG_BL      (1 <<  3)
#define   REG_AH      (1 <<  4)
#define   REG_CH      (1 <<  5)
#define   REG_DH      (1 <<  6)
#define   REG_BH      (1 <<  7)

// 16-bit
#define   REG_AX      (1 <<  8)
#define   REG_CX      (1 <<  9)
#define   REG_DX      (1 << 10)
#define   REG_BX      (1 << 11)
#define   REG_SP      (1 << 12)
#define   REG_BP      (1 << 13)
#define   REG_SI      (1 << 14)
#define   REG_DI      (1 << 15)

// 32-bit
#define   REG_EAX     (1 << 16)
#define   REG_ECX     (1 << 17)
#define   REG_EDX     (1 << 18)
#define   REG_EBX     (1 << 19)
#define   REG_ESP     (1 << 20)
#define   REG_EBP     (1 << 21)
#define   REG_ESI     (1 << 22)
#define   REG_EDI     (1 << 23)

// 64-bit
#define   REG_RAX     (1 << 24)
#define   REG_RCX     (1 << 25)
#define   REG_RDX     (1 << 26)
#define   REG_RBX     (1 << 27)
#define   REG_RSP     (1 << 28)
#define   REG_RBP     (1 << 29)
#define   REG_RSI     (1 << 30)
#define   REG_RDI     (1 << 31)

#define   REG_R8      (1LL << 32)
#define   REG_R9      (1LL << 33)
#define   REG_R10     (1LL << 34)
#define   REG_R11     (1LL << 35)
#define   REG_R12     (1LL << 36)
#define   REG_R13     (1LL << 37)
#define   REG_R14     (1LL << 38)
#define   REG_R15     (1LL << 39)

#define   REG_ALL     (-1LL)
// more are possible...
