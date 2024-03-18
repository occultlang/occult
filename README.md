# Occult
The Occult programming lanugage

# Building
To build occult there's a few steps. you need to run these in order to build it successfully
```bash
$ git clone https://github.com/finalxvd/occult.git
$ ./get_libraries_tcc.sh
$ ./build.sh
```

# Using Occult
To use Occult, you can just build it using the steps above and run this:
```bash
$ ./occultc <path/to/source.occ> 
```
To debug:
```bash
$ ./occultc <path/to/source.occ> -dbg
```

# Testing Occult
Testing of the Occult language
## Speed
Using the `time` command on *nix we can time the time it takes to execute the fibonacci sequence

```
Occult:
fib(45) = 1134903170
real	0m5.021s
user	0m5.012s
sys	0m0.001s

C++:
fib(45) = 1134903170
real	0m1.211s
user	0m1.208s
sys	0m0.001s

Python:
fib(45) = 1134903170

real	1m23.780s
user	1m23.608s
sys	0m0.000s
```

# Cleaning
To clean the build directory just run `clean.sh`

# How?
Right now Occult uses TinyCC as a backend for a JIT compiler, and Occult just compiles its own code into C and it runs on the fly.

Without the advances in generative AI, I would have never been able to write a compiler it has helped me so much with learning new concepts and some coding mistakes!
There's a clear vision for this project in the end to make it bootstrappable so it can compile itself and with safety in mind for the end user. <3 

# LICENSE
GNU Lesser General Public License v2.1 (Same as TinyCC)
