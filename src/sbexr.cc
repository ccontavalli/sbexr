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
#include "ast.h"
#include "pp-tracker.h"

// TODO:
//   P0 - compound blocks insert html in random places, pretty much breaking
//        <span> or others that might happen in between.
//   P0 - type should link to the definition (eg, __m_buf? should link to the
//        typedef corresponding or struct corresponding)
//   P0 - auto types should show the real underlying type
//   P0 - macros should be expandable to their real value
//   P0 - staticmethods like CompilationDatabase::loadFromDirectory should link
//   to class and method, two links.
//   P0 - template parameters should link back to the original type
//   P0 - show tooltip for type tracking -> eg, typedef foo bar; typedef bar
//   int;
//        foo test = 5; tooltip for foo should show 'foo -> bar -> int'
//   P1 - for templates, track which parameter they are instantiated with?
//   P0 - function argument types should point back to the type definition
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
    "snippet-limit",
    cl::desc("Maximum number of characters captured in a "
             "snippet before or after the relevant text."),
    cl::cat(gl_category), cl::init(60));
cl::opt<std::string> gl_bear_filter_regex(
    "l",
    cl::desc(
        "Regex describing which files to parse from the compilation database."),
    cl::value_desc("regex"), cl::cat(gl_category));

cl::opt<std::string> gl_index_dir(
    "index",
    cl::desc("Directory where to output all generated indexes. Tag "
             "name is used to name files."),
    cl::value_desc("directory"), cl::cat(gl_category), cl::Required);
cl::opt<std::string> gl_jsondb_dir(
    "jsondb",
    cl::desc("Directory where the compile_commands.json file can be found."),
    cl::value_desc("directory"), cl::cat(gl_category), cl::Required);
cl::opt<std::string> gl_scan_dir(
    "scandir",
    cl::desc(
        "Directory to scan for files to include in the output, regardless "
        "of the file being parsed or not. This generally is used to complement "
        "the information gathered through clang - to include Makefiles and "
        "such, "
        "for example."),
    cl::value_desc("directory"), cl::cat(gl_category));
cl::opt<std::string> gl_strip_dir(
    "c",
    cl::desc("Path to strip from generated filenames. This is useful to "
             "prevent disclosing the directory on your system used to build "
             "the source code, for example, and have all paths relative to "
             "where you checked out the code / uncompressed the tarball."),
    cl::value_desc("directory"), cl::init(GetCwd()), cl::cat(gl_category));

struct ToParse {
  ToParse(const std::string& file, const std::string& directory,
          const std::vector<std::string>& argv = std::vector<std::string>())
      : file(file), directory(directory), argv(argv) {}
  ToParse() = default;

  std::string file;
  std::string directory;
  const std::vector<std::string> argv;
};

std::unique_ptr<CompilerInstance> CreateCompilerInstance(
    const std::vector<std::string>& argv) {
  std::vector<const char*> args;
  for (const auto& arg : argv) args.push_back(arg.c_str());

  auto diagnostics = CompilerInstance::createDiagnostics(
      new DiagnosticOptions(), new IgnoringDiagConsumer(), true);
  auto invocation = createInvocationFromCommandLine(args, diagnostics);

  invocation->getDiagnosticOpts().ShowCarets = false;
  invocation->getDiagnosticOpts().ShowFixits = false;
  invocation->getDiagnosticOpts().ShowSourceRanges = false;
  invocation->getDiagnosticOpts().ShowColumn = false;
  invocation->getDiagnosticOpts().ShowLocation = false;
  invocation->getDiagnosticOpts().ShowParseableFixits = false;

  invocation->getFrontendOpts().DisableFree = true;
  invocation->getFrontendOpts().FixWhatYouCan = false;
  invocation->getFrontendOpts().FixOnlyWarnings = false;
  invocation->getFrontendOpts().FixAndRecompile = false;
  // Note: FrontendOptions.Inputs and FrontendOptions.Output have list of input
  // and output files for the invocation.

  invocation->getLangOpts()->Sanitize.clear();
  invocation->getLangOpts()->SpellChecking = false;
  invocation->getLangOpts()->CommentOpts.ParseAllComments = true;

  // CompilerInstance will hold the instance of the Clang compiler for us,
  // managing the various objects needed to run the compiler.
  auto instance = llvm::make_unique<CompilerInstance>();
  instance->setInvocation(std::move(invocation));
  instance->createDiagnostics(new IgnoringDiagConsumer(), true);
  instance->setTarget(TargetInfo::CreateTargetInfo(
      instance->getDiagnostics(), instance->getInvocation().TargetOpts));
  instance->createFileManager();
  instance->createSourceManager(instance->getFileManager());
  instance->createPreprocessor(TU_Complete);
  instance->createASTContext();

  auto& pp = instance->getPreprocessor();
  pp.SetSuppressIncludeNotFoundError(true);
  // BUG? setting one of those two values to true causes the AST to miss some definitions?
  // For example: when compiling test-multiple-file-0.cc, the definition
  // of the class Test in test-multiple-file-0.h is missed, does not appear
  // in a dumpColor of the TU - at all. Search for "TestMore", see that Test*
  // is marked as 'invalid', and interpreted as 'int' rather than object Test.
  //pp.SetCommentRetentionState(true, false);
  pp.getBuiltinInfo().initializeBuiltins(pp.getIdentifierTable(), pp.getLangOpts());

  // Dump header files search path if verbose output is enabled.
  if (gl_verbose) {
    const auto& hs = pp.getHeaderSearchInfo();
    for (auto sd = hs.search_dir_begin(); sd != hs.search_dir_end(); ++sd) {
      llvm::errs() << "+ HEADER SEARCH DIR: " << sd->getName()
                   << (sd->isSystemHeaderDirectory() ? " (system)" : "")
                   << "\n";
    }
  }

  return instance;
}

