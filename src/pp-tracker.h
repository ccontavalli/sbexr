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

#include "common.h"

class PPTracker : public PPCallbacks {
 public:
  PPTracker(FileCache* cache, const CompilerInstance& ci)
      : cache_(cache), ci_(ci) {}
  virtual ~PPTracker() = default;

  void FileChanged(SourceLocation loc, FileChangeReason reason,
                   SrcMgr::CharacteristicKind kind, FileID prevfid) override {
    if (gl_verbose) {
      std::cerr << "#CHANGED EVENT " << reason << " FOR " << loc.printToString(ci_.getSourceManager()) << " P:" << ShouldProcess() << " I:" << include_ignored_ << std::endl;
    }
    if (reason == EnterFile) {
      if (!ShouldProcess()) {
        include_ignored_++;
        return;
      }

      auto file = cache_->GetFileFor(ci_.getSourceManager(), loc);
      if (file) {
        if (file->preprocessing || file->preprocessed) {
          include_ignored_++;
          return;
        }
        file->preprocessing = true;
      }

      if (gl_verbose)
        std::cerr << "  -> ENTERING "
                  << (file ? file->path : std::string("<INVALID>")) << " ("
                  << loc.printToString(ci_.getSourceManager()) << ")"
                  << std::endl;

      include_stack_.push(file);
      return;
    }

    if (reason == ExitFile) {
      if (include_ignored_ > 0) {
        --include_ignored_;
        return;
      }

      auto file = include_stack_.top();
      if (file) file->preprocessed = true;

      if (gl_verbose)
        std::cerr << "#EXITING "
                  << (file ? file->path : std::string("<INVALID>"))
                  << std::endl;

      include_stack_.pop();
      return;
    }
  }

  bool ShouldProcess() {
    return include_stack_.empty() ||
           (include_stack_.top() && !include_stack_.top()->preprocessed && include_ignored_ <= 0);
  }

  //  bool FileNotFound(StringRef filename,
  //                    SmallVectorImpl<char>& RecoveryPath) override {
  //    std::cerr << "FILE NOT FOUND !!! " << filename.str() << std::endl;
  //    return false;
  //  }

  void InclusionDirective(SourceLocation loc, const Token& IncludeTok,
                          StringRef FileName, bool IsAngled,
                          CharSourceRange filename_range, const FileEntry* File,
                          StringRef SearchPath, StringRef RelativePath,
                          const clang::Module* Imported,
                          SrcMgr::CharacteristicKind FileType) override {
    auto included_full_path = (SearchPath + "/" + RelativePath).str();
    auto* file_descriptor = cache_->GetFileFor(included_full_path);

    if (gl_verbose)
      std::cerr << "#INCLUDING " << file_descriptor->path << " ("
                << included_full_path << ") FROM "
                << PrintLocation(ci_.getSourceManager(), cache_, loc) << " ("
                << cache_->GetNormalizedPath(ci_.getSourceManager(),
                                             filename_range.getBegin())
                << ") P:"<< ShouldProcess() << " F:" << (File ? "[has file]" : "[NO FILE]")  << std::endl;

    if (!ShouldProcess() || !File) {
      if (gl_verbose)
        std::cerr << "#IGNORING STATEMENT" << std::endl;
      // FIXME: this means there's something wrong. The build would have failed,
      // but here we are trying to index the file.
      return;
    }

    auto html_path = file_descriptor->HtmlPath();
    WrapWithTag(ci_, cache_, filename_range.getAsRange(),
                std::move(MakeTag("a", {"include"}, {"href", html_path})));
  }

  /// \brief Called by Preprocessor::HandleMacroExpandedIdentifier when a
  /// macro invocation is found.
  void MacroExpands(const Token& MacroNameTok, const MacroDefinition& MD,
                    SourceRange Range, const MacroArgs* Args) override {
  };

  /// \brief Hook called whenever a macro definition is seen.
  void MacroDefined(const Token& MacroNameTok,
                    const MacroDirective* MD) override {
  };

  /// \brief Hook called whenever a macro \#undef is seen.
  ///
  /// MD is released immediately following this callback.
  void MacroUndefined(const Token& MacroNameTok, const MacroDefinition& MD,
                      const MacroDirective* Undef) override{};

  /// \brief Hook called when a source range is skipped.
  /// \param Range The SourceRange that was skipped. The range begins at the
  /// \#if/\#else directive and ends after the \#endif/\#else directive.
  void SourceRangeSkipped(SourceRange Range,
                          SourceLocation endifloc) override{};

  /// \brief Hook called whenever an \#if is seen.
  /// \param Loc the source location of the directive.
  /// \param ConditionRange The SourceRange of the expression being tested.
  /// \param ConditionValue The evaluated value of the condition.
  ///
  // FIXME: better to pass in a list (or tree!) of Tokens.
  void If(SourceLocation location, SourceRange cond_range,
          ConditionValueKind value) override {
    if (!ShouldProcess()) return;
    if_stack_.emplace(value, cond_range.getBegin());
  };

