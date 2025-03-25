<div align="center" style="display: grid; place-items: center; gap: 10px;">
  <a href="https://occultlang.org/" target="_blank">
    <img src="occult_circle.svg" width="240" alt="Occult Logo">
  </a>
  <h1 style="margin: 5px;">The Occult Programming Language</h1>
  <p align="center">A modern enigmatic JIT programming language <a href="https://occultlang.org" target="_blank">occultlang.org</a></p> <br>
  <a href="https://discord.gg/ptUACmpg3Z" target="_blank">Official Discord</a> <br><br>
</div>

> [!IMPORTANT]
> Occult 2.0.0-alpha is going to be released on the second birthday (05/31/25) 

### Roadmap for 2.0.0-alpha
- - [x] Lexer
- - [x] Parser
- - [x] Backend (Not fully finished / In progress)
  - - [x] Stack-based IR 
  - - [x] x86 JIT Runtime
  - - [x] IR Translation -> x86 JIT
  - - [ ] Windows Support 
 
### Notes about the roadmap
- IR/IL + Translation
  - Stack-based IR, works with translation to native JIT x86_64, in the very early stages
- x86 JIT Runtime
  - It's working, but it isn't fully finished for the full functionality either, and needs polishing, also in the early stages
- Linter
  - It will be done closer to the end of the development roadmap (Not started)

### Things left to do before testing
- - [x] If statements
- - [x] Loop statements
- - [x] Continue statement
- - [x] Break statement
- - [ ] While loops
- - [ ] For loops
- - [ ] Floating point variables
- - [ ] String variables (immutable)
 
### Things planned for v2.1
- Arrays
- Pointers
- Static Analysis
- An intermediate in between bytecode and AST, in TAC form for further optimisation later on
_____________________________________________________________________________

### [Building](https://github.com/occultlang/occult/blob/main/BUILDING.md)
### [Credits](https://github.com/occultlang/occult/blob/main/CREDITS.md)
