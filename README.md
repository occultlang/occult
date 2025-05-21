<div align="center" style="display: grid; place-items: center; gap: 10px;">
  <a href="https://occultlang.org/" target="_blank">
    <img src="occult_circle.svg" width="240" alt="Occult Logo">
  </a>
  <h1 style="margin: 5px;">The Occult Programming Language</h1>
  <p align="center">An enigmatic JIT programming language... <a href="https://occultlang.org" target="_blank">occultlang.org</a></p> <br>
  <a href="https://discord.gg/ptUACmpg3Z" target="_blank">Official Discord</a> <br><br>
</div>

> [!IMPORTANT]
> Occult 2.0.0-alpha is going to be released within the end of May to mid June 2025. <br>
> Keep the wonder alive, and never lose the spark!

### Roadmap for 2.0.0-alpha
- - [x] Lexer
- - [x] Parser
- - [x] Backend (Not fully finished / In progress)
  - - [x] Stack-based IR 
  - - [x] x86 JIT Runtime
  - - [x] IR Translation -> x86 JIT

### Things left to do before release & alpha-testing
- - [x] If statements
- - [x] Loop statements
- - [x] Continue statement
- - [x] Break statement
- - [x] While loops
- - [x] For loops (foreach will be done when arrays are added)
- - [x] Floating point variables (Minimal so far)
- - [x] String variables (immutable)
- - [x] Arrays (add arrays access)
- - [x] Pointers
- - [x] Reworking of x86_64 runtime (Migration in progress)
- - [ ] Translation from Stack IR to an encoded Register Bytecode for easier translation into machine code

### Things planned
- [ ] Static Analysis
- [ ] An intermediate in between stack IR and register bytecode, in TAC form for further optimisation later on
- [ ] Windows support

_____________________________________________________________________________

### [Building](https://github.com/occultlang/occult/blob/main/BUILDING.md)
### [Credits](https://github.com/occultlang/occult/blob/main/CREDITS.md)
