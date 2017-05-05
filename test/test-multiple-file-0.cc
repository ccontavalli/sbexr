// Copyright (c) 2013 Carlo Contavalli (ccontavalli@gmail.com).
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

#include "test-multiple-file-0.h"
#include "include-all.h"

int global_counter;

struct Double {
  int mydouble;

};
struct Single {
  float mysingle;
};

// The anaonymous union will cause any expression
// accessing one of the elements to have 2 MemberExpr:
// 1 to access the field
// 1 to access the anonymous union itself
struct Nfs {
  union {
    struct Single fusingle;
    struct Double fudouble;
  };
};

bool MySingle(struct Single* single) {
  return (long long int)(single) % 2;
}

int Mytest(struct Nfs* nfs) {
  if (MySingle(&nfs->fusingle))
    return 0;
  return 10;
}


int CheckArgs(
  int test, const char* value);
int FuncWithNamedArgs(int foo, char* bar);
int FuncNoNamedArgs(
  int, char*, bool CheckArgs);

int CheckMyArgs(
  const struct Nfs& nfs);

int FuncWithNamedArgs(int foo, const char* bar) {
  MACRO(foo);
  return foo * *bar + 2;
}

int main(int argc, const char** argv) {
  Test test;
  IncludedByAll(8);

  TestMore(&test);
  MACROP("this is a test\n");
  MACRO(12);
  return test.Do00() + test.Do02() + FuncWithNamedArgs(3, "sbexr");
}

