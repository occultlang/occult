<div align="center" style="display: grid; place-items: center; gap: 10px;">
  <a href="https://occultlang.org/" target="_blank">
    <img src="occult.jpg" width="180" alt="Occult Logo">
  </a>
  <h3 style="margin: 5px;">The Occult Programming Language</h3>
  <a href="https://occultlang.org" target="_blank">occultlang.org</a>
</div>

# About
An enigmatic JIT programming language with a clean and modern syntax.

# Building
```bash
git clone https://github.com/occultlang/occult.git && chmod +x occult/build.sh && ./occult/build.sh
```

# Todo
- - [x] Lexer
- - [ ] Parser
  - - [ ] Arrays
  - - [ ] Pointers
- - [ ] Backend (Codenamed Sigil)
  - - [ ] Bytecode Generation & Interpreter
  - - [ ] Machine code translation
  - - [ ] Textual Representation for modularity
- - [ ] Reformat, clean, and organize all code
  - - [x] Lexer
  - - [ ] Parser
  
# Crediting 
[`parser::to_rpn`](https://github.com/occultlang/occult/blob/main/src/parser/parser.cpp#L51) is based on [this](https://github.com/kamyu104/LintCode/blob/master/C%2B%2B/convert-expression-to-reverse-polish-notation.cpp) <br/><br/>
[fast_float.hpp](https://github.com/fastfloat/fast_float) for number conversions <br/><br/>
I started this project on 05/31/2023, and it would also not be possible without generative artificial intelligence and my friends for helping me write, and learn along the way for this long project!
