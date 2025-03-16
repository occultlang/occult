<div align="center" style="display: grid; place-items: center; gap: 10px;">
  <a href="https://occultlang.org/" target="_blank">
    <img src="occult_circle.svg" width="240" alt="Occult Logo">
  </a>
  <h1 style="margin: 5px;">The Occult Programming Language</h1>
  <p align="center">A modern enigmatic JIT programming language <a href="https://occultlang.org" target="_blank">occultlang.org</a></p> <br>
  <a href="https://discord.gg/ptUACmpg3Z" target="_blank">Official Discord</a> <br><br>
</div>

### Building
```bash
git clone https://github.com/occultlang/occult.git && cd occult
chmod +x build.sh
./build.sh
```

### Building for Visual Studio on Windows
```bash
git clone https://github.com/occultlang/occult.git
cmake -G "Visual Studio 17 2022"
```

> [!IMPORTANT]
> Occult 2.0.0-alpha is going to release on the second birthday (05/31/25) 

### Roadmap for 2.0.0-alpha
- - [x] Lexer
- - [x] Parser
- - [x] Backend (Not fully finished / In progress)
  - - [x] Stack-based IR 
  - - [x] x86 JIT Runtime
  - - [x] IR Translation -> x86 JIT
  - - [ ] Windows Support 
- - [ ] Linter
 
### Notes about the roadmap
- IR/IL + Translation
  - Stack-based IR, works with translation to native JIT x86_64, in the very early stages
- x86 JIT Runtime
  - It's working, it isn't fully finished for the full functionality either, needs polishing, also in the early stages
- Linter
  - It will be done closer to the end of the development roadmap (Not started)

### [Credits](https://github.com/occultlang/occult/blob/main/CREDITS.md)
