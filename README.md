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

# Building
Assuming you have git and gcc, and other required dependencies installed, all of this should go smoothly
> [!WARNING]  
> Occult uses [tgc](https://github.com/orangeduck/tgc) which causes **undefiend behavior** but most of the time it should be fine, as per tgc's documentation
> 
### Building on Linux (64 bit)
Just run [build.sh](https://github.com/occultlang/occult/blob/main/build.sh) and the process should be fine
### Building on Windows
> [!IMPORTANT]
> > [!NOTICE]
> > Even if you have gcc already installed you must run this script! 
> Prior to building Occult you need gcc, so run [install_gcc.bat](https://github.com/occultlang/occult/blob/main/install_gcc.bat) <br>
> Now with that you can run [build_windows.bat](https://github.com/occultlang/occult/blob/main/build_windows.bat)

# Using Occult
> [!TIP]
> If you need help just use the `-h` option
```sh
./occultc <source.occ>
```

# To-do

### Long-term
- [x] Windows support
- [ ] OSX Support 
- [ ] Bootstrapping
- [ ] Move away from cross-compilation for just-in-time
- [ ] Memory safety as far as we can get it
- [ ] Full static analyzer

### v1.1.0-alpha
- [X] Add basic pointer math
- [x] Array syntax function calls
- [ ] Better string support to guarantee safety + syntax
- [ ] Better multidimensional array syntax 
- [x] Add match statements
- [ ] Pointers in arrays 
- [x] Enhance for loops further 
- [x] Remove the unsafe keyword entirely because of memory safety guarantee
- [x] Static analyzer base for Occult
- [ ] Safe functions
- [ ] Forloop with strings

# Libraries Used
[tgc](https://github.com/orangeduck/tgc/tree/master) <br>
[tinycc(mirror)](https://github.com/TinyCC/tinycc)
