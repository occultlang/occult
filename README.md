# Occult
The Occult programming lanugage.

# Building
To build occult there's a few steps. you need to run these in order to build it successfully.
```bash
$ git clone https://github.com/finalxvd/occult.git
$ ./get_libraries_tcc.sh
$ ./build.sh
```

# Testing
Occult has test cases and you can just run `run_test_cases.sh` to run the test cases in Occult.

# Cleaning
To clean the build directory just run `clean.sh`

# How?
Right now Occult uses TinyCC as a backend for a JIT compiler, and Occult just compiles its own code into C and it runs on the fly.

Without the advances in generative AI, I would have never been able to write a compiler it has helped me so much with learning new concepts and some coding mistakes :)
\nThere's a clear vision for this project in the end to make it bootstrappable so it can compile itself and with safety in mind for the end user

# LICENSE
GNU Lesser General Public License v2.1 (Same as TinyCC)
