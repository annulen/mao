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
#include <list>
#include <vector>
#include <map>

#include "MaoDebug.h"
#include "MaoLoops.h"
#include "MaoUnit.h"
#include "MaoPasses.h"
#include "MaoRelax.h"

// Pass that finds paths in inner loops that fits in 4 16-byte lines
// and alligns all (chains of) basicb locks within the paths.
class LoopAlignPass : public MaoFunctionPass {
 public:
  LoopAlignPass(MaoOptionMap *options, MaoUnit *mao, Function *function);
  bool Go();

 private:
  // Path is a list of basic blocks that form a way through the loop
//   typedef std::vector<BasicBlock *> Path;
//   typedef std::vector<Path *> Paths;

  // This is the result found by the loop sequence detector.
  LoopStructureGraph *loop_graph_;
  // This is the sizes of all the instructions in the section
  // as found by the relaxer.
  MaoEntryIntMap *sizes_;

  // TODO(martint): find a useful parameter to pass into the pass
  int maximum_loop_size_;

  // Collect stat during the pass?
  bool collect_stat_;

  class LoopAlignStat : public Stat {
   public:
    LoopAlignStat() : number_of_inner_loops_(0), number_of_aligned_loops_(0) {
      inner_loop_size_distribution_.clear();
    }

    ~LoopAlignStat() {;}

    void FoundInnerLoop(int size, bool aligned) {
      ++number_of_inner_loops_;
      if (aligned) {
        ++number_of_aligned_loops_;
      }
      ++inner_loop_size_distribution_[size];
    }

    virtual void Print(FILE *out) {
      fprintf(out, "Loop Alignment distribution\n");
      fprintf(out, "  # Inner   loops : %d\n",
              number_of_inner_loops_);
      fprintf(out, "  # Aligned loops : %d\n",
              number_of_aligned_loops_);
      fprintf(out, "  # Alignment possibilities : %.2f%%\n",
              100 * static_cast<double>(number_of_aligned_loops_) /
              number_of_inner_loops_);
      // iterate over distribution
      fprintf(out, "   Size : # loops\n");
      for (std::map<int, int>::const_iterator iter =
               inner_loop_size_distribution_.begin();
           iter != inner_loop_size_distribution_.end();
           ++iter) {
        fprintf(out, "   %4d : %4d\n", iter->first, iter->second);
      }
    }

   private:
    int                number_of_inner_loops_;
    std::map<int, int> inner_loop_size_distribution_;
    int                number_of_aligned_loops_;
  };

  LoopAlignStat *loop_align_stat_;

  // Returns the size of the entries in the loop.
  int LoopSize(const SimpleLoop *loop) const;
  // Returns the sizes of the entries in the path.
//   int PathSize(const Path *path) const;

  // Used to find inner loops
  void FindInner(const SimpleLoop *loop, MaoEntryIntMap *offsets);

//   // Get all the possible paths found in a loop. Return the paths in the
//   // paths variable.
//   void GetPaths(const SimpleLoop *loop, Paths *paths);

//   // Helper function for GetPaths
//   void GetPathsImp(const SimpleLoop *loop, BasicBlock *current,
//                     Paths *paths, Path *path);

//   // Adds a path to the paths collection. The path given is copied.
//   void AddPath(Paths *paths, const Path *path);
//   // Free all the memory allocated for paths.
//   void FreePaths(Paths *paths);

//   // Get the number of 16-byte lines needed to store the path.
//   int NumLines(const Path *path, std::map<BasicBlock *, int> *chain_sizes);

//   // Once a path is identified, align it if it fits in the lsd.
//   void ProcessPath(Path *path);

  // Return the size of the basic block.
  int BasicBlockSize(BasicBlock *BB) const;

//   // A chain is a set of basic blocks that are stored next to eachother.
//   // Given the first basic block in the chain, return the size of the whole
//   // chain.
//   int ChainSize(BasicBlock *basicblock,
//                 std::map<BasicBlock *, BasicBlock *> *connections);
  // Try to align the paths in a given inner loop.
  void ProcessInnerLoop(const SimpleLoop *loop,
                        MaoEntryIntMap   *offsets);
//   void ProcessInnerLoopOld(const SimpleLoop *loop);
};


// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
MAO_OPTIONS_DEFINE(LOOPALIGN, 2) {
  OPTION_INT("loop_size", 64, "Maximum size for loops to be "
                              "considered for alignment."),
  OPTION_BOOL("stat", false, "Collect and print(trace) "
                             "statistics about loops."),
};

LoopAlignPass::LoopAlignPass(MaoOptionMap *options, MaoUnit *mao,
                             Function *function)
    : MaoFunctionPass("LOOPALIGN", options, mao, function), sizes_(NULL) {

  maximum_loop_size_ = GetOptionInt("loop_size");
  collect_stat_      = GetOptionBool("stat");

  if (collect_stat_) {
    // check if a stat object already exists?
    if (unit_->GetStats()->HasStat("LOOPALIGN")) {
      loop_align_stat_ = static_cast<LoopAlignStat *>(
          unit_->GetStats()->GetStat("LOOPALIGN"));
    } else {
      loop_align_stat_ = new LoopAlignStat();
      unit_->GetStats()->Add("LOOPALIGN", loop_align_stat_);
    }
  }
}

bool LoopAlignPass::Go() {
  sizes_ = MaoRelaxer::GetSizeMap(unit_, function_->GetSection());
  loop_graph_ =  LoopStructureGraph::GetLSG(unit_, function_);

  Trace(2, "%d loop(s).", loop_graph_->NumberOfLoops()-1);

  if (loop_graph_->NumberOfLoops() > 1) {
    MAO_ASSERT(function_->GetSection());
    // Build an offset map fromt he size map we have.
    // TODO(martint): Optimize the code so that the map is not
    // rebuild for each function.
    MaoEntryIntMap offsets;
    int offset = 0;
    for (SectionEntryIterator iter = function_->GetSection()->EntryBegin();
         iter != function_->GetSection()->EntryEnd();
         ++iter) {
      offsets[*iter] = offset;
      offset += (*sizes_)[*iter];
    }
    FindInner(loop_graph_->root(),  &offsets);
  }
  return true;
}


int LoopAlignPass::BasicBlockSize(BasicBlock *BB) const {
  int size = 0;
  for (SectionEntryIterator iter = BB->EntryBegin();
       iter != BB->EntryEnd();
       ++iter) {
    MaoEntry *entry = *iter;
    size += (*sizes_)[entry];
  }
  return size;
}


// // Given a path, create a newly allocated copy and put it in the collection
// // of found paths
// void LoopAlignPass::AddPath(Paths *paths, const Path *path) {
//   Path *new_path = new Path();
//   for (Path::const_iterator iter = path->begin(); iter != path->end(); ++iter) {
//     new_path->push_back(*iter);
//   }
//   paths->push_back(new_path);
// }


// void LoopAlignPass::FreePaths(Paths *paths) {
//   for (Paths::iterator iter = paths->begin(); iter != paths->end(); ++iter) {
//     free(*iter);
//   }
// }

// // Given an inner loop, find all possible paths and place them in paths
// void LoopAlignPass::GetPaths(const SimpleLoop *loop, Paths *paths) {
//   // iterate over successors that are in the loop
//   BasicBlock *head = loop->header();
//   Path path;
//   path.push_back(head);
//   for (BasicBlock::ConstEdgeIterator iter = head->BeginOutEdges();
//        iter != head->EndOutEdges();
//        ++iter) {
//     BasicBlock *dest = (*iter)->dest();
//     // Dont process basic blocks that are not part of the loop
//     if (loop->Includes(dest)) {
//       GetPathsImp(loop, dest, paths, &path);
//     }
//   }
// }

// void LoopAlignPass::GetPathsImp(const SimpleLoop *loop,
//                                 BasicBlock *current,
//                                 Paths *paths, Path *path) {
//   // Needs to be an inner loop
//   MAO_ASSERT(loop->NumberOfChildren() == 0);
//   if (current == loop->header()) {
//     // Add the path the list of found paths
//     // Memory allocated here must be freed later.
//     AddPath(paths, path);
//   } else {
//     // Since this is an inner loop, current is not in the path yet. add it
//     path->push_back(current);
//     // loop over the children
//     for (BasicBlock::ConstEdgeIterator iter = current->BeginOutEdges();
//          iter != current->EndOutEdges();
//          ++iter) {
//       BasicBlock *dest = (*iter)->dest();
//       if (loop->Includes(dest)) {
//         GetPathsImp(loop, dest, paths, path);
//       }
//     }
//     path->pop_back();
//   }
// }


