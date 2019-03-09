// Copyright (c) 2017 Carlo Contavalli (ccontavalli@gmail.com).
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//
//    2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY Carlo Contavalli ''AS IS'' AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL Carlo Contavalli OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
// THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// The views and conclusions contained in the software and documentation are
// those of the authors and should not be interpreted as representing official
// policies, either expressed or implied, of Carlo Contavalli.

#ifndef PP_TRACKER_H
#define PP_TRACKER_H

#include "ast.h"
#include "common.h"
#include "counters.h"
#include "wrapping.h"

extern Counter& c_pp_file_not_found;
extern Counter& c_pp_file_failed_inclusion;

class PPTracker : public PPCallbacks {
 public:
  PPTracker(SbexrRecorder* recorder) : recorder_(recorder) {}
  virtual ~PPTracker() = default;

  void FileChanged(SourceLocation loc, FileChangeReason reason,
                   SrcMgr::CharacteristicKind kind, FileID prevfid) override {
    if (reason != ExitFile && reason != EnterFile) return;

    if (gl_verbose) {
      if (reason == EnterFile) {
        std::cerr << "## "
                  << " ENTERING " << recorder_->PrintLocation(loc)
                  << " P:" << ShouldProcess();
      } else {
        std::cerr << "## "
                  << " EXITING " << GetFilePath(recorder_->GetFileFor(prevfid))
                  << ShouldProcess();
      }
    }

    // Examples:
    //  file-1 -> processing=1, processed=0
    //    ... block-a
    //    include file-2  -> processing=1, processed=0
    //      ... block-b
    //      include file-1
    //      exit file-1
    //      ... block-c
    //    exit file-2
    //    ... block-d
    switch (reason) {
      case ExitFile: {
        auto file = recorder_->GetFileFor(prevfid);
        if (file && should_process_) file->preprocessed = true;

        file = recorder_->GetFileFor(loc);
        if (!file) {
          should_process_ = false;
        } else if (file->preprocessed) {
          should_process_ = false;
        } else {
          file->preprocessing = true;
          should_process_ = true;
        }
        break;
      }

      case EnterFile: {
        auto file = recorder_->GetFileFor(loc);
        if (!file) {
          should_process_ = false;
        } else if (file->preprocessing || file->preprocessed) {
          should_process_ = false;
        } else {
          file->preprocessing = true;
          should_process_ = true;
        }
        break;
      }
    }
  }

  bool FileNotFound(StringRef filename,
                    SmallVectorImpl<char>& RecoveryPath) override {
    c_pp_file_not_found.Add() << filename.str();
    return false;
  }

  void InclusionDirective(SourceLocation loc, const Token& IncludeTok,
                          StringRef FileName, bool IsAngled,
                          CharSourceRange filename_range, const FileEntry* File,
                          StringRef SearchPath, StringRef RelativePath,
                          const clang::Module* Imported,
                          SrcMgr::CharacteristicKind FileType) override {
    auto included_full_path = (SearchPath + "/" + RelativePath).str();
    auto* file_descriptor =
        recorder_->GetCache()->GetFileFor(included_full_path);

    if (gl_verbose)
      std::cerr << "#INCLUDING " << file_descriptor->path << " ("
                << included_full_path << ") FROM "
                << recorder_->PrintLocation(loc) << " ("
                << GetFilePath(recorder_->GetFileFor(filename_range.getBegin()))
                << ") P:" << ShouldProcess()
                << " F:" << (File ? "[has file]" : "[NO FILE]") << std::endl;

    if (!File) {
      c_pp_file_failed_inclusion.Add(filename_range.getAsRange())
          << included_full_path;
      return;
    }

    auto html_path = file_descriptor->HtmlPath();
    WrapWithTag(*recorder_->GetCI(), recorder_->GetCache(),
                filename_range.getAsRange(),
                std::move(MakeTag("a", {"include"}, {"href", html_path})));
  }

