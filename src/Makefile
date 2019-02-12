LLVMVERSION := 8
LLVMCONFIG := llvm-config-$(LLVMVERSION)
LIBS := $(shell $(LLVMCONFIG) --libs) -lctemplate
CXXFLAGS := $(shell $(LLVMCONFIG) --cxxflags | sed -e 's/-gsplit-dwarf//g') -ggdb3 -O0
LDFLAGS := $(shell $(LLVMCONFIG) --ldflags | sed -e 's/-gsplit-dwarf //g')
TIME := time --format='MEM: unshared process size=%D, average total=%K, max resident=%M\nTIME: total wall time=%e, user time=%U, kernel time=%S'

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

gen:
	$(MAKE) clean && bear $(MAKE) && cp ./compile_commands.json .. && cd .. && $(TIME) ./src/sbexr --index=indexes -x "output|indexes" .

gen-only:
	cd .. && $(TIME) ./src/sbexr -x "output|indexes" .

test-gen: sbexr test-multi
	cd ../test && rm -rf output/ && mkdir -p output && ln -sfT ../static ./static && cp -a ../compile_commands.json ./ && \
	    rm -rf indexes && mkdir indexes && $(TIME) ../src/sbexr --verbose --index=indexes -x "output|indexes" ./

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

test-server:
	killall -TERM server || true
	cd ../server && make && ./server --root=../test/ --index-dir=../test/indexes --listen=:10000 ../test/

server:
	cd ../server && go build && ./server --index-dir=../indexes --root=../output --listen=:9000 ..
	# cd ../server && go build && ./server --root=../output --listen=:9000 ..
	# cd output && python -m SimpleHTTPServer 8000

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
