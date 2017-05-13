## What is sbexr?

Sbexr is a small C++ binary that uses LLVM and CLANG to build a browsable
version of your C or C++ code and an index to search stuff in it.

Differently from many other similar tools, this means that sbexr can
parse your C and C++ code as well as your compiler, which in turn
means:

   * It has an almost perfect view of what is a variable, class,
     method, object, and so on. It knows if variables are local,
     global, static, ...

   * It does not use regular expressions and heuristics to index
     your code.

   * It has to know the exact flags to build your code, and your
     code has to be compilable for it to be indexable. But that's
     easy, see below.

   * It can perform some tasks typical of static analsyis tools.
     For example, there are at least two functionalities that are work
     in progress: block guards highlighting, and function pointer
     tracking. The former to highlight functions and pieces of code
     that are run within the range of two functions/methods, which
     can be used, for example, to highlight code and functions run
     under a lock / rcu / ... The latter to find out all functions
     that are ever assigned into a function pointer field of a
     struct, useful to track abstractions and "virtual methods" in
     plain old C.

   * It is written with modern and hipster technologies. While C++
     is no longer cool nowdays, the index can be served with a simple
     JSON rest API offered to you by a golang server. Integrating
     it in your own web site (or vim, or $random_tool, for
     autocompletion, for example) should be a joke to any hacker like you.

   * Small footprint to index your code. The whole `linux kernel`
     indexing process requires only a few gigs of RAM. Note that just running
     the compiler to build a single C++ file requires almost as much memory.
     This was probably the hardest part once it was working: first few releases
     required O(20 - 30) gigabytes of RAM to index the whole kernel tree.

   * Small footprints to serve your indexes. This was also pretty fun
     to achieve. A linux kernel will have ~1.2 million symbols roughly, 
     a naive implementation was requiring several gigabytes of ram for
     each kernel version. The current implementation uses mmap and compact
     binary structs, so I was able to serve 10 - 20 versions of the linux
     kernel worth of index on a machine with only a few gigs of ram, with
     good performance.


Note that **sbexr** is beta at best. It has some important missing
functionalities.

However, it is "good enough" to index the kernel source code, which
you can find at [http://sbexr.rabexc.org](http://sbexr.rabexc.org),
thought with a few glitches (notably: macro support and full text
indexing are not implemented yet).

sbexr is released under a 3-clause BSD license, so you can do pretty
much anything with it. Contributions are very welcome.


## Building sbexr

The instructions below work on most debian derived systems.

1. Install all the necessary dependencies:

       # To build and compile the tool.
       apt-get install build-essential
       apt-get install libclang-3.9-dev
       apt-get install llvm-3.9-dev

       # g++ >= 5 is fine. 6 is a good default.
       apt-get install g++-6

       apt-get install libsparsehash-dev
       apt-get install libctemplate-dev
       apt-get install rapidjson-dev

       # To build the search index and web server.
       apt-get install golang


## Using sbexr

Make sure you have all the needed tools installed:

    apt-get install bear

Now:

1. Build what you have to build with whichever mechanism
   you like. Just remember to wrap your "final call to make"
   with "bear". Let me explain.

   Generally, to build a piece of C or C++ you run some
   sort of `configure` (`make dep`, `cmake ...`, ...) to generate
   a `Makefile` or some sort of script that will then build
   your code. Whatever command you then end up with to
   build your code (for example: `make`), write `bear -a -o path` before
   it.

       bear -a -o /tmp make -j3


2. bear will generate a file named `compile_commands.json`, a `compilation database`.
   This is used by sbexr as input to read all the `cc` or `c++` commands
   to compile your code.

3. Next, run `sbexr` to compile your code, and output the browsable html somewhere.
   Something like:

       sbexr -c /opt/path/to/strip/from/your/source/files/ \
             --index /opt/path/to/where/you/want/the/index/output \
             -p "Name of the project" \
             /opt/path/to/your/compile_commands.json/directory/

    Of course, you can use `sbexr --help` to have a nice and hard to read help.

4. If your generated index is big (like the one of a kernel), you need to use
   the "sbexr server" (in the server subdirectory) to serve it. It provides
   a simple json REST API.

## A Note on the quality of the tool

I wrote this during a few weekends, while kids and wife were on vacation.
I raced to get something working before they got back.

You'll see that the code is in dire need of many cleanups, there are hard
coded paths, and many TODOs. It's pretty much in a "works well for me status",
"please help me out here to make it better".

There are a few things in my TODO list that are relatively urgent:

   * Improve SEO. Right now, indexing of `sbexr` pages by search engine
     sucks for various reasons. sbexr needs to generate better `metadata` tags,
     as a starting point.

   * Finish moving the templating logic into the golang `server` binary, and
     have it have an option to compile everything statically, rather than force
     you to run the server as an API. For small projects, and earlier versions
     of sbexr, it generated simple ".json" files that required no API server to run
     on your hosting provider - and still provide full search functionality.

   * Finish implementing full text search in source code. Almost there.

   * Write support for MACRO tracking and expansion.

   * Turn proof of concept lock tracking and function pointer analysis in
     production.

   * Tons and tons and tons of cleanups. Some parts of sbexr are fairly horrible.