  void MacroExpands(const Token& name, const MacroDefinition& md,
                    SourceRange range, const MacroArgs* args) override {
    if (!ShouldProcess()) return;

    auto target = GetMacroRange(*md.getLocalDirective()->getInfo());
    auto mrange =
        SourceRange(name.getLocation(), name.getEndLoc().getLocWithOffset(-1));

    if (gl_verbose) {
      std::cerr << "MACRO EXPAND " << name.getIdentifierInfo()->getName().str()
                << " expanding:" << recorder_->PrintLocation(mrange) << "'"
                << recorder_->PrintCode(mrange) << "' "
                << " target:" << recorder_->PrintLocation(target) << " '"
                << recorder_->PrintCode(target) << "'" << std::endl;
    }

    // To see the result of the expansion, we can use a TokenLexer.
    recorder_->CodeUses(mrange, "MACRO", "MACRO", target);
  };

  void MacroDefined(const Token& name, const MacroDirective* md) override {
    if (!ShouldProcess()) return;

    auto* mi = md->getMacroInfo();
    auto target = GetMacroRange(*mi);
    auto highlight = SourceRange(name.getLocation(), mi->getDefinitionEndLoc());
    // auto highlight = SourceRange(name.getLocation(), name.getEndLoc());
    // if (mi->getNumTokens() > 0) {
    //  highlight = SourceRange(name.getLocation(),
    //  mi->tokens()[mi->getNumTokens() - 1].getEndLoc());
    //}

    if (gl_verbose) {
      auto pfd = recorder_->GetCI()->getPreprocessor().getPredefinesFileID();
      auto lid =
          recorder_->GetCI()->getSourceManager().getFileID(name.getLocation());
      auto internal = pfd == lid;

      std::cerr << "MACRO DEFINED " << internal << " "
                << mi->isUsedForHeaderGuard() << " "
                << name.getIdentifierInfo()->getName().str() << " at "
                << recorder_->PrintLocation(name.getLocation()) << " vs "
                << recorder_->PrintLocation(target) << " '"
                << recorder_->PrintCode(highlight) << "'" << std::endl;
      for (const auto& tok : mi->tokens()) {
        std::cerr << "  TOKEN " << tok.getName() << " "
                  << recorder_->PrintLocation(tok.getLocation()) << " "
                  << recorder_->PrintLocation(tok.getEndLoc())
                  /* << " " << (tok.isLiteral() ? tok.getLiteralData() :
                     "not-literal") */
                  << std::endl;
      }
    }

    // TODO: isUsedForHeaderGuard() seems to be always returning false.
    // Is there a reliable way to detect and skip header guards?
    if (!mi->isUsedForHeaderGuard()) {
      recorder_->CodeDefines(highlight, target, target, "MACRO",
                             name.getIdentifierInfo()->getName().str(),
                             AccessSpecifier::AS_public, Linkage::NoLinkage);
    }
  };

  void If(SourceLocation location, SourceRange cond_range,
          ConditionValueKind value) override {
    if (!ShouldProcess()) return;
    if_stack_.emplace(value, cond_range.getBegin());
  };

  void Elif(SourceLocation location, SourceRange cond_range,
            ConditionValueKind value, SourceLocation if_loc) override {
    if (!ShouldProcess()) return;

    auto& state = if_stack_.top();
    if (state.condition == CVK_False) {
      WrapEolSol(*recorder_->GetCI(), recorder_->GetCache(), state.if_start,
                 location, MakeTag("span", {"preprocessor-if", "muted"}, {}));
    }
    state.condition = value;
    state.if_start = cond_range.getBegin();
  };

  void Defined(const Token& name, const MacroDefinition& definition,
               SourceRange location) override {
    if (!ShouldProcess()) return;
    if (gl_verbose)
      std::cerr << "#DEFINED IN " << recorder_->PrintLocation(location) << "  "
                << (bool)(definition) << std::endl;

    auto* mi = definition.getMacroInfo();
    auto mrange =
        SourceRange(name.getLocation(), name.getEndLoc().getLocWithOffset(-1));
    if (definition && mi) {
      auto target = GetMacroRange(*mi);
      recorder_->CodeUses(mrange, "MACRO", "MACRO", target);
    } else {
      WrapWithTag(*recorder_->GetCI(), recorder_->GetCache(), mrange,
                  std::move(MakeTag("span", {"macro-undefined"}, {})));
    }
  }