int main(int argc, const char** argv) {
  HideUnrelatedOptions(&gl_category);

  cl::ParseCommandLineOptions(
      argc, argv, "Indexes and generates HTML files for your source code.");

  LLVMInitializeX86TargetInfo();
  LLVMInitializeX86TargetMC();
  LLVMInitializeX86AsmParser();

  // If those flags were not specified, initialize them to reasonable values.
  if (gl_scan_dir.getNumOccurrences() <= 0) gl_scan_dir.setValue(gl_jsondb_dir);
  if (gl_strip_dir.getNumOccurrences() <= 0)
    gl_strip_dir.setValue(GetRealPath(gl_scan_dir));

  std::cerr << "- INPUT - BUILD DB: " << gl_jsondb_dir << " ("
            << GetRealPath(gl_jsondb_dir) << ")" << std::endl;
  std::cerr << "- INPUT - SCAN DIR: " << gl_scan_dir << " ("
            << GetRealPath(gl_scan_dir) << ")" << std::endl;
  std::cerr << "- PARAM - STRIP PATH: " << gl_strip_dir << " ("
            << GetRealPath(gl_strip_dir) << ")" << std::endl;
  std::cerr << "- OUTPUT - INDEX: " << gl_index_dir << " ("
            << GetRealPath(gl_index_dir) << ")" << std::endl;
  std::cerr << "- OUTPUT - FILES: "
            << "./output"
            << " (" << GetRealPath(".") << "/output"
            << ")" << std::endl;

  std::string error;
  // A Rewriter helps us manage the code rewriting task.
  std::list<ToParse> to_parse;
  std::regex filter(gl_bear_filter_regex.empty() ? std::string(".*")
                                                 : gl_bear_filter_regex);

  {
    auto db = CompilationDatabase::loadFromDirectory(gl_jsondb_dir, error);
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
      for (const auto& command : commands)
        to_parse.emplace_back(file, command.Directory, command.CommandLine);
    }
  }

  // Create an AST consumer instance which is going to get called by
  // ParseAST.
  FileRenderer renderer;
  if (!gl_strip_dir.empty()) renderer.SetStripPath(gl_strip_dir);

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

    std::cerr << to_parse.size() << " PARSING " << filename << " ("
              << parsing.file << " in " << parsing.directory << ") "
              << parsing.argv.size() << std::endl;
    std::cerr << "  ARGV ";
    for (const auto& arg : parsing.argv) std::cerr << arg << " ";
    std::cerr << std::endl;

    auto directory = ChangeDirectoryForScope(parsing.directory);
    if (directory.HasError()) {
      std::cerr << "ERROR: CHANGING DIRECTORY TO " << parsing.directory
                << " FAILED - SKIPPING ARGV" << std::endl;
      continue;
    }
    renderer.SetWorkingPath(parsing.directory);

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
      // FIXME: Sema is using incorrect parameters?
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
  indexer.OutputBinaryIndex(gl_index_dir.c_str(), gl_tag.c_str());
  indexer.Clear();

  MemoryPrinter::OutputStats();

  std::cerr << ">>> EMBEDDING FILES" << std::endl;
  if (!gl_scan_dir.empty()) {
    renderer.ScanTree(gl_scan_dir);
  }
  renderer.OutputJFiles();
  renderer.OutputJOther();
  renderer.OutputJsonTree(gl_index_dir.c_str(), gl_tag.c_str());
  MemoryPrinter::OutputStats();

  const auto& index = MakeMetaPath("index.jhtml");
  if (!MakeDirs(index, 0777)) {
    std::cerr << "ERROR: FAILED TO MAKE META PATH '" << index << "'"
              << std::endl;
  } else {
    if (!gl_scan_dir.empty() || !gl_strip_dir.empty() ||
        !gl_jsondb_dir.empty()) {
      const auto& outputdir =
          gl_scan_dir.empty()
              ? (gl_strip_dir.empty() ? gl_jsondb_dir : gl_strip_dir)
              : gl_scan_dir;
      const auto* dir = renderer.GetDirectoryFor(outputdir);
      const auto& entry = dir->HtmlPath(".jhtml");

      unlink(index.c_str());
      symlink(entry.c_str(), index.c_str());
      std::cerr << ">>> ENTRY POINT " << index << " aka " << entry << std::endl;
    }
  }

  return 0;
}
