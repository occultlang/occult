
====================================================================================
### Building for Visual Studio on Windows
```bash
git clone https://github.com/occultlang/occult.git
cmake -G "Visual Studio 17 2022"
```
Change the C++ Language Standard to the latest version

![image](https://github.com/user-attachments/assets/74a4819b-b49c-44be-a508-384795c20f20)

Next, go into C/C++ -> Command Line, and then remove all the contents of the "Additional Options" field

![image](https://github.com/user-attachments/assets/69c506aa-b649-45aa-a21f-0388ad7b55b0)

Afterwards, you should be good to go
