<div align="center" style="display:grid;place-items:center;">
<p>
    <a href="https://occultlang.org/" target="_blank"><img width="120" src="occult.jpg"></a>
</p>
<h1>The Occult Language</h1>
<p>
An enigmatic programming language.
</p>
<a href="https://occultlang.org/" target="_blank">occultang.org</a>
</div>

## Building
### Linux

Just copy and paste this command and you're all good to go
```sh
git clone https://github.com/occultlang/occult && cd ./occult && chmod +x ./build.sh && ./build.sh
```
### Windows
It uses CodeBlocks GCC, other window's GCC didn't work with arrays... idk why? I would like to know...<br>
Install git <https://git-scm.com/download/win><br>
***Run `install_gcc.bat` or else Occult won't run properly...*** (You only need to run this once)<br>
Now you have gcc and then you can run `build_windows.bat`

## Using Occult
To use Occult, you can just build it using the steps above and run this (Defaults to JIT) if you need help, just use the `-h` option
```sh
./occultc <source.occ>
```

## Testing
All tests are in the `test_cases` directory

## Cleaning
To clean the build directory just run `clean.sh`

## Long-term
- [x] Windows support
- [ ] OSX Support 
- [ ] Bootstrapping
- [ ] Move away from cross-compilation
- [ ] Full memory safety
- [ ] Full static analyzer

## To-do v1.1.0-alpha
- [X] Add basic pointer math
- [x] Array syntax function calls
- [ ] Better string support to guarantee safety + syntax
- [ ] Better multidimensional array syntax 
- [ ] Add match statements, similar to rust 
- [ ] Pointers in arrays 
- [x] Enhance for loops further 
- [x] Remove the unsafe keyword entirely because of memory safety guarantee
- [x] Static analyzer base for Occult
- [ ] Safe functions
- [ ] Forloop with strings

## Finished v1.0.0-alpha
- [x] Add basic for loops
- [x] Add casting
- [x] Refine CLI arguments
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

## How?
Right now Occult uses TinyCC as a backend for a JIT compiler, and Occult just compiles its own code into C and it runs on the fly.

Without the advances in generative AI, I would have never been able to write a compiler it has helped me so much with learning new concepts and some coding mistakes!
There's a clear vision for this project in the end to make it bootstrappable so it can compile itself.

## Libraries Used
https://github.com/orangeduck/tgc/tree/master

https://github.com/TinyCC/tinycc
