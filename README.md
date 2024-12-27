<div align="center" style="display: grid; place-items: center; gap: 10px;">
  <a href="https://occultlang.org/" target="_blank">
    <img src="occult_circle.svg" width="240" alt="Occult Logo">
  </a>
  <h1 style="margin: 5px;">The Occult Programming Language</h1>
  <p align="center">An modern enigmatic JIT programming language <a href="https://occultlang.org" target="_blank">occultlang.org</a></p> <br><br>
</div>

### Building
```bash
git clone https://github.com/occultlang/occult.git && chmod +x occult/build.sh && ./occult/build.sh
```

### Todo (For 2.0.0-alpha)
- - [x] Lexer
- - [x] Parser
- - [ ] Backend (Codenamed Sigil)
  - - [ ] Bytecode Generation & Interpreter

### Todo (After 2.0.0-alpha)
- - [ ] Arrays
- - [ ] Linter
- - [ ] Backend (Codenamed Sigil)
  - - [ ] Machine code translation from bytecode
  - - [ ] Textual Representation for modularity
- - [ ] Reformat, clean, and organize all code that's necessary

  
### Crediting 
[`parser::to_rpn`](https://github.com/occultlang/occult/blob/main/src/parser/parser.cpp#L51) is based on [this](https://github.com/kamyu104/LintCode/blob/master/C%2B%2B/convert-expression-to-reverse-polish-notation.cpp) <br/><br/>
[fast_float.hpp](https://github.com/fastfloat/fast_float) for number conversions <br/><br/>
I started this project on 05/31/2023, and it would also not be possible without generative artificial intelligence and my friends for helping me write, and learn along the way for this long project!