  /// \brief Hook called whenever an \#elif is seen.
  /// \param Loc the source location of the directive.
  /// \param ConditionRange The SourceRange of the expression being tested.
  /// \param ConditionValue The evaluated value of the condition.
  /// \param IfLoc the source location of the \#if/\#ifdef/\#ifndef directive.
  // FIXME: better to pass in a list (or tree!) of Tokens.
  void Elif(SourceLocation location, SourceRange cond_range,
            ConditionValueKind value, SourceLocation if_loc) override {
    if (!ShouldProcess()) return;
    if (if_stack_.empty()) return;

    auto& state = if_stack_.top();
    if (state.condition == CVK_False) {
      WrapEolSol(ci_, cache_, state.if_start, location,
                 MakeTag("span", {"preprocessor-if", "muted"}, {}));
    }
    state.condition = value;
    state.if_start = cond_range.getBegin();
  };

  /// \brief Hook called whenever an \#ifdef is seen.
  /// \param Loc the source location of the directive.
  /// \param MacroNameTok Information on the token being tested.
  /// \param MD The MacroDefinition if the name was a macro, null otherwise.
  void Ifdef(SourceLocation location, const Token& token,
             const MacroDefinition& definition) override {
    if (!ShouldProcess()) return;
    if (gl_verbose)
      std::cerr << "#IFDEF IN "
                << PrintLocation(ci_.getSourceManager(), cache_, location)
                << std::endl;
    if_stack_.emplace((definition ? CVK_True : CVK_False), token.getEndLoc());
  };

  /// \brief Hook called whenever an \#ifndef is seen.
  /// \param Loc the source location of the directive.
  /// \param MacroNameTok Information on the token being tested.
  /// \param MD The MacroDefiniton if the name was a macro, null otherwise.
  void Ifndef(SourceLocation location, const Token& token,
              const MacroDefinition& definition) override {
    if (!ShouldProcess()) return;
    if (gl_verbose)
      std::cerr << "#IFNDEF IN "
                << PrintLocation(ci_.getSourceManager(), cache_, location)
                << std::endl;
    if_stack_.emplace((definition ? CVK_False : CVK_True), token.getEndLoc());
  };

  /// \brief Hook called whenever an \#else is seen.
  /// \param Loc the source location of the directive.
  /// \param IfLoc the source location of the \#if/\#ifdef/\#ifndef directive.
  void Else(SourceLocation location, SourceLocation if_location) override {
    if (!ShouldProcess()) return;
    if (if_stack_.empty()) return;

    if (gl_verbose)
      std::cerr << "#ELSE IN "
                << PrintLocation(ci_.getSourceManager(), cache_, location)
                << std::endl;

    auto& state = if_stack_.top();
    if (state.condition == CVK_False) {
      //     std::cerr << "IFELSE " <<
      //     location.printToString(rewriter_.getSourceMgr()) << " "
      //                           <<
      //                           if_location.printToString(rewriter_.getSourceMgr())
      //                           << " "
      //                           <<
      //                           state.if_start.printToString(rewriter_.getSourceMgr())
      //                           << std::endl;
      WrapEolSol(ci_, cache_, state.if_start, location,
                 MakeTag("span", {"preprocessor-if", "muted"}, {}));
      state.condition = CVK_True;
    } else {
      state.condition = CVK_False;
    }
    state.if_start = location;
  };

  /// \brief Hook called whenever an \#endif is seen.
  /// \param Loc the source location of the directive.
  /// \param IfLoc the source location of the \#if/\#ifdef/\#ifndef directive.
  void Endif(SourceLocation location, SourceLocation if_location) override {
    if (!ShouldProcess()) return;
    if (gl_verbose)
      std::cerr << "#ENDIF IN "
                << PrintLocation(ci_.getSourceManager(), cache_, location)
                << std::endl;

    if (if_stack_.empty()) return;

    const auto& state = if_stack_.top();
    if (state.condition == CVK_False) {
      WrapEolSol(ci_, cache_, state.if_start, location,
                 MakeTag("span", {"preprocessor-if", "muted"}, {}));
    }
    if_stack_.pop();
  };

 private:
  struct State {
    State(const ConditionValueKind& condition, const SourceLocation& location)
        : condition(condition), if_start(location) {}
    ConditionValueKind condition = CVK_NotEvaluated;
    SourceLocation if_start;
  };
  std::stack<State> if_stack_;
  std::stack<FileRenderer::ParsedFile*> include_stack_;
  int include_ignored_ = 0;

  FileCache* cache_;
  const CompilerInstance& ci_;
};


#endif
