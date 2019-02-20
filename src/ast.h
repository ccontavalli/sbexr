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

#ifndef AST_H
#define AST_H

#include "common.h"

class SbexrRecorder {
 public:
  SbexrRecorder(FileCache* cache, Indexer* index)
    : cache_(cache), index_(index) {}

  void SetParameters(const CompilerInstance* ci) {
    ci_ = ci;
    printer_ = Printer(*ci, cache_);
  }

  // Example:
  //
  //   Point foo;
  //   Point foo(float bar);
  //
  // There are 3 locations necessary:
  //   1 - location of the user ("foo"), to be marked as a link.
  //
  //   2 - location of the text "Point", to turn into a link.
  //   3 - location of the type Point definition, to use as link target.
  //
  // Now:
  //   - DeclStmt provides 2 and 3.
  //   - DeclaratorDecl (variable) should mark that type Point is used by 1.
  //   - NamedDecl (function return value) should do all, 1, 2 and 3.
  //
  // Below:
  //   - user is "Point"
  //   - qual_type is the Point Definition
  //
  //   - one function:
  //     - goal: link "Point", to the Point definition.
  //
  //   - another function:
  //     - goal: record that foo, uses the Point definition.
  //
  TagDecl* GetTagDeclForType(const QualType& qual_type) const {
    auto* real_type = qual_type.split().Ty;
    // For pointers, like:
    //     Point *** foo;
    // find the original type, descending the type system.
    //
    // Note that nullptr_t seems to have hasPointerRepresentation() true,
    // and real_type->getPointeeType().split().Ty returning null.
    while (real_type && real_type->hasPointerRepresentation())
      real_type = real_type->getPointeeType().split().Ty;

    if (!real_type) return nullptr;
    return real_type->getAsTagDecl();
  }

  // Returns true if the location has already been rendered.
  bool LocationRendered(SourceLocation loc) const {
    auto* file = GetFileFor(loc);
    return file && file->Rendered();
  }

  template <typename UserT>
  void LinkToType(const UserT& user, const char* description, TagDecl* target) {
    if (!target) return;

    const auto& ntarget = NormalizeSourceRange(target->getSourceRange());
    if (!ntarget.isValid()) return;

    auto sr = GetSourceRangeOrFail(user);
    WrapWithTag(*ci_, cache_, sr,
                MakeTag("a", {std::string(description) + "-uses"},
                        {"href", MakeIdLink(ntarget)}));
  }

  template <typename UserT>
  void RecordTypeUse(const UserT& user, const char* description,
                     TagDecl* target) {
    if (!target) return;

    const auto& ntarget = NormalizeSourceRange(target->getSourceRange());
    if (!ntarget.isValid()) return;

    const auto& nuser = NormalizeSourceRange(GetSourceRangeOrFail(user));
    index_->RecordUse(ci_->getSourceManager(), ntarget, nuser, description);
  }

  template <typename UserT>
  void CodeUsesQualType(const UserT& user, const char* description,
                        const QualType& qual_type) {
    auto* real_type = GetTagDeclForType(qual_type);
    LinkToType(user, description, real_type);
    RecordTypeUse(user, description, real_type);
  }

  template <typename UserT, typename TargetT>
  void CodeUses(const UserT& user, const char* description,
                const TargetT& target) {
    // Code may use anonymous structs, unions or other objects that
    // really don't exist in the source code.
    //
    // In that case, they don't have an associated identifier.
    // Creating a link to them is pointless, as there is nothing
    // in the source to look at.
    //
    // Majority of cases, also, if the struct / union is anonymous,
    // the expression must be referring to a field.
    //
    // This means that there will be a link pointing correctly to
    // the field.
    const auto sr = GetSourceRangeOrFail(user);

    const auto& ntarget = NormalizeSourceRange(target.getSourceRange());
    const auto& nuser = NormalizeSourceRange(sr);

    if ((isa<RecordDecl>(&target) &&
         cast<RecordDecl>(&target)->isAnonymousStructOrUnion()) ||
        (isa<FieldDecl>(&target) &&
         cast<FieldDecl>(&target)->isAnonymousStructOrUnion()) ||
        isa<IndirectFieldDecl>(&target)) {
      if (gl_verbose)
        std::cerr << "+ IGNORED-ANON " << description << " "
                  << target.Decl::getDeclKindName() << " "
                  << PrintLocation(nuser) << " " << PrintLocation(ntarget)
                  << std::endl;
      return;
    }

    if (!ntarget.isValid()) return;

    if (gl_verbose)
      std::cerr << "+ USE " << description << " "
                << target.Decl::getDeclKindName() << " " << PrintLocation(nuser)
                << " " << PrintLocation(ntarget) << std::endl;

    WrapWithTag(*ci_, cache_, sr,
                MakeTag("a", {std::string(description) + "-uses"},
                        {"href", MakeIdLink(ntarget)}));
    index_->RecordUse(ci_->getSourceManager(), ntarget, nuser, description);
  }

