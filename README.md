<div align="center" style="display:grid;place-items:center;">
<p>
    <a href="https://occultlang.org/" target="_blank"><img width="120" src="occult.jpg"></a>
</p>
<h1>The Occult Language</h1>
<p>An enigmatic programming language.</p>
<a href="https://occultlang.org/" target="_blank">occultang.org</a>
</div>

## Building
To build Occult just copy and paste this
```sh
git clone https://github.com/occultlang/occult && cd ./occult && chmod +x ./build.sh && ./build.sh
```

## Using Occult
To use Occult, you can just build it using the steps above and run this (Defaults to JIT)
```sh
./occultc [-debug] <source.occ> [-o <filename>]
```
If you don't need debug or AOT just do this
```sh
./occultc <source.occ>
```

## Testing
All tests are in the `test_cases` directory

## Cleaning
To clean the build directory just run `clean.sh`

## To-do v1.1.0-alpha
- [ ] Guarantee memory safety through different tests
- [ ] Add pointer math
- [ ] Better error handling, polishing the language
- [ ] Better string support to guarantee safety + syntax
- [ ] Better multidimensional array syntax
- [ ] Add match statements, similar to rust
- [ ] Windows support (For now use [WSL](https://learn.microsoft.com/en-us/windows/wsl/install))
- [ ] Pointers in arrays
- [ ] Enhance for loops further
- [ ] Remove the unsafe keyword entirely because of memory safety guarantee
- [ ] Basic static analyzer for Occult

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

## LICENSE
GNU Lesser General Public License v2.1 (Same as TinyCC)

## Credits
https://github.com/orangeduck/tgc/tree/master

https://github.com/TinyCC/tinycc
