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

#ifndef PRINTER_H
#define PRINTER_H

// GetSourceRangeOrFail will return a SourceRange from an object
// or fail to compile.
template <typename AnyT>
static inline SourceRange GetSourceRangeOrFail(const AnyT& any) {
  return any.getSourceRange();
}
template <>
inline SourceRange GetSourceRangeOrFail<SourceRange>(
    const SourceRange& any) {
  return any;
}

// Magic: SFINAE will fail to find the template if any.getSourceRange() does not
// exist (the decltype will be invalid).
// If it succeeds, though, this function will be preferred to the one taking
// long as a parameter.
template <typename AnyT>
static inline auto GetSourceRange_impl(const AnyT& any, int)
    -> decltype(any.getSourceRange(), SourceRange()) {
  return any.getSourceRange();
}
template <typename AnyT>
static inline SourceRange GetSourceRange_impl(const AnyT& any, long) {
  return SourceRange();
}

template <>
inline SourceRange GetSourceRange_impl<SourceRange>(
    const SourceRange& any, long) {
  return any;
}

template <typename AnyT>
static inline SourceRange GetSourceRange(const AnyT& any) {
  return GetSourceRange_impl(any, 0);
}

static inline void PrintLineNumbers(raw_ostream& s, const SourceManager& sm,
                                    SourceLocation location) {
  s << sm.getExpansionLineNumber(location);
  s << ":" << sm.getExpansionColumnNumber(location);
}

static inline void PrintSpellingLineNumbers(raw_ostream& s,
                                            const SourceManager& sm,
                                            SourceLocation location) {
  s << sm.getSpellingLineNumber(location);
  s << ":" << sm.getSpellingColumnNumber(location);
}

// SpellingLocation -> the place where the bytes shown have been typed.
//    If the code is "extern 'C' {" from macro EXTERN, Spelling Location
//    is next to the macro definition, where "extern 'C' was typed.
//
// ExpansionLocation -> the place where the bytes shown have been put
//    by the compiler. In the example above, where EXTERN was used.
//
// Now, a location can be:
//   - isInvalid -> 0 value, not initialized.
//   - isFileID -> true if it refers to a file. It means that this location
//     has neither a corresponding ExpansionLocation nor SpellingLocation.
//     Both getters will return the Location as is.
//   - isMacroID -> true if this refers to a macro. In this case, both
//     getExpansionLoc and getSpellingLoc will walk up the expansion stack
//     to return the original definition.
//
// Now we have:
//
//   PresumedLoc and getPresumedLoc, which are supposed to represent the
//   ExpansionLocation (eg, where the code is being processed) and lookup
//   all human interesting information about that location.
//   (eg, file name, column and line number, all in one go).
static inline std::string PrintCode(const SourceManager& sm,
                                    const SourceRange& range) {
  FileID bid;
  unsigned boffset;
  std::tie(bid, boffset) = sm.getDecomposedExpansionLoc(range.getBegin());
  if (!bid.isValid()) return "<no-code:invalid-begin-file>";

  FileID eid;
  unsigned eoffset;
  std::tie(eid, eoffset) = sm.getDecomposedExpansionLoc(range.getEnd());
  if (!eid.isValid()) return "<no-code:invalid-end-file>";
  if (eid != bid) return "<no-code:different-begin-end-file>";

  bool invalid = false;
  const auto& buffer = sm.getBufferData(bid, &invalid);
  if (invalid) return "<no-code:invalid-buffer>";

  const char* data = buffer.data();
  if (boffset >= buffer.size()) return "<no-code:invalid-begin-offset>";
  if (eoffset >= buffer.size()) return "<no-code:invalid-end-offset>";
  if (eoffset < boffset) return "<no-code:end-offset-smaller-than-begin>";

  return std::string(data + boffset, eoffset - boffset);
}

static inline std::string PrintLocation(const SourceManager& sm,
                                        FileCache* cache,
                                        SourceRange location) {
  if (!location.isValid()) return "<invalid-location>";

  auto sf = cache->GetFileFor(sm, location.getBegin());
  auto ef = cache->GetFileFor(sm, location.getEnd());

  std::string output(GetFilePath(sf));
  raw_string_ostream s(output);

  PrintLineNumbers(s, sm, location.getBegin());
  s << "-";
  if (sf != ef) s << GetFilePath(ef);
  PrintLineNumbers(s, sm, location.getEnd());
  return s.str();
}

