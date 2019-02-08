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

#include "base.h"
#include "indexer.h"
#include "printer.h"
#include "wrapping.h"

// TODO:
//   P0 - compound blocks insert html in random places, pretty much breaking
//        <span> or others that might happen in between.
//   P0 - type should link to the definition (eg, __m_buf? should link to the
//        typedef corresponding or struct corresponding)
//   P0 - auto types should show the real underlying type
//   P0 - macros should be expandable to their real value
//   P0 - staticmethods like CompilationDatabase::loadFromDirectory should link
//   to class and method, two links.
//   P0 - path at top of file should be broken down
//   P0 - template parameters should link back to the original type
//   P0 - show tooltip for type tracking -> eg, typedef foo bar; typedef bar
//   int;
//        foo test = 5; tooltip for foo should show 'foo -> bar -> int'
//   P1 - for templates, track which parameter they are instantiated with?
//
//   P0 - function argument types should point back to the type definition
//   P0 - #ifdef blocks disabled should be grayed out.
//
// ACTIONS:
// - click on variable:
//   - variable definition?
//   - variable declaration?
//   - variable type?
//   - variable value?
//   - other uses of that variable?
//   - where is variable modified?
//   - functions usable on that type?
//   - comment on top of variable?
//
// STRUCT FIELDS
// - struct it is defined into
// - type of field
// - comments on top of field
// - comments on top of struct
//
//
//
// ./clang/lib/Frontend/Rewrite/HTMLPrint.cpp
// CommentVisitor

// Scoping:
//   - variable names: links are scoped. Clicking on hte name brings to the
//   definition within the scope.
//   - function names: static in .cc are per file, non static are global.
//   - class definitions: in .cc file, they are per file?
//
// Two parts of the problem:
// - identifying position of objects, linking to objects.
//
// How does clang work:
// - Preprocessor produces tokens, for anything in the file.
// - parser reads one token at a time, and feeds it to the consumer.
//
// Accessing the Lexer:
// - 2 kind of lexers: raw lexer, rough, not very good. expensive lexe.
// - Preprocessor contains a lexer. As the parser reads from the file,
//   the lexer is advanced. However:
//   - preprocessor, to start, needs a call to EnterMainFile or similar.
//     only one call is allowed. So can't reuse the preprocessor.
//   - lexer in preprocessor can't be seeked back to start.
//
// Alaternatives:
// - create a new preprocessor
// - create a new lexer
//
//

cl::OptionCategory gl_category("Useful commands");
cl::opt<bool> gl_verbose("verbose", cl::desc("Provide debug output."),
                         cl::cat(gl_category), cl::init(false));
cl::opt<int> gl_limit("limit", cl::desc("Limit the number of files processed."),
                      cl::cat(gl_category), cl::init(0));
cl::opt<int> gl_snippet_limit(
    "snippet-limit", cl::desc("Maximum number of characters captured in a "
                              "snippet before or after the relevant text."),
    cl::cat(gl_category), cl::init(60));
cl::opt<std::string> gl_output_cwd(
    "c", cl::desc("Path to strip from generated filenames."),
    cl::value_desc("directory"), cl::init(GetCwd()), cl::cat(gl_category));
cl::opt<std::string> gl_index_dir(
    "index", cl::desc("Directory where to output all generated indexes. Tag "
                      "name is used to name files."),
    cl::value_desc("directory"), cl::cat(gl_category));
cl::opt<std::string> gl_bear_filter_regex(
    "l",
    cl::desc(
        "Regex describing which files to parse from the compilation database."),
    cl::value_desc("regex"), cl::cat(gl_category));
cl::opt<std::string> gl_input_dir(cl::Positional, cl::desc("<input directory>"),
                                  cl::Required);

std::string MakeIdLink(const SourceManager& sm, FileCache* cache,
                       const SourceRange& range) {
  std::string prefix(MakeHtmlPath(cache->GetFileHashFor(sm, range.getBegin())));
  prefix.append("#");
  prefix.append(MakeIdName(sm, range));

  return prefix;
}

SourceRange NormalizeSourceRange(const SourceRange& range) {
  if (!range.getEnd().isValid())
    return SourceRange(range.getBegin(), range.getBegin());
  return range;
}