// // Find out how many lines a 16 bytes are needed
// // to store the set of basic blocks in the path.
// // No assumptions about the order of the paths.
// // For each set of consecutive basic block, see how many
// // lines they require. Add up the total number of lines.
// //
// //  Assume that BB1, BB2, .. are stored in memory after each other
// //  Assume that BB1, BB2, BB  are in the loop
// //  Then the result will look like this
// //      {BB1, BB2} -> size(BB1, BB2)
// //      {BB4}      -> size(BB4)
// //
// // With this information, we can calculate how many lines required to
// // hold this loop path.
// void LoopAlignPass::ProcessPath(Path *path) {
//   Trace(3, "Process path:");
//   // Keeps track of what blocks we have already processed
//   std::map<BasicBlock *, BasicBlock *> f_connections;
//   std::map<BasicBlock *, BasicBlock *> b_connections;

//  for (Path::iterator iiter = path->begin(); iiter !=  path->end(); ++iiter) {
//    for (Path::iterator jiter = path->begin(); jiter !=  path->end(); ++jiter) {
//       if (*iiter != *jiter) {
//         if ((*iiter)->DirectlyPreceeds(*jiter)) {
//           // Connect them!
//           MAO_ASSERT(f_connections.find(*iiter) == f_connections.end() ||
//                      f_connections[*iiter] == *jiter);
//           MAO_ASSERT(b_connections.find(*jiter) == b_connections.end() ||
//                      b_connections[*jiter] == *iiter);
//           f_connections[*iiter] = *jiter;
//           b_connections[*jiter] = *iiter;
//         }
//         if ((*jiter)->DirectlyPreceeds(*iiter)) {
//           // Connect them!
//           MAO_ASSERT(b_connections.find(*iiter) == b_connections.end() ||
//                      b_connections[*iiter] == *jiter);
//           MAO_ASSERT(f_connections.find(*jiter) == f_connections.end() ||
//                      f_connections[*jiter] == *iiter);
//           f_connections[*jiter] = *iiter;
//           b_connections[*iiter] = *jiter;
//         }
//       }
//     }
//   }

//   // Maps the startblock of the chain the size of the chain.
//   std::map<BasicBlock *, int> chain_sizes;
//   // Now we are ready to check for sizes.
//   for (Path::iterator iter = path->begin(); iter !=  path->end(); ++iter) {
//     // is it a start of a chain?
//     if (b_connections.find(*iter) == b_connections.end()) {
//       chain_sizes[*iter] = ChainSize(*iter, &f_connections);
//     }
//   }

//   int lines = NumLines(path, &chain_sizes);
//   MAO_ASSERT(16*lines >= PathSize(path));

//   if (lines <= 4) {
//     // Align path!
//     for (Path::iterator iter = path->begin(); iter !=  path->end(); ++iter) {
//       // is it a start of a chain?
//       if (b_connections.find(*iter) == b_connections.end()) {
//         // This is the start of a chain which should be aligned!
//         Trace(3, "Aligned block :%d", (*iter)->id());
//         //  AlignBlock(*iter);
//       }
//     }
//   }
// }


// int LoopAlignPass::ChainSize(BasicBlock *basicblock,
//                          std::map<BasicBlock *, BasicBlock *> *connections) {
//   int size = 0;
//   while (connections->find(basicblock) != connections->end()) {
//     size += BasicBlockSize(basicblock);
//     basicblock = (*connections)[basicblock];
//   }
//   size += BasicBlockSize(basicblock);
//   return size;
// }


