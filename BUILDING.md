_____________________________________________________________________________

> [!NOTE]
> **The highest C++ standard that Occult should use is C++20**

### Building for Linux / MacOS
```bash
git clone https://github.com/occultlang/occult.git && cd occult
chmod +x build.sh
./build.sh
```
_____________________________________________________________________________
### Building for Visual Studio on Windows (You must install LLVM with clang inside Visual Studio Installer)
```bash
git clone https://github.com/occultlang/occult.git
cmake -G "Visual Studio 17 2022"
```
Change the C++ Language Standard to the latest version

![image](https://github.com/user-attachments/assets/74a4819b-b49c-44be-a508-384795c20f20)

Change the Platform Toolset to LLVM(clang-cl) (Or else Occult **WILL NOT** compile on Windows because of MSVC...)

<img width="500" height="99" alt="image" src="https://github.com/user-attachments/assets/0ed5e04b-0432-432f-9772-0a4ea29f8d15" />

Next, go into C/C++ -> Command Line, and then remove all the contents of the "Additional Options" field

![image](https://github.com/user-attachments/assets/69c506aa-b649-45aa-a21f-0388ad7b55b0)

Afterwards, you should be good to go
_____________________________________________________________________________
