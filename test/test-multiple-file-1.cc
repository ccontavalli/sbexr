#include "test-multiple-file-1.h"

// #include <list>
#include <stdio.h>
#include "include-all.h"

namespace test {
struct Point {
  int x = 0, y = 0;
};


struct PointerSquare {
  // Coordinates of the four vertexes.
  const Point *a, **b, ***c, *d;
};

struct MixedSquare {
  // Coordinates of the four vertexes.
  const Point ***a, &b, *c[3], *&d;
};

struct SimpleSquare {
  // Coordinates of the four vertexes.
  const Point a, b, c, d;
};

struct RefSquare {
  const Point &f, &g, &h, &j;
};

struct ArraySquare {
  const Point vx[4];
};

struct ArrayPointerSquare {
  const Point ***vx[4];
};

}

void TestFunction() {
  MACRO(10);
  MACROP("TEST TEST");
  printf("Test Function: %d\n", global_counter);
}

# define POINT volatile test::Point
typedef POINT* punt;

typedef int u65535_t;

using namespace test;


void TestMore(Test* test) {
  Point ab, ac, ad;
  Point p0, p1, p2, *p3, *p4;
  static POINT __attribute__((used))* fuffa;
  static __attribute__((used)) POINT* fuffa2;
  u65535_t value = 1000;

  // POINT* foffa
  // volatile Point* foffa
  punt foffa;
  // POINT** foffa
  // volatile Point** foffa
  punt* foffa2;

  // std::list<bool>* plist2, *plist3;
  test::Point test3;

  test::Point &z = ab, &w = ac, &x = ad;

  MACRO(MACRO_RECURSIVE);
  MACROP("TOST TOST");
  printf("Test pointer %p %d\n", (void*)test, value);
  printf("Test point %d %d %d\n", p0.x, p1.x, p2.x);
}