  template <typename DefinerT, typename DefinedT>
  void CodeDefines(const DefinerT& definer, const DefinedT& defined,
                   const char* kind, const std::string& name,
                   AccessSpecifier access, clang::Linkage linkage) {
    if (name.empty()) return;
    auto definer_range = NormalizeSourceRange(definer.getSourceRange());
    auto defined_range = NormalizeSourceRange(defined.getSourceRange());

    const auto& id = MakeIdName(definer_range);
    if (gl_verbose)
      std::cerr << "+ DEFINE FOR " << id << " " << kind << std::endl;

    WrapWithTag(
        *ci_, cache_, definer_range,
        MakeTag("span", {"def", std::string("def-") + kind}, {"id", id}));
    index_->RecordDefines(ci_->getSourceManager(), defined_range, definer_range,
                          kind, name, GetSnippet(definer_range), access,
                          linkage);
  }

  // Records that the code declares something.
  // DeclarerT, aka the provider, is an object which can provide a source range.
  template <typename DeclarerT, typename DeclaredT>
  void CodeDeclares(const DeclarerT& declarer, const DeclaredT& declared,
                    const char* kind, const std::string& name,
                    AccessSpecifier access, clang::Linkage linkage) {
    if (name.empty()) return;
    auto declarer_range = NormalizeSourceRange(declarer.getSourceRange());
    auto declared_range = NormalizeSourceRange(declared.getSourceRange());

    if (!declared_range.isValid()) return;

    const auto& id = MakeIdName(declared_range);
    if (gl_verbose)
      std::cerr << "+ DECLARES FOR " << id << " " << kind << std::endl;
    if (&declared == &declarer)
      WrapWithTag(
          *ci_, cache_, declared_range,
          MakeTag("span", {"decl", std::string("decl-") + kind}, {"id", id}));
    index_->RecordDeclares(ci_->getSourceManager(), declared_range,
                           declarer_range, kind, name,
                           GetSnippet(declared_range), access, linkage);
  }

  template <typename TypeT>
  std::string PrintLocation(const TypeT& range) {
    const auto& sm = ci_->getSourceManager();
    return ::PrintLocation(sm, cache_, range);
  }

  template <typename TypeT>
  std::string TryPrint(const TypeT* v) {
    return printer_.Print(v);
  }

 private:
  std::string MakeIdLink(SourceRange location) {
    auto& sm = ci_->getSourceManager();
    std::string prefix(MakeHtmlPath(cache_->GetFileHashFor(sm, location.getBegin())));
    prefix.append("#");
    prefix.append(::MakeIdName(sm, location));

    return prefix;
  }
  std::string MakeIdName(SourceRange location) {
    return ::MakeIdName(ci_->getSourceManager(), location);
  }

  FileRenderer::ParsedFile* GetFileFor(SourceLocation location) const {
    return cache_->GetFileFor(ci_->getSourceManager(), location);
  }

  StringRef GetSnippet(const SourceRange& range) {
    const auto& sm = ci_->getSourceManager();

    FileID fid;
    unsigned offset;

    std::tie(fid, offset) = sm.getDecomposedExpansionLoc(range.getBegin());
    if (!fid.isValid()) return "<invalid-file>";

    bool invalid = false;
    const auto& buffer = sm.getBufferData(fid, &invalid);
    if (invalid) return "<invalid-buffer>";

    const char* data = buffer.data();
    if (offset >= buffer.size()) return "<invalid-offset>";

    const char* begin = data + offset;
    const char* end = begin + 1;

    // FIXME: instead of stopping at \n, stop at start of file, ; or }
    // Find end of previous line or beginning of file.
    for (; begin > data && *begin != '\n' && *begin != '\r'; --begin)
      ;
    // Skip any whitespace.
    for (; begin <= (data + offset) && isspace(*begin); ++begin)
      ;
    // FIXME: instead of stopping at \n, stop at end of file, ; or {
    // Find end of line.
    for (; end < data + buffer.size() && *end != '\n' && *end != '\r'; ++end)
      ;
    // FIXME: limit the number of characters collected, strip \n and others??
    // Skip any whitespace.
    for (; end > (begin + 1) && isspace(*end); --end)
      ;
    return StringRef(begin, 1 + end - begin);
  }