void LoopAlignPass::ProcessInnerLoop(const SimpleLoop *loop,
                                     MaoEntryIntMap   *offsets) {
  // Find out if the distance between the first entry in the first block to the
  // end of the last block fits in 64 bytes? Then try to align it.

  BasicBlock *first = *(loop->ConstBasicBlockBegin());
  BasicBlock *last = *(loop->ConstBasicBlockBegin());

  for (SimpleLoop::BasicBlockSet::iterator iter = loop->ConstBasicBlockBegin();
       iter != loop->ConstBasicBlockEnd();
       ++iter) {
    if ((*offsets)[(*iter)->first_entry()] >
        (*offsets)[last->first_entry()]) {
      last = *iter;
    }
    if ((*offsets)[(*iter)->first_entry()] <
        (*offsets)[first->first_entry()]) {
      first = *iter;
    }
  }

  int loop_space = (*offsets)[last->last_entry()] +
      (*sizes_)[last->last_entry()] - (*offsets)[first->first_entry()];


  // Only 64-byte looks and smaller will ever fit in the LSD
  // If the loop is smaller than 50 bytes, it do not need
  // to be aligned.
  if (loop_space <= 64 && loop_space >= 50) {
    // Align the first entry int he basic block.
    first->first_entry()->AlignTo(4, 0, 0);
  }
}



// // Given an inner loop, get all the paths that go from the
// // header, back to the header. If any paths fit in the
// // loop buffer, consider it for alignment
// void LoopAlignPass::ProcessInnerLoopOld(const SimpleLoop *loop) {
//   // Hold all the paths found in the loop
//   Paths paths;
//   // Get all the possible paths in this loop
//   GetPaths(loop, &paths);

//   // Process the paths
//   for (Paths::iterator iter = paths.begin();
//        iter != paths.end();
//        ++iter) {
//     // Process the given path
//     ProcessPath(*iter);
//   }
//   // Return memory
//   FreePaths(&paths);
// }

// int LoopAlignPass::NumLines(const Path *path,
//                             std::map<BasicBlock *, int> *chain_sizes) {
//   int lines = 0;
//   for (Path::const_iterator iter = path->begin();
//        iter != path->end();
//        ++iter) {
//     if (chain_sizes->find(*iter) != chain_sizes->end()) {
//       // This assert would trigger if it found
//       // basic blocks without any size!
//       MAO_ASSERT((*chain_sizes)[*iter] >= 0);
//       int c_lines = ((*chain_sizes)[*iter]-1)/16+1;
//       lines += c_lines;
//     }
//   }
//   return lines;
// }

int LoopAlignPass::LoopSize(const SimpleLoop *loop) const {
  int size = 0;
  // Loop over basic block to get their sizes!
  for (SimpleLoop::BasicBlockSet::const_iterator iter =
           loop->ConstBasicBlockBegin();
       iter != loop->ConstBasicBlockEnd();
       ++iter) {
    size += BasicBlockSize(*iter);
  }
  return size;
}


// int LoopAlignPass::PathSize(const Path *path) const {
//   int size = 0;
//   // Loop over basic block to get their sizes!
//   for (Path::const_iterator iter = path->begin();
//        iter != path->end();
//        ++iter) {
//     size += BasicBlockSize(*iter);
//   }
//   return size;
// }


// Function called recurively to find the inner loops that are candidates
// for alignment.
void LoopAlignPass::FindInner(const SimpleLoop *loop,
                              MaoEntryIntMap   *offsets) {
  if (loop->nesting_level() == 0 &&   // Leaf node = Inner loop
      !loop->is_root()) {             // Make sure its not the root node
    // Found an inner loop
    MAO_ASSERT(loop->NumberOfChildren() == 0);
    Trace(3, "Found an inner loop...");

    // Currently we see if there any paths in the inner loops that are
    // candiates for alignment!
    int size = LoopSize(loop);

    // This simple heuristic will process all loops that are
    // smaller than 64 bytes.
    if (size <= 64) {
      ProcessInnerLoop(loop, offsets);
    }

    // Statistics gathering
    if (collect_stat_) {
      loop_align_stat_->FoundInnerLoop(size,
                                       size <= maximum_loop_size_);
    }
  }

  // Recursive call in order to find all
  for (SimpleLoop::LoopSet::const_iterator liter = loop->ConstChildrenBegin();
       liter != loop->ConstChildrenEnd(); ++liter) {
    FindInner(*liter, offsets);
  }
}

// External Entry Point
void InitLoopAlign() {
  RegisterFunctionPass(
      "LOOPALIGN",
      MaoFunctionPassManager::GenericPassCreator<LoopAlignPass>);
}