  void Ifdef(SourceLocation location, const Token& name,
             const MacroDefinition& definition) override {
    if (!ShouldProcess()) return;
    if (gl_verbose)
      std::cerr << "#IFDEF IN " << recorder_->PrintLocation(location) << "  "
                << (bool)(definition) << std::endl;

    auto* mi = definition.getMacroInfo();
    auto mrange =
        SourceRange(name.getLocation(), name.getEndLoc().getLocWithOffset(-1));
    if (definition && mi) {
      auto target = GetMacroRange(*mi);

      recorder_->CodeUses(mrange, "MACRO", "MACRO", target);
    } else {
      WrapWithTag(*recorder_->GetCI(), recorder_->GetCache(), mrange,
                  std::move(MakeTag("span", {"macro-undefined"}, {})));
    }

    if_stack_.emplace((definition ? CVK_True : CVK_False), name.getEndLoc());
  };

  void Ifndef(SourceLocation location, const Token& name,
              const MacroDefinition& definition) override {
    if (!ShouldProcess()) return;
    if (gl_verbose)
      std::cerr << "#IFNDEF IN " << recorder_->PrintLocation(location) << " "
                << (bool)(definition) << std::endl;

    auto* mi = definition.getMacroInfo();
    auto mrange =
        SourceRange(name.getLocation(), name.getEndLoc().getLocWithOffset(-1));
    if (definition && mi) {
      auto target = GetMacroRange(*mi);
      recorder_->CodeUses(mrange, "MACRO", "MACRO", target);
    } else {
      WrapWithTag(*recorder_->GetCI(), recorder_->GetCache(), mrange,
                  std::move(MakeTag("span", {"macro-undefined"}, {})));
    }

    if_stack_.emplace((definition ? CVK_False : CVK_True), name.getEndLoc());
  };

  void Else(SourceLocation location, SourceLocation if_location) override {
    if (!ShouldProcess()) return;

    if (gl_verbose)
      std::cerr << "#ELSE IN " << recorder_->PrintLocation(location)
                << std::endl;

    auto& state = if_stack_.top();
    if (state.condition == CVK_False) {
      WrapEolSol(*recorder_->GetCI(), recorder_->GetCache(), state.if_start,
                 location, MakeTag("span", {"preprocessor-if", "muted"}, {}));
      state.condition = CVK_True;
    } else {
      state.condition = CVK_False;
    }
    state.if_start = location;
  };

  void Endif(SourceLocation location, SourceLocation if_location) override {
    if (!ShouldProcess()) return;
    if (gl_verbose)
      std::cerr << "#ENDIF IN " << recorder_->PrintLocation(location)
                << std::endl;

    const auto& state = if_stack_.top();
    if (state.condition == CVK_False) {
      WrapEolSol(*recorder_->GetCI(), recorder_->GetCache(), state.if_start,
                 location, MakeTag("span", {"preprocessor-if", "muted"}, {}));
    }
    if_stack_.pop();
  };

 private:
  // Return true if we should process this file. Ensures a file is processed
  // only once.
  bool ShouldProcess() { return should_process_; }

  // Return the range used to identify a MACRO.
  SourceRange GetMacroRange(const MacroInfo& mi) {
    // in #define FOO 1, getDefinitionLoc points right after FOO,
    // getDefintionEndLoc() points to the 1.
    return SourceRange(mi.getDefinitionLoc(), mi.getDefinitionEndLoc());
  }

  struct State {
    State(const ConditionValueKind& condition, const SourceLocation& location)
        : condition(condition), if_start(location) {}
    ConditionValueKind condition = CVK_NotEvaluated;
    SourceLocation if_start;
  };
  std::stack<State> if_stack_;
  bool should_process_ = true;

  SbexrRecorder* recorder_;
};

#endif