  FileCache* cache_;
  Indexer* index_;
  Printer printer_;

  const CompilerInstance* ci_ = nullptr;
};

// By implementing RecursiveASTVisitor, we can specify which AST nodes
// we're interested in by overriding relevant methods.
class SbexrAstVisitor : public RecursiveASTVisitor<SbexrAstVisitor> {
 public:
  using Base = RecursiveASTVisitor<SbexrAstVisitor>;
  SbexrAstVisitor(FileCache* cache, Indexer* indexer)
      : recorder_(cache, indexer) {}
  SbexrRecorder* GetRecorder() { return &recorder_; }


  bool shouldVisitTemplateInstantiations() const { return true; }
  // Setting this to true will get the visitor to enter things like
  // implicitly defined constructors, but also things like "using" directives
  // that bring in more details.
  // bool shouldVisitImplicitCode() const { return true; }

  //const CompilerInstance& GetCompilerInstance() const { return *ci_; }

  bool TraverseMemberExpr(MemberExpr* e) {
    if (gl_verbose) {
      std::cerr << "MEMBEREXPR " << recorder_.PrintLocation(e->getSourceRange())
                << recorder_.PrintLocation(e->getMemberNameInfo().getSourceRange())
                << std::endl;
      e->dump();
    }
    recorder_.CodeUses(e->getMemberNameInfo(), "expression", *e->getFoundDecl());
    return Base::TraverseMemberExpr(e);
  }
  bool TraverseDecl(Decl* decl) {
    if (!decl) return Base::TraverseDecl(decl);

    if (recorder_.LocationRendered(decl->getBeginLoc())) {
      if (gl_verbose)
        std::cerr << "FILE ALREADY PARSED " << recorder_.TryPrint(decl) << std::endl;
      return true;
    }

    return Base::TraverseDecl(decl);
  }
  bool TraverseDeclRefExpr(DeclRefExpr* e) {
    // getNameInfo().getAsString() -> returns the variable name, eg, int foo;
    // would return foo.
    if (gl_verbose)
      std::cerr << "DECLREFEXPR " << e->getNameInfo().getAsString() << " "
                << e->getFoundDecl()->getNameAsString() << " "
                << recorder_.PrintLocation(e->getFoundDecl()->getSourceRange())
                << std::endl;
    recorder_.CodeUses(*e, "variable", *e->getFoundDecl());
    return Base::TraverseDeclRefExpr(e);
  }

