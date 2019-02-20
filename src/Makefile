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

DEPS := sbexr.o indexer.o renderer.o wrapping.o rewriter.o cache.o mempool.o common.o

sbexr: .depend $(DEPS)
	$(CXX) -lclang-$(LLVMVERSION) -lLLVM-$(LLVMVERSION) $(CXXFLAGS) $(LDFLAGS) $(LIBS) $(LIBDIR) -o sbexr $(DEPS) $(EXTRALIBS)

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

SOURCES := $(wildcard *.cc)

.depend:
	fastdep --extraremakedep=Makefile \
	        --remakedeptarget=.depend $(SOURCES) > .depend 2>/dev/null

-include .depend

clean:
	rm -f sbexr
	rm -f *.o
