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

// Generate enums and a hashtable for x86 instructions.
// Usage: GenOpcodes instruction-table
//
// Instruction table is something like:
//    binutils-2.19/opcodes/i386-opc.tbl
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include <map>
#include <list>
#include <algorithm>

#include "opcodes/i386-opc.h"
#include "MaoDefs.h"

struct ltstr {
  bool operator()(const char* s1, const char* s2) const {
    return strcmp(s1, s2) < 0;
  }
};

static bool emit_warnings = false;

// Note: The following few helper routines, as
//       well as the main loop in main(), are directly
//       copied from the i386-gen.c sources in
//       binutils-2.19/opcodes/...
//

static char *
remove_leading_whitespaces(char *str) {
  while (isspace(*str))
    str++;
  return str;
}

/* Remove trailing white spaces.  */

static void
remove_trailing_whitespaces(char *str) {
  size_t last = strlen(str);

  if (last == 0)
    return;

  do {
    last--;
    if (isspace(str[last]))
      str[last] = '\0';
    else
      break;
  } while (last != 0);
}


static char *
next_field(char *str, char sep, char **next) {
  char *p;

  p = remove_leading_whitespaces(str);
  for (str = p; *str != sep && *str != '\0'; str++);

  *str = '\0';
  remove_trailing_whitespaces(p);

  *next = str + 1;

  return p;
}

void usage(char *const argv[]) __attribute__ ((noreturn));

void usage(char *const argv[]) {
  fprintf(stderr, "USAGE:\n"
          "  %s [-p outputpath] table-file sideeffect-file \n\n", argv[0]);
  fprintf(stderr,
          "Creates headerfiles in in directory outputpath, "
          "defaults to current path\n");
  exit(1);
}

class GenDefEntry {
 public:
  GenDefEntry(char *op_str) :
    op_str_(op_str) {
    flags.CF = flags.PF = flags.AF = flags.ZF = flags.SF =
    flags.TP = flags.IF = flags.DF = flags.OF = flags.IOPL =
    flags.NT = flags.RF = flags.VM = flags.AC = flags.VIF =
    flags.VIP = flags.ID = false;
    found = false;
  }
  char *op_str() { return op_str_; }
  unsigned int       op_mask;
  BitString reg_mask;
  BitString reg_mask8;
  BitString reg_mask16;
  BitString reg_mask32;
  BitString reg_mask64;
  struct flags_ {
    bool  CF :1;
    bool  PF :1;
    bool  AF :1;
    bool  ZF :1;
    bool  SF :1;
    bool  TP :1;
    bool  IF :1;
    bool  DF :1;
    bool  OF :1;
    bool  IOPL :1;
    bool  NT :1;
    bool  RF :1;
    bool  VM :1;
    bool  AC :1;
    bool  VIF:1;
    bool  VIP :1;
    bool  ID :1;
  } flags;
  bool found :1;
  char *op_str_;
};

typedef std::map<char *, GenDefEntry *, ltstr> MnemMap;
static MnemMap mnem_map;

class RegEntry {
 public:
  RegEntry(char *name, int num) :
    name_(strdup(name)), num_(num) {}
  char *name_;
  int   num_;
};

typedef std::list<RegEntry *> RegList;
static RegList reg_list;

void ReadRegisterTable(const char *op_table) {
  char buff[1024];
  strcpy(buff, op_table);
  char *p = strrchr(buff, '/');
  strcpy(p+1, "i386-reg.tbl");

  FILE *regs = fopen(buff, "r");
  if (!regs) {
    fprintf(stderr, "Cannot open register table file: %s\n", buff);
    exit(1);
  }

  int count = 0;
  while (!feof(regs)) {
    char *p = fgets(buff, 1024, regs);
    if (!p) break;

    if (buff[0] == '/' && buff[1] == '/') continue;
    if (buff[0] == '#' || buff[0] == '\n') continue;
    char *reg = next_field(buff, ',', &p);

    RegEntry *r = new RegEntry(reg, count++);
    reg_list.push_back(r);
  }

  fclose(regs);
}