  bool VisitDeclaratorDecl(DeclaratorDecl* v) {
    if (!v) return true;
      // Notes:
      // getName -> returns the name of the variable declared.
      // getTypeSourceInfo -> returns the original type declared in the source
      // code (eg, 'auto') getType -> returns the type the variable is actually
      // considered to be.

#if 0
    auto decl = v->getCanonicalDecl();
    auto* und = v->getUnderlyingDecl();
    auto* def1 = v->getDefinition();
    auto* def2 = v->getActingDefinition();
    auto* type = v->getTypeSourceInfo();
    auto* under = v->getUnderlyingDecl();
    auto rtype = v->getType();

    assert(decl && "No declaration??");

    std::cerr << "VARDECL " << v->getName().str() << " " << v->getQualifiedNameAsString() << " " << PrintLocation(v->getSourceRange()) << " " << PrintLocation(decl->getSourceRange());

    if (def2) {
      std::cerr << " ACTING " << PrintLocation(def2->getSourceRange());
    }
    if (def1) {
      std::cerr << " ACTUAL " << PrintLocation(def1->getSourceRange());
    }
    if (und) {
      std::cerr << " UNDERLYING " << PrintLocation(und->getSourceRange());
    }
    if (type) {
      std::cerr << " TYPE " << PrintLocation(type->getTypeLoc().getSourceRange()) <<  " " << QualType::getAsString(type->getType().split());
      type->getType().dump();
    }
    if (under) {
      std::cerr << " UNDER " << PrintLocation(under->getSourceRange());
    }
    auto* td = rtype.split().Ty->getAsTagDecl();
    std::cerr << " RTYPE " << rtype.getAsString() <<  " " << QualType::getAsString(rtype.split()) << " ";
    if (td)
     std::cerr << PrintLocation(td->getSourceRange());

#endif
    auto decl = v->getCanonicalDecl();
    if (gl_verbose) {
      std::cerr << "DECLARATORDECL " << v->getName().str() << " "
                << v->getQualifiedNameAsString() << " "
                << recorder_.PrintLocation(v->getSourceRange()) << " "
                << recorder_.PrintLocation(decl->getSourceRange());
      std::cerr << std::endl;
      std::cerr << "DeclaratorDecl: " << recorder_.TryPrint(v) << std::endl;
    }
    // v->dump();

    // TODO:
    //   DONE - pointer and reference types are not linked correctly, probably
    //   need to go
    //     to canonical type or similar before getAsTagDecl().
    //   - need to add tooltip to show canonical type if different from type
    //   (eg, typedefs at play, typedef fuffa int;)
    //     and/or if getTypeSourceInfo != from type (eg, auto at play, auto&
    //     foo);
    //   - types in function names and return values.
    //   - hard to tell if a variable is public / private / local / global.

    if (v->getTypeSourceInfo()) {
#if 0
      auto* expected = static_cast<Decl*>(v);
      auto* context = v->getDeclContext();
      if (context) {
        TypeSourceInfo* type;
        for (const auto* decl : context->decls()) {
          if (decl == expected) break;
        }
      }
#endif

      if (gl_verbose)
        std::cerr << "getTypeSourceInfo: " << recorder_.TryPrint(v->getTypeSourceInfo())
                  << std::endl;

      // If this is a VarDecl in a DeclStmt, then it only needs to be recorded.
      // The type link was already created by the DeclStmt.
      auto* vt = dyn_cast<VarDecl>(v);
      if (vt && vt->isLocalVarDecl()) {
        auto* rt = recorder_.GetTagDeclForType(v->getType());
        recorder_.RecordTypeUse(v->getTypeSourceInfo()->getTypeLoc(), "declaration", rt);
      } else {
        recorder_.CodeUsesQualType(v->getTypeSourceInfo()->getTypeLoc(), "declaration",
                         v->getType());
      }
    }
    return Base::VisitDeclaratorDecl(v);
  }
  bool VisitUsingDecl(UsingDecl* v) {
    // FIXME: do something smart on using declarations.
    if (gl_verbose) std::cerr << recorder_.TryPrint(v) << std::endl;
#if 0
    if (v->shadow_size() == 1) {
      const auto& target = v->shadow_begin()->getTargetDecl();
    }

    static int count = 0;
    if (++count >= 5)
      exit(1);
#endif
    return true;
  }

  bool VisitNamedDecl(NamedDecl* v) {
    if (!v) return true;

    if (gl_verbose) std::cerr << recorder_.TryPrint(v) << std::endl;

    if (isa<FunctionDecl>(v)) {
      // For each use of a templated function, the AST will contain,
      // at the source location, a copy of the specialized function.
      //
      // Without this, we end up parsing that same function over and over,
      // and adding html tags over and over.
      auto* f = cast<FunctionDecl>(v);
      if (f->getTemplatedKind() != FunctionDecl::TK_NonTemplate &&
          f->getTemplatedKind() != FunctionDecl::TK_FunctionTemplate)
        return true;

      // Record the use of the return type.
      recorder_.CodeUsesQualType(f->getReturnTypeSourceRange(), "return",
                       f->getReturnType());

      auto* first = f->getFirstDecl();
      if (!first) first = f;
      if (gl_verbose) std::cerr << recorder_.TryPrint(f) << "\n";

      if (f->isThisDeclarationADefinition()) {
        recorder_.CodeDefines(*v, *first, v->getDeclKindName(),
                    v->getQualifiedNameAsString(), v->getAccess(),
                    v->getLinkageInternal());
      } else {
        recorder_.CodeDeclares(*v, *first, v->getDeclKindName(),
                     v->getQualifiedNameAsString(), v->getAccess(),
                     v->getLinkageInternal());
      }
    } else if (isa<TagDecl>(v)) {
      auto* t = cast<TagDecl>(v);

      auto* first = t->getFirstDecl();
      if (!first) first = t;

      if (t->isCompleteDefinition()) {
        recorder_.CodeDefines(*v, *first, v->getDeclKindName(),
                    v->getQualifiedNameAsString(), v->getAccess(),
                    v->getLinkageInternal());
      } else {
        recorder_.CodeDeclares(*v, *first, v->getDeclKindName(),
                     v->getQualifiedNameAsString(), v->getAccess(),
                     v->getLinkageInternal());
      }
    } else if (isa<VarDecl>(v)) {
      auto* t = cast<VarDecl>(v);
      if (gl_verbose) std::cerr << recorder_.TryPrint(t) << std::endl;

      auto* first = t->getFirstDecl();
      if (!first) first = t;

      if (isa<ParmVarDecl>(t)) {
        auto* context = t->getParentFunctionOrMethod();
        if (context && context->getDeclKind() == Decl::Function) {
          auto* function = cast<FunctionDecl>(context);
          if (function && function->isThisDeclarationADefinition()) {
            recorder_.CodeDefines(*v, *first, v->getDeclKindName(),
                        v->getQualifiedNameAsString(), v->getAccess(),
                        v->getLinkageInternal());
          }
        }
      } else {
        // FIXME: 'extern' variables are declarations. But so are some form of
        // static attributes and similar? This code could be better.
        if (t->hasExternalStorage()) {
          recorder_.CodeDeclares(*v, *first, v->getDeclKindName(),
                       v->getQualifiedNameAsString(), v->getAccess(),
                       v->getLinkageInternal());
        } else {
          recorder_.CodeDefines(*v, *first, v->getDeclKindName(),
                      v->getQualifiedNameAsString(), v->getAccess(),
                      v->getLinkageInternal());
        }
      }
    } else {
      recorder_.CodeDefines(*v, *v, v->getDeclKindName(), v->getQualifiedNameAsString(),
                  v->getAccess(), v->getLinkageInternal());
    }
    return Base::VisitNamedDecl(v);
  }

