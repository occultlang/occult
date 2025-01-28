<div align="center" style="display: grid; place-items: center; gap: 10px;">
  <a href="https://occultlang.org/" target="_blank">
    <img src="occult_circle.svg" width="240" alt="Occult Logo">
  </a>
  <h1 style="margin: 5px;">The Occult Programming Language</h1>
  <p align="center">A modern enigmatic JIT programming language <a href="https://occultlang.org" target="_blank">occultlang.org</a></p> <br><br>
</div>

### Building
```bash
git clone https://github.com/occultlang/occult.git && chmod +x occult/build.sh && ./occult/build.sh
```
> [!IMPORTANT]
> Occult 2.0.0-alpha is going to release on the second birthday (05/31/25) 

### Roadmap for 2.0.0-alpha
- - [x] Lexer
- - [x] Parser
- - [ ] Backend 
  - - [ ] Custom IL/IR
  - - [x] x86 JIT Runtime
- - [ ] Linter
 
### Notes about the roadmap
- IR/IL
  - Researching options for the project, SSA, TAC, or HLA (Not done)
- x86 JIT Runtime
  - It's working, it just simply needs to have bugs fixed and needs to be polished overall (Working)
- Linter
  - It will be done closer to the end of the development roadmap (Not done)
  
### Crediting 
- [ELF Header](https://wiki.osdev.org/ELF_Tutorial) (Tysm Nisan)<br/>
- [JIT](https://solarianprogrammer.com/2018/01/10/writing-minimal-x86-64-jit-compiler-cpp/) <br/>
- [Shunting Yard](https://github.com/kamyu104/LintCode/blob/master/C%2B%2B/convert-expression-to-reverse-polish-notation.cpp) <br/>
- [fast_float.hpp](https://github.com/fastfloat/fast_float) <br/><br/>
I started this project on 05/31/2023, and it would also not be possible without generative artificial intelligence and my friends for helping me write, and learn along the way for this long project!