// linear search through list to find a register by name
//
RegEntry *FindRegister(const char *str) {
  for (RegList::iterator it = reg_list.begin();
       it != reg_list.end(); ++it) {
    if (!strcasecmp((*it)->name_, str))
      return *it;
  }
  return NULL;
}


void ReadSideEffects(const char *fname) {
  char buff[1024];

  FILE *f = fopen(fname, "r");
  if (!f) {
    fprintf(stderr, "Cannot open side-effect table: %s\n", fname);
    exit(1);
  }
  while (!feof(f)) {
    char *p = fgets(buff, 1024, f);
    if (!p) break;
    char *end = p + strlen(p);

    if (buff[0] == '/' && buff[1] == '/') continue;
    if (buff[0] == '#' || buff[0] == '\n') continue;
    char *mnem = next_field(buff, ' ', &p);

    GenDefEntry *e = new GenDefEntry(strdup(mnem));
    mnem_map[strdup(mnem)] = e;

    BitString *mask = &e->reg_mask;

    while (p && *p && (p < end)) {
      char *q = next_field(p, ' ', &p);
      if (!*q) break;
      if (!strcasecmp(q, "all:"))    mask = &e->reg_mask; else
      if (!strcasecmp(q, "addr8:"))  mask = &e->reg_mask8; else
      if (!strcasecmp(q, "addr16:")) mask = &e->reg_mask16; else
      if (!strcasecmp(q, "addr32:")) mask = &e->reg_mask32; else
      if (!strcasecmp(q, "addr64:")) mask = &e->reg_mask64; else

      if (!strcasecmp(q, "flags:"))  /* TODO(rhundt): refine */; else
      if (!strcasecmp(q, "clear:"))  /* TODO(rhundt): refine */; else
      if (!strcasecmp(q, "undef:"))  /* TODO(rhundt): refine */; else

      if (!strcasecmp(q, "CF")) e->flags.CF = true; else
      if (!strcasecmp(q, "PF")) e->flags.PF = true; else
      if (!strcasecmp(q, "AF")) e->flags.AF = true; else
      if (!strcasecmp(q, "ZF")) e->flags.ZF = true; else
      if (!strcasecmp(q, "SF")) e->flags.SF = true; else
      if (!strcasecmp(q, "TP")) e->flags.TP = true; else
      if (!strcasecmp(q, "IF")) e->flags.IF = true; else
      if (!strcasecmp(q, "DF")) e->flags.DF = true; else
      if (!strcasecmp(q, "OF")) e->flags.OF = true; else
      if (!strcasecmp(q, "IOPL")) e->flags.IOPL = true; else
      if (!strcasecmp(q, "NT")) e->flags.NT = true; else
      if (!strcasecmp(q, "RF")) e->flags.RF = true; else
      if (!strcasecmp(q, "VM")) e->flags.VM = true; else
      if (!strcasecmp(q, "AC")) e->flags.AC = true; else
      if (!strcasecmp(q, "VIF")) e->flags.VIF = true; else
      if (!strcasecmp(q, "VIP")) e->flags.VIP = true; else
      if (!strcasecmp(q, "ID")) e->flags.ID = true; else

      if (!strcasecmp(q, "op0"))  e->op_mask |= DEF_OP0; else
      if (!strcasecmp(q, "src"))  e->op_mask |= DEF_OP0; else

      if (!strcasecmp(q, "op1"))  e->op_mask |= DEF_OP1; else
      if (!strcasecmp(q, "dest")) e->op_mask |= DEF_OP1; else

      if (!strcasecmp(q, "op2"))  e->op_mask |= DEF_OP2; else
      if (!strcasecmp(q, "op3"))  e->op_mask |= DEF_OP3; else
      if (!strcasecmp(q, "op4"))  e->op_mask |= DEF_OP4; else
      if (!strcasecmp(q, "op5"))  e->op_mask |= DEF_OP5; else {
        RegEntry *r = FindRegister(q);
        if (r)
          mask->Set(r->num_);
        else {
          fprintf(stderr, "Unknown string: %s <%s>\n", q, buff);
          exit(1);
        }
      }
      if (p > end) break;
    }
  }

  fclose(f);
}

