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
AOT Compilation (outputs file as occult_out):
```
$ ./occultc <path/to/source.occ> -aot
```
To debug (Gives Tokens, AST, and the transpiled C code):
```
$ ./occultc <path/to/source.occ> -dbg
```

# Speed & Tests
Occult is about the same as C 
Test cases are in the `test_cases` directory

# Cleaning
To clean the build directory just run `clean.sh`

# To-do
- [ ] Array declarations for arguments (Focusing on this more)
- [ ] 2d, 3d etc. support for arrays (Focusing on this more)
- [x] Wrapper around arrays to make it easier to type (Focusing on this more)
- [x] ~~Add for loops~~ (FUTURE VERSION)
- [ ] Add matching
- [ ] Add imports
- [ ] Add casting
- [x] Fix Commenting in lexical stage as well as floating points
- [x] Fix floating points not working fully when generating code + parsing
- [ ] Add tuples
- [ ] Add incrementing with i++ and i-- etc. to codegen 
- [x] Get TinyC working with Ahead-of-Time compilation (made mode is still JIT)

# Issues 
With my current array implementation in [occlib](https://github.com/occultlang/occlib) there is a slight complexity while adding multidimensional arrays, while it is supported, it is more complex than I'd like...
 
Although I can probably get around it by modifying [`code_gen.hpp`](https://github.com/occultlang/occult/blob/main/code_generation/code_gen.hpp) slightly, I have to work and modify how arrays are made now, which would require A LOT of refactoring.
In the compiler itself, I can log the types inside the function arguments (see [`std::vector<std::string> symbols`](https://github.com/occultlang/occult/blob/462f078cdb715e102ba011a9663fcff9f3b0ef94/code_generation/code_gen.hpp#L26)) and check it against the actual func itself, as we wont have variadic. This will help figure out what type we need to use for each array... (this is a huge hassle)

# How?
Right now Occult uses TinyCC as a backend for a JIT compiler, and Occult just compiles its own code into C and it runs on the fly.

Without the advances in generative AI, I would have never been able to write a compiler it has helped me so much with learning new concepts and some coding mistakes!
There's a clear vision for this project in the end to make it bootstrappable so it can compile itself and with safety in mind for the end user. <3 

# LICENSE
GNU Lesser General Public License v2.1 (Same as TinyCC)

# Credits
https://github.com/orangeduck/tgc/tree/master

https://github.com/TinyCC/tinycc
