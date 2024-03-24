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
- [ ] Array declarations for arguments
- [ ] 2d, 3d etc. support for arrays
- [ ] Wrapper around arrays to make it easier to type 
- [ ] Add for loops
- [ ] Add matching
- [ ] Add imports
- [ ] Add casting
- [x] Fix Commenting in lexical stage as well as floating points
- [x] Fix floating points not working fully when generating code + parsing
- [ ] Add tuples
- [ ] Add incrementing with i++ and i-- etc. to codegen
- [x] Get TinyC working with Ahead-of-Time compilation (made mode is still JIT)

# How?
Right now Occult uses TinyCC as a backend for a JIT compiler, and Occult just compiles its own code into C and it runs on the fly.

Without the advances in generative AI, I would have never been able to write a compiler it has helped me so much with learning new concepts and some coding mistakes!
There's a clear vision for this project in the end to make it bootstrappable so it can compile itself and with safety in mind for the end user. <3 

# LICENSE
GNU Lesser General Public License v2.1 (Same as TinyCC)