std::unique_ptr<Lexer> CreateLexer(const CompilerInstance* ci,
                                   SourceLocation location);

// By implementing RecursiveASTVisitor, we can specify which AST nodes
// we're interested in by overriding relevant methods.
class SbexrAstVisitor : public RecursiveASTVisitor<SbexrAstVisitor> {
 public:
  using Base = RecursiveASTVisitor<SbexrAstVisitor>;
  SbexrAstVisitor(FileCache* cache, Indexer* indexer)
      : cache_(cache), index_(indexer) {}

  bool shouldVisitTemplateInstantiations() const { return true; }
  // Setting this to true will get the visitor to enter things like
  // implicitly defined constructors, but also things like "using" directives
  // that bring in more details.
  // bool shouldVisitImplicitCode() const { return true; }

  std::string MakeIdLink(SourceRange location) {
    return ::MakeIdLink(ci_->getSourceManager(), cache_, location);
  }
  std::string MakeIdName(SourceRange location) {
    return ::MakeIdName(ci_->getSourceManager(), location);
  }
  FileRenderer::ParsedFile* GetFileFor(SourceLocation location) {
    return cache_->GetFileFor(ci_->getSourceManager(), location);
  }
  StringRef GetNormalizedPath(SourceLocation location) {
    return cache_->GetNormalizedPath(ci_->getSourceManager(), location);
  }
  template <typename TypeT>
  std::string PrintLocation(const TypeT& range) {
    const auto& sm = ci_->getSourceManager();
    return ::PrintLocation(sm, cache_, range);
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

  template <typename TypeT>
  std::string TryPrint(const TypeT* v) {
    return printer_.Print(v);
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

  template<typename UserT>
  void RecordTypeUse(const UserT& user, const char* description, TagDecl* target) {
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

  void SetParameters(const CompilerInstance* ci) {
    ci_ = ci;
    printer_ = Printer(*ci, cache_);
  }
  const CompilerInstance& GetCompilerInstance() const { return *ci_; }

  bool TraverseMemberExpr(MemberExpr* e) {
    if (gl_verbose) {
      std::cerr << "MEMBEREXPR " << PrintLocation(e->getSourceRange())
                << PrintLocation(e->getMemberNameInfo().getSourceRange())
                << std::endl;
      e->dump();
    }
    CodeUses(e->getMemberNameInfo(), "expression", *e->getFoundDecl());
    return Base::TraverseMemberExpr(e);
  }
  bool TraverseDecl(Decl* decl) {
    if (!decl) return Base::TraverseDecl(decl);

    auto* file = GetFileFor(decl->getBeginLoc());
    if (file && file->Rendered()) {
      if (gl_verbose)
        std::cerr << "FILE ALREADY PARSED " << TryPrint(decl) << std::endl;
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
                << PrintLocation(e->getFoundDecl()->getSourceRange())
                << std::endl;
    CodeUses(*e, "variable", *e->getFoundDecl());
    return Base::TraverseDeclRefExpr(e);
  }

  bool VisitDeclaratorDecl(DeclaratorDecl* v) {
    if (!v) return true;
// Notes:
// getName -> returns the name of the variable declared.
// getTypeSourceInfo -> returns the original type declared in the source code
// (eg, 'auto')
// getType -> returns the type the variable is actually considered to be.

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
                << PrintLocation(v->getSourceRange()) << " "
                << PrintLocation(decl->getSourceRange());
      std::cerr << std::endl;
      std::cerr << "DeclaratorDecl: " << TryPrint(v) << std::endl;
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
        std::cerr << "getTypeSourceInfo: " << TryPrint(v->getTypeSourceInfo())
                  << std::endl;

      // If this is a VarDecl in a DeclStmt, then it only needs to be recorded.
      // The type link was already created by the DeclStmt.
      auto* vt = dyn_cast<VarDecl>(v);
      if (vt && vt->isLocalVarDecl()) {
        auto* rt = GetTagDeclForType(v->getType());
        RecordTypeUse(v->getTypeSourceInfo()->getTypeLoc(), "declaration", rt);
      } else {
        CodeUsesQualType(v->getTypeSourceInfo()->getTypeLoc(), "declaration",
                         v->getType());
      }
    }
    return Base::VisitDeclaratorDecl(v);
  }
  bool VisitUsingDecl(UsingDecl* v) {
    // FIXME: do something smart on using declarations.
    if (gl_verbose) std::cerr << TryPrint(v) << std::endl;
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

    if (gl_verbose) std::cerr << TryPrint(v) << std::endl;

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
      CodeUsesQualType(f->getReturnTypeSourceRange(), "return",
                       f->getReturnType());

      auto* first = f->getFirstDecl();
      if (!first) first = f;
      if (gl_verbose) std::cerr << TryPrint(f) << "\n";

      if (f->isThisDeclarationADefinition()) {
        CodeDefines(*v, *first, v->getDeclKindName(),
                    v->getQualifiedNameAsString(), v->getAccess(),
                    v->getLinkageInternal());
      } else {
        CodeDeclares(*v, *first, v->getDeclKindName(),
                     v->getQualifiedNameAsString(), v->getAccess(),
                     v->getLinkageInternal());
      }
    } else if (isa<TagDecl>(v)) {
      auto* t = cast<TagDecl>(v);

      auto* first = t->getFirstDecl();
      if (!first) first = t;

      if (t->isCompleteDefinition()) {
        CodeDefines(*v, *first, v->getDeclKindName(),
                    v->getQualifiedNameAsString(), v->getAccess(),
                    v->getLinkageInternal());
      } else {
        CodeDeclares(*v, *first, v->getDeclKindName(),
                     v->getQualifiedNameAsString(), v->getAccess(),
                     v->getLinkageInternal());
      }
    } else if (isa<VarDecl>(v)) {
      auto* t = cast<VarDecl>(v);
      if (gl_verbose) std::cerr << TryPrint(t) << std::endl;

      auto* first = t->getFirstDecl();
      if (!first) first = t;

      if (isa<ParmVarDecl>(t)) {
        auto* context = t->getParentFunctionOrMethod();
        if (context && context->getDeclKind() == Decl::Function) {
          auto* function = cast<FunctionDecl>(context);
          if (function && function->isThisDeclarationADefinition()) {
            CodeDefines(*v, *first, v->getDeclKindName(),
                        v->getQualifiedNameAsString(), v->getAccess(),
                        v->getLinkageInternal());
          }
        }
      } else {
        // FIXME: 'extern' variables are declarations. But so are some form of
        // static attributes and similar? This code could be better.
        if (t->hasExternalStorage()) {
          CodeDeclares(*v, *first, v->getDeclKindName(),
                       v->getQualifiedNameAsString(), v->getAccess(),
                       v->getLinkageInternal());
        } else {
          CodeDefines(*v, *first, v->getDeclKindName(),
                      v->getQualifiedNameAsString(), v->getAccess(),
                      v->getLinkageInternal());
        }
      }
    } else {
      CodeDefines(*v, *v, v->getDeclKindName(), v->getQualifiedNameAsString(),
                  v->getAccess(), v->getLinkageInternal());
    }
    return Base::VisitNamedDecl(v);
  }

#if 0
  bool VisitDecl(Decl* decl) { return Base::VisitDecl(decl); }
  bool VisitStmt(Stmt* stmt) { return Base::VisitStmt(stmt); }
#endif
  bool VisitDeclStmt(DeclStmt* stmt) {
    if (gl_verbose) std::cerr << TryPrint(stmt) << std::endl;
    
    // The first declaration in the statement that has a type should link the type.
    // Example:
    //
    //   Point foo, *bar;
    //
    // The declaration of 'foo' only contains the right offsets to find 'Point',
    // so that should be linked to the Point type.
    for (auto& decl : stmt->decls()) {
      auto* decldecl = dyn_cast<DeclaratorDecl>(decl);
      if (decldecl) {
        auto* rt = GetTagDeclForType(decldecl->getType());
        LinkToType(decldecl->getTypeSourceInfo()->getTypeLoc(), "declaration", rt);
        break;
      }
    }
    return Base::VisitDeclStmt(stmt);
  }

  bool TraverseCompoundStmt(CompoundStmt* statement) {
    static int depth = 0;

    // Find location with start of column.
    const auto& sm = ci_->getSourceManager();
    auto start = statement->getBeginLoc();
    auto fid = sm.getFileID(start);

    if (!start.isMacroID()) {
      auto line = sm.getExpansionLineNumber(start);
      if (line <= 1) {
        std::cerr << "ERROR: SKIPPiNG INVALID LINE" << std::endl;
        return Base::TraverseCompoundStmt(statement);
      }
      auto cache = sm.getSLocEntry(fid).getFile().getContentCache();
      auto offset = cache->SourceLineCache[line - 1];

      auto max = cache->NumLines;
      if (line >= max) {
        std::cerr << "ERROR: SKIPPiNG INVALID LINE" << std::endl;
        return Base::TraverseCompoundStmt(statement);
      }

      // auto& buffer = rewriter_->getEditBuffer(fid);
      std::string div =
          "<div class='compound level-" + std::to_string(depth) + "'>";
      // buffer.InsertText(offset, div, false);

      ++depth;
      auto result = Base::TraverseCompoundStmt(statement);
      --depth;
      // rewriter_->InsertTextAfterToken(statement->getLocEnd(), "</div>");
      return result;
    }

    return Base::TraverseCompoundStmt(statement);
  }

 private:
  FileCache* cache_;
  Indexer* index_;

  const CompilerInstance* ci_ = nullptr;
  Printer printer_;
};

struct ToParse {
  ToParse(const std::string& file,
          const std::vector<std::string>& argv = std::vector<std::string>())
      : file(file), argv(argv) {}
  ToParse() = default;

  std::string file;
  const std::vector<std::string> argv;
};

class PPTracker : public PPCallbacks {
 public:
  PPTracker(FileCache* cache, const CompilerInstance& ci)
      : cache_(cache), ci_(ci) {}
  virtual ~PPTracker() = default;

  void FileChanged(SourceLocation loc, FileChangeReason reason,
                   SrcMgr::CharacteristicKind kind, FileID prevfid) override {
    if (reason == EnterFile) {
      if (!ShouldProcess()) {
        include_ignored_++;
        return;
      }

      auto file = cache_->GetFileFor(ci_.getSourceManager(), loc);
      if (file) {
        if (file->preprocessing) {
          include_ignored_++;
          return;
        }
        file->preprocessing = true;
      }

      if (gl_verbose)
        std::cerr << "#ENTERING "
                  << (file ? file->path : std::string("<INVALID>"))
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
           (include_stack_.top() && !include_stack_.top()->preprocessed);
  }

  void InclusionDirective(SourceLocation loc, const Token& IncludeTok,
                          StringRef FileName, bool IsAngled,
                          CharSourceRange filename_range, const FileEntry* File,
                          StringRef SearchPath, StringRef RelativePath,
                          const clang::Module* Imported,
                          SrcMgr::CharacteristicKind FileType) override {
    if (!File || !ShouldProcess()) return;

    auto included_full_path = (SearchPath + "/" + RelativePath).str();
    auto* file_descriptor = cache_->GetFileFor(included_full_path);
    auto html_path = file_descriptor->HtmlPath();

    WrapWithTag(ci_, cache_, filename_range.getAsRange(),
                std::move(MakeTag("a", {"include"}, {"href", html_path})));

    if (gl_verbose)
      std::cerr << "#INCLUDING "
                << PrintLocation(ci_.getSourceManager(), cache_, loc) << " "
                << included_full_path << " " << file_descriptor->path
                << " from "
                << cache_->GetNormalizedPath(ci_.getSourceManager(),
                                             filename_range.getBegin())
                << std::endl;
  }

  /// \brief Called by Preprocessor::HandleMacroExpandedIdentifier when a
  /// macro invocation is found.
  void MacroExpands(const Token& MacroNameTok, const MacroDefinition& MD,
                    SourceRange Range, const MacroArgs* Args) override{};

  /// \brief Hook called whenever a macro definition is seen.
  void MacroDefined(const Token& MacroNameTok,
                    const MacroDirective* MD) override{};

  /// \brief Hook called whenever a macro \#undef is seen.
  ///
  /// MD is released immediately following this callback.
  void MacroUndefined(const Token& MacroNameTok,
                      const MacroDefinition& MD,
                      const MacroDirective *Undef) override{};

  /// \brief Hook called when a source range is skipped.
  /// \param Range The SourceRange that was skipped. The range begins at the
  /// \#if/\#else directive and ends after the \#endif/\#else directive.
  void SourceRangeSkipped(SourceRange Range, SourceLocation endifloc) override{};

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

// Implementation of the ASTConsumer interface for reading an AST produced
// by the Clang parser.
class SbexrAstConsumer : public ASTConsumer {
 public:
  SbexrAstConsumer(FileCache* cache, Indexer* index) : visitor_(cache, index) {}
  SbexrAstVisitor* GetVisitor() { return &visitor_; }

  void HandleTranslationUnit(ASTContext& context) override {
    if (gl_verbose) std::cerr << "ENTERING TRANSLATION UNIT\n";
    visitor_.TraverseDecl(context.getTranslationUnitDecl());
  }

#if 0
  bool HandleTopLevelDecl(DeclGroupRef DR) override {
    if (gl_verbose) std::cerr << "TOP LEVEL DECLARATION ";
    int i = 0;
    for (DeclGroupRef::iterator b = DR.begin(), e = DR.end(); b != e; ++b) {
      if (gl_verbose && i++ == 0)
        std::cerr << PrintLocation(
                         visitor_.GetCompilerInstance().getSourceManager(),
                         (*b)->getSourceRange())
                  << std::endl;
      // Traverse the declaration using our AST visitor.
      visitor_.TraverseDecl(*b);
    }
    if (gl_verbose && i++ == 0) std::cerr << "EMPTY" << std::endl;

    return true;
  }
#endif

 private:
  SbexrAstVisitor visitor_;
};

#if 0
std::string GetSourceRange(const SourceManager& sm, const SourceRange& range) {
  const auto& begin = range.getBegin();
  const auto& end = range.getEnd();

  const auto fid = sm.getFileID(end);
  if (fid != sm.getFileID(begin)) {
    if (gl_verbose)
      std::cerr << "BEGIN AND END IN DIFFERENT FILES: "
                << sm.getFilename(begin).str() << " vs "
                << sm.getFilename(end).str() << std::endl;
    return "<different-files>";
  }

  bool invalid = false;
  const auto& buffer = sm.getBufferData(fid, &invalid);
  const char* data = buffer.data();
  if (invalid) {
    if (gl_verbose) std::cerr << "getBufferData returned invalid" << std::endl;
    return "<invalid-range>";
  }

  const auto begin_offset = sm.getFileOffset(begin);
  const auto end_offset = sm.getFileOffset(end);
  return std::string(data + begin_offset, end_offset - begin_offset + 1);
}
#endif

std::unique_ptr<Lexer> CreateLexer(const CompilerInstance* ci,
                                   SourceLocation location) {
  const auto& sm = ci->getSourceManager();

  location = sm.getExpansionLoc(location);
  std::pair<FileID, unsigned> info = sm.getDecomposedLoc(location);
  bool invalid = false;
  auto buffer = sm.getBufferData(info.first, &invalid);
  if (invalid) return nullptr;

  const char* data = buffer.data() + info.second;

  auto lexer = llvm::make_unique<Lexer>(sm.getLocForStartOfFile(info.first),
                                        ci->getLangOpts(), buffer.begin(), data,
                                        buffer.end());

  lexer->SetCommentRetentionState(true);
  // lexer->LexFromRawLexer(Result);
  return std::move(lexer);
}

std::unique_ptr<CompilerInstance> CreateCompilerInstance(
    const std::vector<std::string>& argv, FileManager* fm = nullptr,
    SourceManager* sm = nullptr) {
  // CompilerInstance will hold the instance of the Clang compiler for us,
  // managing the various objects needed to run the compiler.
  auto ci = llvm::make_unique<CompilerInstance>();
  auto& options = ci->getDiagnosticOpts();
  options.ShowCarets = 0;
  ci->createDiagnostics(new TextDiagnosticPrinter(llvm::outs(), &options),
                        true);

  LangOptions& lo = ci->getLangOpts();
  // Without this, does not recognize bool and wchar_t, and a few other errors.
  CompilerInvocation::setLangDefaults(lo, InputKind(InputKind::CXX, InputKind::Source, false),
                                      llvm::Triple(ci->getTargetOpts().Triple),
                                      ci->getPreprocessorOpts());

  if (argv.size()) {
    std::vector<const char*> args;
    for (const auto& arg : argv) args.push_back(arg.c_str());

    auto invocation =
        createInvocationFromCommandLine(args, &ci->getDiagnostics());
    ci->setInvocation(std::move(invocation));
  } else {
    lo.CPlusPlus = 1;
  }

  lo.CommentOpts.ParseAllComments = true;
  // Initialize target info with the default triple for our platform.
  auto target_options = std::make_shared<::clang::TargetOptions>();
  target_options->Triple = llvm::sys::getDefaultTargetTriple();
  TargetInfo* target_info =
      TargetInfo::CreateTargetInfo(ci->getDiagnostics(), target_options);
  ci->setTarget(target_info);

  if (!fm)
    ci->createFileManager();
  else
    ci->setFileManager(fm);

  FileManager& file_mgr = ci->getFileManager();
  if (!sm)
    ci->createSourceManager(file_mgr);
  else
    ci->setSourceManager(sm);

  // ci->createPreprocessor(TU_Module);
  ci->createPreprocessor(TU_Complete);
  ci->createASTContext();

  // PPCallbacks -> to track all includes and similar.

  Preprocessor& pp = ci->getPreprocessor();
  if (gl_verbose) {
    const auto& hs = pp.getHeaderSearchInfo();
    for (auto sd = hs.system_dir_begin(); sd != hs.system_dir_end(); ++sd) {
      llvm::errs() << "+ HEADER SEARCH DIR: " << sd->getName() << "\n";
    }
  }

  pp.getBuiltinInfo().initializeBuiltins(pp.getIdentifierTable(),
                                         pp.getLangOpts());
  return ci;
}

int main(int argc, const char** argv) {
  HideUnrelatedOptions(&gl_category);
  FileRenderer::InitFlags();

  cl::ParseCommandLineOptions(
      argc, argv, "Indexes and generates HTML files for your source code.");

  LLVMInitializeX86TargetInfo();
  LLVMInitializeX86TargetMC();
  LLVMInitializeX86AsmParser();

  // TODO: use ParseCommandLineOptions to parse command line
  // TODO: Create a CompilationDatabase object to load the json file.
  // TODO: use approach here http://fdiv.net/2012/08/15/compiling-code-clang-api
  //   to pass the options to a CompilerInstance. Namely:
  //
  // // The compiler invocation needs a DiagnosticsEngine so it can report
  // problems
  // clang::TextDiagnosticPrinter *DiagClient = new
  // clang::TextDiagnosticPrinter(llvm::errs(), clang::DiagnosticOptions());
  // llvm::IntrusiveRefCntPtr<clang::DiagnosticIDs> DiagID(new
  // clang::DiagnosticIDs());
  // clang::DiagnosticsEngine Diags(DiagID, DiagClient);
  //
  // llvm::OwningPtr<clang::CompilerInvocation> CI(new
  // clang::CompilerInvocation);
  //
  // clang::CompilerInvocation::CreateFromArgs(*CI, &args[0], &args[0] +
  // args.size(), Diags);
  // clang::CompilerInstance Clang;
  // Clang.setInvocation(CI.take());
  // cl::ParseCommandLineOptions(argc, argv);

  std::string error;
  // A Rewriter helps us manage the code rewriting task.
  std::list<ToParse> to_parse;
  std::regex filter(gl_bear_filter_regex.empty() ? std::string(".*")
                                                 : gl_bear_filter_regex);

  {
    auto db = CompilationDatabase::loadFromDirectory(gl_input_dir, error);
    if (db == nullptr) {
      llvm::errs() << "ERROR " << error << "\n";
      return 2;
    }

    const auto& allfiles = db->getAllFiles();
    const auto& commands = db->getAllCompileCommands();
    llvm::errs() << ">>> FILES TO PARSE: " << allfiles.size() << "\n";
    llvm::errs() << ">>> COMMANDS TO RUN: " << commands.size() << "\n";

    for (const auto& file : allfiles) {
      if (!std::regex_search(file, filter)) continue;

      const auto& commands = db->getCompileCommands(file);
      for (const auto& command : commands) {
        to_parse.emplace_back(file, command.CommandLine);

        const auto& argv = command.CommandLine;
        llvm::errs() << "ARGV " << file << " " << to_parse.back().argv.size()
                     << " ";
        for (const auto& arg : argv) llvm::errs() << arg << " ";
        llvm::errs() << "\n";
      }
    }
  }

  // Create an AST consumer instance which is going to get called by
  // ParseAST.
  FileRenderer renderer(gl_output_cwd);
  FileCache cache(&renderer);

  Indexer indexer(&cache);
  SbexrAstConsumer consumer(&cache, &indexer);

  if (gl_limit > 0 && static_cast<size_t>(gl_limit) < to_parse.size())
    to_parse.resize(gl_limit);

  while (!to_parse.empty()) {
    MemoryPrinter::OutputStats();

    const auto parsing = std::move(to_parse.front());
    to_parse.pop_front();

    const auto& filename = cache.GetFileFor(parsing.file)->path;

    std::cerr << to_parse.size() << " PARSING " << filename << " "
              << parsing.file << " " << parsing.argv.size() << std::endl;

    {
      auto nci = CreateCompilerInstance(parsing.argv);
      auto& sm = nci->getSourceManager();
      auto& pp = nci->getPreprocessor();
      auto* input = nci->getFileManager().getFile(parsing.file);
      if (!input) {
        std::cerr << "COULD NOT FIND " << filename << "(" << parsing.file << ")"
                  << std::endl;
        continue;
      }
      auto fid = sm.createFileID(input, SourceLocation(), SrcMgr::C_User);
      sm.setMainFileID(fid);

      nci->getDiagnosticClient().BeginSourceFile(nci->getLangOpts(), &pp);
      pp.addPPCallbacks(llvm::make_unique<PPTracker>(&cache, *nci.get()));
      consumer.GetVisitor()->SetParameters(nci.get());

      // Parse the file to AST, registering our consumer as the AST consumer.
      Sema sema(pp, nci->getASTContext(), consumer, TU_Complete, nullptr);
      ParseAST(sema);

      // Get the list of FIDs parsed so far out of the SourceManager.
      for (auto it = sm.fileinfo_begin(); it != sm.fileinfo_end(); ++it) {
        auto fid = sm.translateFile(it->getFirst());
        if (!fid.isValid()) std::cerr << "UNEXPECTED INVALID FID";
        if (gl_verbose)
          std::cerr << "RENDERING FILE " << cache.GetFileFor(sm, fid)->name
                    << std::endl;
        renderer.RenderFile(sm, cache.GetFileFor(sm, fid), fid, pp);
      }
    }
  }

  std::cerr << ">>> GENERATING INDEX" << std::endl;
  indexer.OutputTree();
  indexer.OutputJsonIndex();
  if (!gl_index_dir.empty())
    indexer.OutputBinaryIndex(gl_index_dir.c_str(), gl_tag.c_str());
  indexer.Clear();

  MemoryPrinter::OutputStats();

  std::cerr << ">>> EMBEDDING FILES" << std::endl;
  renderer.ScanTree(gl_input_dir);
  renderer.OutputTree();
  renderer.OutputJsonIndex();
  MemoryPrinter::OutputStats();

  const auto& index = MakeMetaPath("index.html");
  if (!MakeDirs(index, 0777)) {
    std::cerr << "FAILED TO MAKE DIRS " << index << std::endl;
  } else {
    const auto* dir = renderer.GetDirectoryFor(gl_input_dir);
    const auto& entry = dir->HtmlPath();
    unlink(index.c_str());
    symlink(entry.c_str(), index.c_str());
    std::cerr << ">>> ENTRY POINT " << index << " aka " << entry << std::endl;
  }

  return 0;
}