static inline std::string PrintLocation(const SourceManager& sm,
                                        FileCache* cache,
                                        SourceLocation location) {
  std::string output(GetFilePath(cache->GetFileFor(sm, location)));
  raw_string_ostream s(output);
  s << ":";
  PrintLineNumbers(s, sm, location);
  return s.str();
}

static inline std::string PrintSpellingLocation(const SourceManager& sm,
                                                FileCache* cache,
                                                SourceRange location) {
  if (!location.isValid()) return "<invalid-location>";

  auto sf = cache->GetSpellingFileFor(sm, location.getBegin());
  auto ef = cache->GetSpellingFileFor(sm, location.getEnd());

  std::string output(sf->path);
  raw_string_ostream s(output);

  PrintSpellingLineNumbers(s, sm, location.getBegin());
  s << "-";
  if (sf != ef) s << ef->path;
  PrintSpellingLineNumbers(s, sm, location.getEnd());
  return s.str();
}

static inline std::string PrintSpellingLocation(const SourceManager& sm,
                                                FileCache* cache,
                                                SourceLocation location) {
  std::string output(GetFilePath(cache->GetSpellingFileFor(sm, location)));
  raw_string_ostream s(output);
  s << ":";
  PrintSpellingLineNumbers(s, sm, location);
  return s.str();
}

#define PRINT_RECURSE_BASE(type, var) \
  s << TryPrint(static_cast<const type*>(var), ctx, false)
#define PRINT_RECURSE(var) s << TryPrint(var, ctx, true)
#define PRINT_RECURSE_TMP(base, method) \
  {                                     \
    const auto& __var = base->method(); \
    PRINT_RECURSE(&__var);              \
  }
#define PRINT_RECURSE_GUARD(base, check, method) \
  {                                              \
    if (base->check()) {                         \
      const auto& __var = base->method();        \
      PRINT_RECURSE(&__var);                     \
    }                                            \
  }
#define PRINT_RECURSE_PTR(base, method) \
  {                                     \
    if (base->method()) {               \
      PRINT_RECURSE(base->method());    \
    }                                   \
  }
