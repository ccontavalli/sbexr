LIBS := $(shell llvm-config-3.9 --libs) -lctemplate
CXXFLAGS := $(shell llvm-config-3.9 --cxxflags | sed -e 's/-gsplit-dwarf//g') -ggdb3 -O0 -I../
LDFLAGS := $(shell llvm-config-3.9 --ldflags | sed -e 's/-gsplit-dwarf //g')
EXTRALIBS :=

all: test-single-file test-multiple-file

dump-single:
	clang-3.8 -Xclang -ast-dump -fsyntax-only $(CXXFLAGS) $(LIBS) $(LIBDIR) ./test-single-file.c

dump-multi:
	clang-3.8 -Xclang -ast-dump -fsyntax-only $(CXXFLAGS) $(LIBS) $(LIBDIR) ./test-multiple-file-0.cc
	clang-3.8 -Xclang -ast-dump -fsyntax-only $(CXXFLAGS) $(LIBS) $(LIBDIR) ./test-multiple-file-1.cc



test-single-file: test-single-file.o

test-single-file.o: test-single-file.c test-single-file.h

test-multiple-file: test-multiple-file-0.o test-multiple-file-1.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(LIBS) $(LIBDIR) -o test-multiple-file $^ $(EXTRALIBS)

test-multiple-file-0.o: test-multiple-file-0.cc test-multiple-file-0.h

test-multiple-file-1.o: test-multiple-file-1.cc test-multiple-file-1.h

format:
	clang-format-3.8 --style=google -i *.cc *.h

clean:
	rm -f test-multiple-file test-single-file
	rm -f *.o