  bool VisitDeclStmt(DeclStmt* stmt) {
    if (gl_verbose) std::cerr << recorder_.TryPrint(stmt) << std::endl;

    // The first declaration in the statement that has a type should link the
    // type. Example:
    //
    //   Point foo, *bar;
    //
    // The declaration of 'foo' only contains the right offsets to find 'Point',
    // so that should be linked to the Point type.
    for (auto& decl : stmt->decls()) {
      auto* decldecl = dyn_cast<DeclaratorDecl>(decl);
      if (decldecl) {
        auto* rt = recorder_.GetTagDeclForType(decldecl->getType());
        recorder_.LinkToType(decldecl->getTypeSourceInfo()->getTypeLoc(), "declaration",
                   rt);
        break;
      }
    }
    return Base::VisitDeclStmt(stmt);
  }

// TODO: compound statement annotations have been disabled for some time.
//       Fix the code here to enable them again.
//  bool TraverseCompoundStmt(CompoundStmt* statement) {
//    static int depth = 0;
//
//    // Find location with start of column.
//    const auto& sm = ci_->getSourceManager();
//    auto start = statement->getBeginLoc();
//    auto fid = sm.getFileID(start);
//
//    if (!start.isMacroID()) {
//      auto line = sm.getExpansionLineNumber(start);
//      if (line <= 1) {
//        std::cerr << "ERROR: SKIPPING INVALID LINE" << std::endl;
//        return Base::TraverseCompoundStmt(statement);
//      }
//      auto cache = sm.getSLocEntry(fid).getFile().getContentCache();
//      auto offset = cache->SourceLineCache[line - 1];
//
//      auto max = cache->NumLines;
//      if (line >= max) {
//        std::cerr << "ERROR: SKIPPiNG INVALID LINE" << std::endl;
//        return Base::TraverseCompoundStmt(statement);
//      }
//
//      // auto& buffer = rewriter_->getEditBuffer(fid);
//      std::string div =
//          "<div class='compound level-" + std::to_string(depth) + "'>";
//      // buffer.InsertText(offset, div, false);
//
//      ++depth;
//      auto result = Base::TraverseCompoundStmt(statement);
//      --depth;
//      // rewriter_->InsertTextAfterToken(statement->getLocEnd(), "</div>");
//      return result;
//    }
//
//    return Base::TraverseCompoundStmt(statement);
//  }

 private:
  SbexrRecorder recorder_;
};

// Implementation of the ASTConsumer interface for reading an AST produced
// by the Clang parser.
class SbexrAstConsumer : public ASTConsumer {
 public:
  SbexrAstConsumer(FileCache* cache, Indexer* index) : visitor_(cache, index) {}
  SbexrAstVisitor* GetVisitor() { return &visitor_; }

  void HandleTranslationUnit(ASTContext& context) override {
    if (gl_verbose) std::cerr << "ENTERING TRANSLATION UNIT\n";

    auto* turd = context.getTranslationUnitDecl();
    if (gl_verbose) turd->dump(); // giggling... could not resist.
    visitor_.TraverseDecl(turd);

    if (gl_verbose) std::cerr << "EXITING TRANSLATION UNIT\n";
  }

 private:
  SbexrAstVisitor visitor_;
};


#endif
