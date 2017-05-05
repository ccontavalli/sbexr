#include "test-multiple-file-1.h"

// #include <list>
#include <stdio.h>
#include "include-all.h"

namespace test {
struct Point {
  int x = 0, y = 0;
};
}

void TestFunction() {
  MACRO(10);
  MACROP("TEST TEST");
  printf("Test Function: %d\n", global_counter);
}

# define POINT volatile test::Point
typedef POINT* punt;

using namespace test;


void TestMore(Test* test) {
  Point ab, ac, ad;
  Point p0, p1, p2, *p3, *p4;
  static POINT __attribute__((used))* fuffa;
  static __attribute__((used)) POINT* fuffa2;

  // POINT* foffa
  // volatile Point* foffa
  punt foffa;
  // POINT** foffa
  // volatile Point** foffa
  punt* foffa2;

  // std::list<bool>* plist2, *plist3;
  test::Point test3;

  MACRO(20);
  MACROP("TOST TOST");
  printf("Test pointer %p\n", (void*)test);
  printf("Test point %d %d %d\n", p0.x, p1.x, p2.x);
}