static void PrintRegMask(FILE *def, BitString &mask) {
  mask.PrintInitializer(def);
}

int fail_on_open(char *const argv[], const char *filename)
    __attribute__ ((noreturn));

int fail_on_open(char *const argv[], const char *filename) {
  fprintf(stderr, "Cannot open output file: %s\n", filename);
  usage(argv);
  exit(1);
}

int main(int argc, char *const argv[]) {
  char buf[2048];
  int  lineno = 0;
  char lastname[2048];
  char sanitized_name[2048];

  // Options processing
  //
  if (argc < 3)
    usage(argv);

  const char *out_path = NULL; // Default to current directory.
  opterr = 0;

  int c;
  while ((c = getopt (argc, argv, "p:w")) != -1) {
    switch (c) {
      case 'w':
        emit_warnings = true;
        break;
      case 'p':
        out_path = optarg;
        break;
      case '?':
        if (optopt == 'p')
          fprintf (stderr, "Option -%c requires an argument.\n", optopt);
        else if (isprint (optopt))
          fprintf (stderr, "Unknown option `-%c'.\n", optopt);
        else
          fprintf (stderr,
                   "Unknown option character `\\x%x'.\n",
                   optopt);
        return 1;
      default:
        abort ();
    }
  }

  // there shoule be two more arguments
  if ((argc - optind) != 2)
    usage(argv);


  const char *op_table = argv[optind];
  const char *side_effect_table = argv[optind+1];

  // Get tables and data
  //
  ReadRegisterTable(op_table);
  ReadSideEffects(side_effect_table);

  // Open the various input and output files,
  // emit top portion in generated files.
  //
  FILE *in = fopen(op_table, "r");
  if (!in) {
    fprintf(stderr, "Cannot open table file: %s\n", argv[1]);
    usage(argv);
  }

  const char *out_filename;
  const char *table_filename;
  const char *defs_filename;

  if (out_path != NULL) {
    char out_filename_buf[2048];
    char table_filename_buf[2048];
    char defs_filename_buf[2048];

    sprintf(out_filename_buf, "%s%s",   out_path, "/gen-opcodes.h");
    out_filename = out_filename_buf;
    sprintf(table_filename_buf, "%s%s", out_path, "/gen-opcodes-table.h");
    table_filename = table_filename_buf;
    sprintf(defs_filename_buf, "%s%s",  out_path, "/gen-defs.h");
    defs_filename = defs_filename_buf;
  } else {
    out_filename   = "gen-opcodes.h";
    table_filename = "gen-opcodes-table.h";
    defs_filename  = "gen-defs.h";
  }
  FILE *out, *table, *def;

  (out   = fopen(out_filename,   "w")) || fail_on_open(argv, out_filename);
  (table = fopen(table_filename, "w")) || fail_on_open(argv, table_filename);
  (def   = fopen(defs_filename,  "w")) || fail_on_open(argv, defs_filename);

  fprintf(out,
          "// DO NOT EDIT - this file is automatically "
          "generated by GenOpcodes\n//\n\n"
          "typedef enum MaoOpcode {\n"
          "  OP_invalid,\n");

  fprintf(table,
          "// DO NOT EDIT - this file is automatically "
          "generated by GenOpcodes\n//\n\n"
          "#include \"gen-opcodes.h\"\n\n"
          "struct MaoOpcodeTable_ {\n"
          "   MaoOpcode    opcode;\n"
          "   const char  *name;\n"
          "} MaoOpcodeTable[] = {\n"
          "  { OP_invalid, \"invalid\" },\n");

  fprintf(def,
          "// DO NOT EDIT - this file is automatically "
          "generated by GenOpcodes\n//\n"
          "#define BNULL BitString(0x0ull, 0x0ull, 0x0ull, 0x0ull)\n"
          "#define BALL  BitString(-1ull, -1ull, -1ull, -1ull)\n"
          "DefEntry def_entries [] = {\n"
          "  { OP_invalid, 0, BNULL, BNULL, BNULL, BNULL, BNULL },\n" );

  // Read through the instruction description file, isolate the first
  // field, which contains the opcode, and generate an
  //   OP_... into the gen-opcodes.h file.
  //
  // Also, lookup side effects, and generate an entry in the side
  // effect tables in
  //    gen-defs.h
  //
  while (!feof(in)) {
    char *p, *str, *last, *name;
    if (fgets (buf, sizeof (buf), in) == NULL)
      break;

    lineno++;

    p = remove_leading_whitespaces(buf);

    /* Skip comments.  */
    str = strstr(p, "//");
    if (str != NULL)
      str[0] = '\0';

    /* Remove trailing white spaces.  */
    remove_trailing_whitespaces(p);

    switch (p[0]) {
      case '#':
        fprintf(out, "%s\n", p);
      case '\0':
        continue;
        break;
      default:
        break;
    }

    last = p + strlen(p);

    /* Find name.  */
    name = next_field(p, ',', &str);

    /* sanitize name */
    int len = strlen(name);
    for (int i = 0; i < len+1; i++)
      if (name[i] == '.' || name[i] == '-')
        sanitized_name[i] = '_';
      else
        sanitized_name[i] = name[i];


    /* compare and emit */
    if (strcmp(name, lastname)) {
      fprintf(out, "  OP_%s,\n", sanitized_name);
      fprintf(table, "  { OP_%s, \t\"%s\" },\n", sanitized_name, name);

      /* Emit def entry */
      MnemMap::iterator it = mnem_map.find(sanitized_name);
      if (it == mnem_map.end()) {
        fprintf(def, "  { OP_%s, DEF_OP_ALL, BALL, BALL, BALL, BALL, BALL },\n", sanitized_name);
        if (emit_warnings)
          fprintf(stderr, "Warning: No side-effects for: %s\n", sanitized_name);
      } else {
        fprintf(def, "  { OP_%s, 0", sanitized_name);
        GenDefEntry *e = (*it).second;
        e->found = true;
        if (e->op_mask & DEF_OP0) fprintf(def, " | DEF_OP0");
        if (e->op_mask & DEF_OP1) fprintf(def, " | DEF_OP1");
        if (e->op_mask & DEF_OP2) fprintf(def, " | DEF_OP2");
        if (e->op_mask & DEF_OP3) fprintf(def, " | DEF_OP3");
        if (e->op_mask & DEF_OP4) fprintf(def, " | DEF_OP4");
        if (e->op_mask & DEF_OP5) fprintf(def, " | DEF_OP5");

        // populate reg_mask
        fprintf(def, ", ");
        PrintRegMask(def, e->reg_mask);
        fprintf(def, ", ");
        PrintRegMask(def, e->reg_mask8);
        fprintf(def, ", ");
        PrintRegMask(def, e->reg_mask16);
        fprintf(def, ", ");
        PrintRegMask(def, e->reg_mask32);
        fprintf(def, ", ");
        PrintRegMask(def, e->reg_mask64);
        fprintf(def, " },\n");
      }
    }
    strcpy(lastname, name);
  }

  // quality check - see which DefEntries remain unused
  //
  if (emit_warnings)
    for (MnemMap::iterator it = mnem_map.begin();
         it != mnem_map.end(); ++it) {
      if (!(*it).second->found)
        fprintf(stderr, "Warning: Unused side-effects description: %s\n",
                (*it).second->op_str());
    }

  fprintf(out, "};  // MaoOpcode\n\n"
          "MaoOpcode GetOpcode(const char *opcode);\n");

  fprintf(table, "  { OP_invalid, 0 }\n");
  fprintf(table, "};\n");

  fprintf(def, "};\n"
          "const unsigned int def_entries_size = "
          "sizeof(def_entries) / sizeof(DefEntry);\n"
          );

  fclose(in);
  fclose(out);
  fclose(table);
  fclose(def);

  return 0;
}
