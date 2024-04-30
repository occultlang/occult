# Occult
Inspired by the *enchanting* occultist ideals, just put into a programming language in an *enigmatic* way... 

# Building
To build Occult just copy and paste this:
```
$ git clone https://github.com/occultlang/occult && cd ./occult && chmod +x ./build.sh && ./build.sh
```

# Using Occult
To use Occult, you can just build it using the steps above and run this (Defaults to JIT):
```
$ ./occultc <path/to/source.occ> 
```
AOT Compilation (outputs file as a.out):
```
$ ./occultc <path/to/source.occ> -aot
```
To debug (Gives Tokens, AST, and the transpiled C code):
```
$ ./occultc <path/to/source.occ> -dbg
```

# Testing
All tests are in the `test_cases` directory

# Cleaning
To clean the build directory just run `clean.sh`

# To-do
- [ ] Windows support
- [ ] Add for loops
- [ ] Add matching
- [x] Add casting
- [ ] Better string support to guarantee safety + syntax
- [ ] Better multidimensional array syntax 
- [x] Allow function defs to be at the beginning of everything so they can be called anywhere
- [x] Pointers and manual memory management fully working with garbage collection
- [x] Add imports
- [x] Add new array implementation from [occlib](https://github.com/occultlang/occlib) to Occult 
- [x] Array declarations for arguments 
- [x] 2d, 3d etc. support for arrays 
- [x] Fix Commenting in lexical stage as well as floating points
- [x] Fix floating points not working fully when generating code + parsing
- [x] Add incrementing with i++ and i-- etc. to codegen 
- [x] Get TinyC working with Ahead-of-Time compilation (made mode is still JIT)

# How?
Right now Occult uses TinyCC as a backend for a JIT compiler, and Occult just compiles its own code into C and it runs on the fly.

Without the advances in generative AI, I would have never been able to write a compiler it has helped me so much with learning new concepts and some coding mistakes!
There's a clear vision for this project in the end to make it bootstrappable so it can compile itself.

# LICENSE
GNU Lesser General Public License v2.1 (Same as TinyCC)

# Credits
https://github.com/orangeduck/tgc/tree/master

https://github.com/TinyCC/tinycc
