<div align="center" style="display: grid; place-items: center; gap: 10px;">
  <a href="https://occultlang.org/" target="_blank">
    <img src="mascot.svg" width="240" alt="Occult Logo">
  </a>
  <h1 style="margin: 5px;">The Occult Programming Language</h1>
  <p align="center">An enigmatic systems programming language.</p> <br>
  
[![Website](https://img.shields.io/badge/Website-occultlang.org-blue)](https://occultlang.org/)
[![License](https://img.shields.io/github/license/occultlang/occult)](https://github.com/occultlang/occult/blob/main/LICENSE.md) <br> <br>
[![Stars](https://img.shields.io/github/stars/occultlang/occult?style=social)](https://github.com/occultlang/occult/stargazers)
[![Forks](https://img.shields.io/github/forks/occultlang/occult?style=social)](https://github.com/occultlang/occult/network/members)
</div>

### What is Occult? 
Occult is a systems programming language, meant to give the user the full control and power of a C-like language, but with a modern syntax with modern features, making it easy to write, read and learn. 

### Learning Occult, and the Language Specifications
You can learn more about the language and its specifications here [https://occultlang.org/language-specifications](https://occultlang.org/language-specifications).

### Building Occult

> [!NOTE]
> **The highest C++ standard that Occult should use is C++23** <br>
> Also, the only architecture Occult supports is `amd64` as of now, obviously this will change!

### Linux / MacOS
```bash
git clone --recurse-submodules https://github.com/occultlang/occult.git && cd occult
chmod +x build.sh
./build.sh
```
### Windows (You must install LLVM with clang inside Visual Studio Installer) 
There's a more in-depth guide on getting started & building Occult on windows at [https://occultlang.org/getting-started](https://occultlang.org/getting-started). <br/>
If you already have the prerequisites installed, just run the following in bash.
```bash
git clone --recurse-submodules https://github.com/occultlang/occult.git
cd occult
./build.bat
```

### Roadmap 
- More standard library functions
- Bug Testing & Stability
- Foreach and other advanced modern language features (will be thought out)
- Self hosting compiler
- Multi Architecture, and an optimization pass (RISCV + ARM + WASM) *Saving for the bootstrap*
_____________________________________________________________________________

### [Resources Used](https://github.com/occultlang/occult/blob/main/RESOURCES.md)