#define PRINT_RANGE(v, method)         \
  s << (std::string("!" #method "=") + \
        PrintLocation(*sm_, cache_, v->method()) + ",")
#define PRINT_LOCATION(v, method)      \
  s << (std::string("+" #method "=") + \
        PrintLocation(*sm_, cache_, v->method()) + ",")

#define PRINT_RANGE_DIRECT(range) s << PrintLocation(*sm_, cache_, *range) + ","

#define PRINT_NAME_VALUE(name, value) \
  s << (std::string(#name "=") + std::to_string(value) + ",")

#define PRINT_BOOL(base, method) \
  PRINT_NAME_VALUE(#method, ((base)->method() ? "=true" : "=false"))
#define PRINT_STRING(base, method) PRINT_NAME_VALUE(#method, (base)->method())
#define PRINT_INT(base, method) PRINT_NAME_VALUE(#method, (base)->method())

#define PRINT_FUNCTION_BASE_BODY(type, variable, code) \
  {                                                    \
    std::string output;                                \
    raw_string_ostream s(output);                      \
    code s << "}";                                     \
    return s.str();                                    \
  }
#define PRINT_FUNCTION_BODY(type, variable, code)                             \
  PRINT_FUNCTION_BASE_BODY(type, variable, {                                  \
    s << #type << "[" << static_cast<const void*>(variable) << "]"            \
      << "{";                                                                 \
    if (variable) {                                                           \
      if (!suppress || (ctx->printed.find(variable) == ctx->printed.end())) { \
        ctx->printed.insert(variable);                                        \
        code                                                                  \
      } else {                                                                \
        s << "!DUP";                                                          \
      }                                                                       \
    } else {                                                                  \
      s << "!NULL";                                                           \
    };                                                                        \
  })

#define PRINT_FUNCTION_PROTO(name, type, variable) \
  std::string name(const type* variable, Context* ctx, bool suppress)
#define PRINT_FUNCTION(type, variable, code)     \
  PRINT_FUNCTION_PROTO(TryPrint, type, variable) \
  PRINT_FUNCTION_BODY(type, variable, code)

class Printer {
 private:
  struct Context {
    int indentation = 0;
    std::set<const void*> printed;
  };

 public:
  Printer() = default;

  Printer(const CompilerInstance& ci, FileCache* cache)
      : sm_(&ci.getSourceManager()), lo_(&ci.getLangOpts()), cache_(cache) {}
  Printer(const SourceManager* sm, const LangOptions* lo, FileCache* cache)
      : sm_(sm), lo_(lo), cache_(cache) {}

  template <typename TypeT>
  static std::string Print(const CompilerInstance& ci, FileCache* cache,
                           const TypeT* v) {
    Printer printer(ci, cache);
    return printer.Print(v);
  }

  template <typename TypeT>
  std::string Print(const TypeT* v) PRINT_FUNCTION_BASE_BODY(TypeT, v, {
    Context context;
    return Print(v, &context, false);
  });

  template <typename TypeT>
  PRINT_FUNCTION_PROTO(Print, TypeT, v)
  PRINT_FUNCTION_BASE_BODY(TypeT, v, {
    s << "DUMPING " << PrintLocation(*sm_, cache_, GetSourceRange(*v)) << ":";
    PRINT_RECURSE(v);
  });

  PRINT_FUNCTION(DeclStmt, v, {
    PRINT_LOCATION(v, getBeginLoc);
    PRINT_LOCATION(v, getEndLoc);
    if (v->isSingleDecl())
      PRINT_NAME_VALUE("size", 1);
    else
      PRINT_NAME_VALUE("size", v->getDeclGroup().getDeclGroup().size());
  });

  PRINT_FUNCTION(TagDecl, v, {
    PRINT_RANGE(v, getBraceRange);
    PRINT_RANGE(v, getSourceRange);
    PRINT_LOCATION(v, getInnerLocStart);
    PRINT_LOCATION(v, getOuterLocStart);

    PRINT_BOOL(v, isThisDeclarationADefinition);
    PRINT_BOOL(v, isCompleteDefinition);
    PRINT_BOOL(v, isCompleteDefinitionRequired);
    PRINT_BOOL(v, isBeingDefined);
    PRINT_BOOL(v, isEmbeddedInDeclarator);
    PRINT_BOOL(v, isFreeStanding);
    PRINT_BOOL(v, isDependentType);

    PRINT_STRING(v, getKindName);
    PRINT_INT(v, getTagKind);

    PRINT_RECURSE_BASE(TypeDecl, v);
  });

  PRINT_FUNCTION(clang::Type, v, {
    PRINT_BOOL(v, isFromAST);
    PRINT_BOOL(v, containsUnexpandedParameterPack);
    PRINT_BOOL(v, isCanonicalUnqualified);
    PRINT_BOOL(v, isIncompleteType);
    PRINT_BOOL(v, isObjectType);
    // PRINT_BOOL(v, isLiteralType);
    PRINT_BOOL(v, isStandardLayoutType);
    PRINT_BOOL(v, isBuiltinType);
    PRINT_BOOL(v, isPlaceholderType);
    PRINT_BOOL(v, isIntegerType);
    PRINT_BOOL(v, isFunctionType);
    PRINT_BOOL(v, isFunctionProtoType);
    PRINT_BOOL(v, isFunctionNoProtoType);
    PRINT_BOOL(v, isPointerType);
    PRINT_BOOL(v, isAnyPointerType);
    PRINT_BOOL(v, isDependentType);
    PRINT_BOOL(v, isUndeducedType);
    PRINT_BOOL(v, hasPointerRepresentation);
    PRINT_STRING(v, getTypeClassName);

    PRINT_RECURSE_GUARD(v, hasPointerRepresentation, getPointeeType);
    PRINT_RECURSE_PTR(v, getAsTagDecl);

    // getCanonicalTypeUnqualified
  });

  PRINT_FUNCTION(Qualifiers, v, {});

  PRINT_FUNCTION(SplitQualType, v, {
    PRINT_RECURSE(v->Ty);
    PRINT_RECURSE(&v->Quals);
  });

  PRINT_FUNCTION(QualType, v, {
    PRINT_STRING(v, getAsString);
    PRINT_BOOL(v, isCanonical);
    PRINT_BOOL(v, isCanonicalAsParam);
    PRINT_BOOL(v, isNull);
    PRINT_BOOL(v, isLocalConstQualified);
    PRINT_BOOL(v, isConstQualified);

    PRINT_RECURSE_TMP(v, split);
  });

  PRINT_FUNCTION(FunctionDecl, v, {
    PRINT_RANGE(v, getSourceRange);
    PRINT_BOOL(v, hasBody);
    PRINT_BOOL(v, hasTrivialBody);
    PRINT_BOOL(v, isDefined);
    PRINT_BOOL(v, isThisDeclarationADefinition);
    PRINT_BOOL(v, doesThisDeclarationHaveABody);

    PRINT_RECURSE_BASE(DeclaratorDecl, v);
  });
  PRINT_FUNCTION(DeclaratorDecl, v, {
    PRINT_RECURSE_PTR(v, getTypeSourceInfo);

    PRINT_LOCATION(v, getInnerLocStart);
    PRINT_LOCATION(v, getOuterLocStart);
    PRINT_RANGE(v, getSourceRange);
    PRINT_LOCATION(v, getBeginLoc);
    PRINT_LOCATION(v, getTypeSpecStartLoc);
    PRINT_INT(v, getNumTemplateParameterLists);

    PRINT_RECURSE_PTR(v, getQualifier);
    PRINT_RECURSE_BASE(ValueDecl, v);
  });
  PRINT_FUNCTION(ValueDecl, v, {
    PRINT_BOOL(v, isWeak);

    PRINT_RECURSE_TMP(v, getType);
    PRINT_RECURSE_BASE(NamedDecl, v);
  });

  PRINT_FUNCTION(TypeLoc, v, {
    PRINT_BOOL(v, isNull);
    PRINT_RECURSE_TMP(v, getType);

    PRINT_LOCATION(v, getBeginLoc);
    PRINT_LOCATION(v, getEndLoc);
    PRINT_RANGE(v, getSourceRange);
    PRINT_RANGE(v, getLocalSourceRange);
    PRINT_INT(v, getFullDataSize);
  });

  PRINT_FUNCTION(TypeSourceInfo, v, {
    PRINT_RECURSE_TMP(v, getType);
    PRINT_RECURSE_TMP(v, getTypeLoc);
  });

  PRINT_FUNCTION(SourceRange, v, { PRINT_RANGE_DIRECT(v); });

  PRINT_FUNCTION(UsingShadowDecl, v, { PRINT_RECURSE_PTR(v, getTargetDecl); });

  PRINT_FUNCTION(DeclContext, v, {
    return "";

    switch (v->getDeclKind()) {
      case Decl::Function: {
        auto* function = cast<FunctionDecl>(v);
        s << "[function:" << function << "],";
        PRINT_RECURSE(static_cast<const FunctionDecl*>(function));
        break;
      }

      default:
        s << "[" << v->getDeclKindName() << "],";
        break;
    }

    for (const auto* decl : v->decls()) {
      decl->print(s);
      s << ",";
    }
  });

  PRINT_FUNCTION(VarDecl, v, {
    PRINT_RECURSE(dynamic_cast<const DeclaratorDecl*>(v));
    PRINT_RECURSE_PTR(v, getParentFunctionOrMethod);

    PRINT_BOOL(v, hasLocalStorage);
    PRINT_BOOL(v, hasExternalStorage);
    PRINT_BOOL(v, hasGlobalStorage);
    PRINT_BOOL(v, isLocalVarDecl);
    PRINT_BOOL(v, isLocalVarDeclOrParm);
    PRINT_BOOL(v, isFunctionOrMethodVarDecl);

    // TODO: PRINT_ENUM?
    PRINT_INT(v, isThisDeclarationADefinition);
  });

  PRINT_FUNCTION(UsingDecl, v, {
    PRINT_RECURSE_TMP(v, getNameInfo);
    PRINT_RECURSE_PTR(v, getQualifier);
    // NestedNameSpecifierLoc.
    // PRINT_RECURSE_TMP(v, getQualifierLoc);
    PRINT_RECURSE_PTR(v, getCanonicalDecl);

    if (v->shadow_size() > 0) {
      s << " shadow_size{" << v->shadow_size() << "}";
      int i = 0;
      for (const auto* shadow : v->shadows()) {
        s << "[" << i++ << "]";
        PRINT_RECURSE(shadow);
      }
    }

    PRINT_RECURSE_BASE(NamedDecl, v);
  });

  PRINT_FUNCTION(NestedNameSpecifier, v, {
    PRINT_BOOL(v, isDependent);
    v->print(s, PrintingPolicy(*lo_));
  });

  PRINT_FUNCTION(DeclarationName, v, {
    PRINT_BOOL(v, isEmpty);
    PRINT_BOOL(v, isIdentifier);
    PRINT_BOOL(v, isDependentName);

    PRINT_STRING(v, getAsString);
    // PRINT_RECURSE_TMP(v, getCXXNameType);
  });

  PRINT_FUNCTION(NamedDecl, v, {
    PRINT_RECURSE_BASE(Decl, v);

    PRINT_STRING(v, getNameAsString);
    PRINT_STRING(v, getQualifiedNameAsString);

    PRINT_RECURSE_TMP(v, getDeclName);
    PRINT_BOOL(v, hasLinkage);
    PRINT_BOOL(v, isHidden);
    PRINT_BOOL(v, isCXXClassMember);
    PRINT_BOOL(v, isCXXInstanceMember);
    PRINT_BOOL(v, isExternallyVisible);
    PRINT_BOOL(v, isLinkageValid);

    //       PRINT_RECURSE_PTR(v, getUnderlyingDecl);
  });

  PRINT_FUNCTION(DeclRefExpr, v, {
    PRINT_RECURSE_PTR(v, getDecl);
    PRINT_RECURSE_TMP(v, getNameInfo);
    PRINT_LOCATION(v, getLocation);
    PRINT_LOCATION(v, getBeginLoc);
    PRINT_LOCATION(v, getEndLoc);
    PRINT_BOOL(v, hasQualifier);
    PRINT_RECURSE_PTR(v, getFoundDecl);
    PRINT_BOOL(v, hasTemplateKWAndArgsInfo);
    PRINT_BOOL(v, hasTemplateKeyword);
    PRINT_BOOL(v, hasExplicitTemplateArgs);
    PRINT_BOOL(v, hadMultipleCandidates);
    PRINT_BOOL(v, refersToEnclosingVariableOrCapture);
    PRINT_RECURSE_TMP(v, getType);
    PRINT_BOOL(v, isValueDependent);
    PRINT_BOOL(v, isTypeDependent);
    PRINT_BOOL(v, isInstantiationDependent);
    PRINT_LOCATION(v, getExprLoc);
    PRINT_RECURSE_PTR(v, getReferencedDeclOfCallee);
  });

  PRINT_FUNCTION(Decl, v, {
    PRINT_STRING(v, getDeclKindName);
    PRINT_BOOL(v, hasBody);
    PRINT_BOOL(v, isInAnonymousNamespace);
    PRINT_BOOL(v, isInStdNamespace);
    PRINT_BOOL(v, hasAttrs);
    PRINT_BOOL(v, isUsed);
    PRINT_BOOL(v, isReferenced);

    PRINT_RANGE(v, getSourceRange);
    PRINT_LOCATION(v, getBeginLoc);
    PRINT_LOCATION(v, getEndLoc);
    PRINT_LOCATION(v, getLocation);

    PRINT_RECURSE_PTR(v, getDeclContext);

    s << " print{";
    v->print(s);
    s << "}";
  });

  PRINT_FUNCTION(DeclarationNameInfo, v, {
    PRINT_STRING(v, getAsString);
    PRINT_RANGE(v, getSourceRange);
  });

 private:
  const SourceManager* sm_ = nullptr;
  const LangOptions* lo_ = nullptr;
  FileCache* cache_ = nullptr;
};

#endif /* PRINTER_H */
