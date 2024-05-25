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

# About
Occult aims to be a memory-safe, statically typed programming language with an elegant syntax similar to Rust and the performance of C. It achieves this by cross-compiling Occult code into C and using [tinycc](https://github.com/TinyCC/tinycc) as both a just-in-time (JIT) and ahead-of-time (AOT) compiler.

In Occult, dynamic arrays are the default array type, functioning similarly to vectors. These arrays are managed by [tgc](https://github.com/orangeduck/tgc/tree/master), a lightweight garbage collector implemented in C. Occult enforces the use of stack-based variables, promoting predictable memory management and reducing the risk of dangling pointers. However, it also allows for heap allocations using tgc's malloc implementation, which the garbage collector automatically frees.
# Building
Assuming you have git gcc, and other required dependencies installed, all of this should go smoothly.
> [!WARNING]  
> Occult uses [tgc](https://github.com/orangeduck/tgc) which causes **undefined behavior** but most of the time it should be fine, as per tgc's documentation.
> 
### Building on Linux (64-bit)
1) Run [build.sh](https://github.com/occultlang/occult/blob/main/build.sh)
### Building on Windows
> [!IMPORTANT]
> Even if you have gcc installed, you must follow this for now!

1) Run [install_gcc.bat](https://github.com/occultlang/occult/blob/main/install_gcc.bat) <br>
2) Run [build_windows.bat](https://github.com/occultlang/occult/blob/main/build_windows.bat)

# Using Occult
> [!TIP]
> If you need help use the `-h` option!
```sh
./occultc <source.occ>
```

# To-do

> [!NOTE]
> What am I currently working on you ask? Fixing bugs!

### Long-term
- [x] Windows support
- [ ] OSX Support 
- [ ] Bootstrapping
- [ ] Move away from cross-compilation for just-in-time
- [ ] Memory safety as far as we can get it
- [ ] Full static analyzer
- [ ] Fixing bugs
- [ ] Add string-supported function calls + other types (array, etc.)
- [ ] Module system base and change "import" and add "include" for including files

### v1.1.0-alpha
- [X] Add basic pointer math
- [x] Array syntax function calls
- [ ] Better multidimensional array syntax 
- [x] Add match statements
- [ ] Pointers in arrays 
- [x] Enhance for loops further 
- [x] Remove the unsafe keyword entirely because of the memory safety guarantee
- [x] Static analyzer base for Occult

### Temporary
- [x] Add a "compilerbreakpoint" keyword (stops codegen / program during compilation)
