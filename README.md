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

> [!IMPORTANT]
> There's a more in-depth guide on getting started at [https://occultlang.org/getting-started](https://occultlang.org/getting-started)

### What is Occult? 
Occult is a systems programming language, meant to give the user the full control and power of a C-like language, but with a modern syntax with modern features, making it easy to write, read and learn. 

### What Occult is capable of?
```
fn write(i64, string, i64) shellcode i64 { 0x55 0x48 0x89 0xe5 0x48 0xc7 0xc0 0x01 0x00 0x00 0x00 0x0f 0x05 0x48 0x31 0xc0 0x48 0x89 0xec 0x5d 0xc3 }

fn strlen(string) shellcode i64 { 0x55 0x48 0x89 0xe5 0x48 0x31 0xc0 0x80 0x3c 0x07 0x00 0x74 0x05 0x48 0xff 0xc0 0xeb 0xf5 0x48 0x89 0xec 0x5d 0xc3 }

fn puts(string s) {
    write(1, s, strlen(s));
}

fn main() {
    puts("1\n");
    puts("2\n");
    puts("3\n");
    puts("4\n");
    puts("5\n");
}
```
This is an example of using raw x86_64 shellcode with the Linux write syscall to print out 1 to 5!
_____________________________________________________________________________

### Building Occult

> [!NOTE]
> **The highest C++ standard that Occult should use is C++23** <br>
> Also, the only architecture Occult supports is `amd64` as of now, obviously this will change!

### Building for Linux / MacOS
```bash
git clone https://github.com/occultlang/occult.git && cd occult
chmod +x build.sh
./build.sh
```
### Building for Visual Studio on Windows (You must install LLVM with clang inside Visual Studio Installer)
```bash
git clone https://github.com/occultlang/occult.git && cd occult
cmake -G "<YOUR VISUAL STUDIO VERSION>"
# i.e "Visual Studio 18 2026" for V.S 2026
# running cmake -G should just give you options to choose from
```
Change the C++ Language Standard to the latest version

![image](https://github.com/user-attachments/assets/74a4819b-b49c-44be-a508-384795c20f20)

Change the Platform Toolset to LLVM(clang-cl) (Or else Occult **WILL NOT** compile on Windows because of MSVC...)

<img width="500" height="99" alt="image" src="https://github.com/user-attachments/assets/0ed5e04b-0432-432f-9772-0a4ea29f8d15" />

Next, go into C/C++ -> Command Line, and then remove all the contents of the "Additional Options" field

![image](https://github.com/user-attachments/assets/69c506aa-b649-45aa-a21f-0388ad7b55b0)

Afterwards, you should be good to go
_____________________________________________________________________________

> [!WARNING]
> **Known Issue**: The `-o` (output native binary) flag is currently broken. Use JIT mode (default) for compilation and execution.

### Roadmap
- New Linker (Planned for v2.2-alpha)
- Generics
- Foreach implementation 
- Static Analysis
- Borrow Checker or RAII (Similar to C++ `std::unique_ptr`)
- New Intermediate Representation (SSA) for optimization pass
- RISCV Support
- ARM64 Support
_____________________________________________________________________________

### [Credits](https://github.com/occultlang/occult/blob/main/CREDITS.md)
