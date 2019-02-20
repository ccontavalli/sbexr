LLVMVERSION := 8
LLVMCONFIG := llvm-config-$(LLVMVERSION)
LIBS := $(shell $(LLVMCONFIG) --libs)
CXXFLAGS := $(shell $(LLVMCONFIG) --cxxflags) -ggdb3 -O0
LDFLAGS := $(shell $(LLVMCONFIG) --ldflags)
TIME := time --format='MEM: unshared process size=%D, average total=%K, max resident=%M\nTIME: total wall time=%e, user time=%U, kernel time=%S'
VERBOSE := --verbose

EXTRALIBS := -lclangFrontend \
	     -lclangParse \
	     -lclangSema \
	     -lclangAnalysis \
	     -lclangAST \
	     -lclangEdit \
	     -lclangLex \
	     -lclangBasic \
	     -lclangDriver \
	     -lclangTooling \
	     -lclangToolingCore \
	     -lclangSerialization \
	     -lclangRewrite \
	     -lLLVMMC \
	     -lLLVMSupport

all: sbexr

sbexr: sbexr.o indexer.o renderer.o rewriter.h wrapping.o rewriter.o cache.o mempool.h mempool.o common.o common.h
	$(CXX) -lclang-$(LLVMVERSION) -lLLVM-$(LLVMVERSION) $(CXXFLAGS) $(LDFLAGS) $(LIBS) $(LIBDIR) -o sbexr $^ $(EXTRALIBS)

validator:
	$(MAKE) -C ../validator

test-run:
	$(MAKE) -C ../server test-run

test-gen: validator sbexr test-multi
	cd ../test && rm -rf output/ && rm -rf indexes/ && ln -sfT ../static ./static \
	    && $(TIME) ../src/sbexr $(VERBOSE) --index=indexes -x "output|indexes" --jsondb . \
	    && ../validator/validator --root ./output |tee ./output/validator.report \
	    && cp -a ./indexes/*.json ./output/


test-dump: test-dump-multi

test-single:
	cd .. && bear $(MAKE) -C ./test clean test-single-file

test-multi:
	cd .. && bear $(MAKE) -C ./test clean test-multiple-file

test-dump-multi:
	cd .. && $(MAKE) -C ./test dump-multi

clean-test:
	$(MAKE) -C ../test clean

dump:
	clang-$(LLVMVERSION) -Xclang -ast-dump -fsyntax-only $(CXXFLAGS) $(LIBS) $(LIBDIR) ./sbexr.cc

.PHONY: server test format

format:
	clang-format-$(LLVMVERSION) --style=google -i *.cc *.h

renderer.o: renderer.cc renderer.h
sbexr.o: sbexr.cc common.h printer.h
indexer.o: indexer.cc indexer.h
wrapping.o: wrapping.cc wrapping.h
rewriter.o: rewriter.cc rewriter.h
cache.o: cache.cc cache.h
mempool.o: mempool.cc mempool.h
common.o: common.cc common.h

clean:
	rm -f sbexr
	rm -f *.o
