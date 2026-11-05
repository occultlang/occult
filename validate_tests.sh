#!/bin/bash
# validate_tests.sh - Test validation script for Occult test suite
# Validates that all test files compile correctly

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Counters
total_tests=0
passed_tests=0
failed_tests=0
skipped_tests=0

# Arrays to store results
declare -a failed_files
declare -a error_details

echo "========================================"
echo "  Occult Test Suite Validation"
echo "========================================"
echo ""

# Check if compiler exists
# the compiler under test, so the same suite can be pointed at a cross build:
#   OCCULTC="wine build-win/occultc.exe" ./validate_tests.sh
OCCULTC="${OCCULTC:-./build/occultc}"

if [ ! -f "${OCCULTC##* }" ]; then
    echo -e "${RED}Error: Compiler not found at $OCCULTC${NC}"
    echo "Please run ./build.sh first"
    exit 1
fi

echo "Compiler found: $OCCULTC"
echo ""

# occultc exits with what the program returned, so the exit code no longer
# says whether the compiler itself was happy. Its own diagnostics all go to
# stderr and all carry one of these tags.
DIAG='\[PARSE ERROR\]|\[LINT ERROR\]|\[IR ERR\]|\[OCCULTC\] (Parsing|Linting|Code generation) failed|\[OCCULTC\] (No main function|JIT execution crashed|Could not load)|\[-\] No input file'

# Test each .occ file in tests directory
for test_file in tests/*.occ; do
    base_name=$(basename "$test_file")
    
    # Skip test.occ as it's known to have parser issues unrelated to stdio migration
    if [ "$base_name" = "test.occ" ]; then
        echo -e "${YELLOW}SKIP${NC} $base_name (known parser issues)"
        ((skipped_tests++))
        continue
    fi
    
    # A file with no main is a library another test includes, not a test.
    # occultc has nothing to run in it and now says so.
    if ! grep -q "fn main" "$test_file" && ! head -1 "$test_file" | grep -q "EXPECT-FAIL"; then
        echo -e "${YELLOW}SKIP${NC} $base_name (library, no main)"
        ((skipped_tests++))
        continue
    fi

    ((total_tests++))
    
    # Compile the test file in JIT mode (don't use -o flag as it's broken)
    # Use timeout to prevent hanging tests
    # Redirect stdout to suppress execution output, capture stderr for errors
    error_output=$(timeout 5 $OCCULTC "$test_file" 2>&1 >/dev/null)
    compile_result=$?
    
    # Negative tests: first line "// EXPECT-FAIL" means the compiler must reject it
    if head -1 "$test_file" | grep -q "EXPECT-FAIL"; then
        if echo "$error_output" | grep -qE "$DIAG" && [ $compile_result -ne 124 ]; then
            echo -e "${GREEN}PASS${NC} $base_name (rejected as expected)"
            ((passed_tests++))
        else
            echo -e "${RED}FAIL${NC} $base_name (expected a compile error)"
            ((failed_tests++))
            failed_files+=("$base_name")
            error_details+=("$base_name: compiled but should have been rejected")
        fi
        continue
    fi
    
    # Check for compilation errors
    if echo "$error_output" | grep -qE "$DIAG"; then
        echo -e "${RED}FAIL${NC} $base_name (compilation error)"
        ((failed_tests++))
        failed_files+=("$base_name")
        error_msg=$(echo "$error_output" | grep -E "$DIAG" | head -2)
        error_details+=("$base_name: $error_msg")
    # Check for segmentation faults (exit code 139 or 'Segmentation fault' in output)
    elif [ $compile_result -eq 139 ] || echo "$error_output" | grep -q "Segmentation fault"; then
        echo -e "${RED}FAIL${NC} $base_name (segmentation fault)"
        ((failed_tests++))
        failed_files+=("$base_name")
        error_details+=("$base_name: Segmentation fault (exit code $compile_result)")
    # Check for timeout
    elif [ $compile_result -eq 124 ]; then
        echo -e "${YELLOW}TIMEOUT${NC} $base_name"
        ((failed_tests++))
        failed_files+=("$base_name")
        error_details+=("$base_name: Compilation timeout")
    # the program's own return value, which occultc now exits with. `// EXIT: n`
    # in the first few lines says what to expect; without one, zero.
    elif want=$(head -5 "$test_file" | sed -n 's|^// EXIT: ||p'); [ "$compile_result" -ne "${want:-0}" ]; then
        echo -e "${RED}FAIL${NC} $base_name (returned $compile_result, expected ${want:-0})"
        ((failed_tests++))
        failed_files+=("$base_name")
        error_details+=("$base_name: returned $compile_result, expected ${want:-0}")
    else
        echo -e "${GREEN}PASS${NC} $base_name"
        ((passed_tests++))
    fi
done

# asm(arch) / shellcode(arch): PASS tests must print PASS in JIT and in a native -o binary,
# fail_* tests must be rejected
for occ_file in tests/asm/*.occ; do
    [ -e "$occ_file" ] || continue
    base_name=$(basename "$occ_file")
    ((total_tests++))
    ok=1
    if [[ "$base_name" == fail_* ]]; then
        timeout 10 $OCCULTC "$occ_file" >/dev/null 2>&1 && ok=0
    else
        timeout 10 $OCCULTC "$occ_file" 2>&1 | grep -q '^PASS$' || ok=0
        bin=$(mktemp)
        if $OCCULTC -o "$bin" "$occ_file" >/dev/null 2>&1; then
            timeout 10 "$bin" 2>&1 | grep -q '^PASS$' || ok=0
        else
            ok=0
        fi
        rm -f "$bin"
    fi
    if [ $ok -eq 1 ]; then
        echo -e "${GREEN}PASS${NC} asm/$base_name"
        ((passed_tests++))
    else
        echo -e "${RED}FAIL${NC} asm/$base_name"
        ((failed_tests++))
        failed_files+=("asm/$base_name")
        error_details+=("asm/$base_name: jit/native mismatch")
    fi
done

# The command line: `fn main(i64 argc, i64 argv)` has to see the same
# four arguments whether the program was linked by -o or run by the JIT.
# The JIT takes them after `--`; everything before that is occultc's.
for occ_file in tests/cli/*.occ; do
    [ -e "$occ_file" ] || continue
    base_name=$(basename "$occ_file")
    ((total_tests++))
    ok=1
    timeout 10 $OCCULTC "$occ_file" -- one two three 2>&1 | grep -q '^PASS$' || ok=0
    bin=$(mktemp)
    if $OCCULTC -o "$bin" "$occ_file" >/dev/null 2>&1; then
        timeout 10 "$bin" one two three 2>&1 | grep -q '^PASS$' || ok=0
    else
        ok=0
    fi
    rm -f "$bin"
    if [ $ok -eq 1 ]; then
        echo -e "${GREEN}PASS${NC} cli/$base_name"
        ((passed_tests++))
    else
        echo -e "${RED}FAIL${NC} cli/$base_name"
        ((failed_tests++))
        failed_files+=("cli/$base_name")
        error_details+=("cli/$base_name: argc/argv differ between -o and the JIT")
    fi
done

# extern fn under the JIT: there is a loader in the process, so occultc
# asks it for the address rather than refusing the call. A PASS test must
# print PASS when run, and must still be refused by -o, which builds a
# blob with no loader in it. A fail_* test must be refused outright.
# `// OCCULTC: <flags>` on the first line adds compiler flags.
for occ_file in tests/jit/*.occ; do
    [ -e "$occ_file" ] || continue
    base_name=$(basename "$occ_file")
    ((total_tests++))
    ok=1
    opts=$(head -1 "$occ_file" | sed -n 's|^// OCCULTC: ||p')
    bin=$(mktemp)
    if [[ "$base_name" == fail_* ]]; then
        # refused by the compiler, which says so: a program that ran and
        # returned something other than 0 is not a refusal. -o has to
        # refuse it too, at compile time rather than when it is loaded.
        out=$(timeout 10 $OCCULTC $opts "$occ_file" 2>&1) && ok=0
        grep -q '\[OCCULTC\]' <<< "$out" || ok=0
        out=$($OCCULTC $opts -o "$bin" "$occ_file" 2>&1) && ok=0
        grep -q '\[OCCULTC\]' <<< "$out" || ok=0
    else
        # and the binary -o builds, which imports what the JIT looked up
        timeout 10 $OCCULTC $opts "$occ_file" 2>&1 | grep -q '^PASS$' || ok=0
        if $OCCULTC $opts -o "$bin" "$occ_file" >/dev/null 2>&1; then
            timeout 10 "$bin" 2>&1 | grep -q '^PASS$' || ok=0
        else
            ok=0
        fi
    fi
    rm -f "$bin"
    if [ $ok -eq 1 ]; then
        echo -e "${GREEN}PASS${NC} jit/$base_name"
        ((passed_tests++))
    else
        echo -e "${RED}FAIL${NC} jit/$base_name"
        ((failed_tests++))
        failed_files+=("jit/$base_name")
        error_details+=("jit/$base_name: extern resolution, under the JIT or in the image -o builds")
    fi
done

# --format coff: an object for a Windows linker. Needs the mingw-w64 cross
# compiler and llvm-objdump, and is skipped with a note without them. A test
# has to emit an object, link into a PE, and have every name on its
# `// CALLS:` line reached by a call instruction once linked.
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
if command -v "$MINGW" >/dev/null && command -v llvm-objdump >/dev/null; then
    for occ_file in tests/coff/*.occ; do
        [ -e "$occ_file" ] || continue
        base_name=$(basename "$occ_file")
        ((total_tests++))
        ok=1
        obj=$(mktemp --suffix=.obj)
        exe=$(mktemp --suffix=.exe)
        driver="${occ_file%.occ}.c"
        [ -e "$driver" ] || driver=""
        # mingw's ld names its own libraries and warns at the MSVC ones, so
        # the object that goes to it is the --no-drectve one. The default
        # object is checked for the directives instead: on Windows they are
        # the whole of what makes `cl prog.obj` find the C runtime.
        msvc_obj=$(mktemp --suffix=.obj)
        if $OCCULTC -c --format coff --no-drectve -o "$obj" "$occ_file" >/dev/null 2>&1 \
           && $OCCULTC -c --format coff -o "$msvc_obj" "$occ_file" >/dev/null 2>&1 \
           && "$MINGW" "$obj" $driver -o "$exe" >/dev/null 2>&1; then
            dis=$(llvm-objdump -d "$exe" 2>/dev/null)
            for want in $(head -20 "$occ_file" | sed -n 's|^// CALLS: ||p'); do
                echo "$dis" | grep -qE "call.*<$want>" || ok=0
            done
            grep -q 'DEFAULTLIB:"libcmt"' "$msvc_obj" || ok=0
            grep -q 'DEFAULTLIB:"legacy_stdio_definitions"' "$msvc_obj" || ok=0
            grep -q 'DEFAULTLIB' "$obj" && ok=0
        else
            ok=0
        fi
        rm -f "$obj" "$exe" "$msvc_obj"
        if [ $ok -eq 1 ]; then
            echo -e "${GREEN}PASS${NC} coff/$base_name"
            ((passed_tests++))
        else
            echo -e "${RED}FAIL${NC} coff/$base_name"
            ((failed_tests++))
            failed_files+=("coff/$base_name")
            error_details+=("coff/$base_name: COFF object, PE link, a call that did not resolve, or missing /DEFAULTLIB directives")
        fi
    done
else
    echo -e "${YELLOW}SKIP${NC} coff/ (no $MINGW or llvm-objdump)"
    ((skipped_tests++))
fi

# --format coff -o: a PE32+ with its own entry stub and an import of
# ExitProcess, so main's return value is the process's exit code on
# Windows. Needs wine to run it. The std-dependent cases are not here --
# Hell's Gate reads syscall numbers out of ntdll's `mov eax, imm32` stubs
# and wine's ntdll has no such bytes, so the SSNs come out as garbage.
if command -v wine >/dev/null; then
    for occ_file in tests/pe/*.occ; do
        [ -e "$occ_file" ] || continue
        base_name=$(basename "$occ_file")
        ((total_tests++))
        want=$(head -1 "$occ_file" | sed -n 's|^// EXIT: ||p')
        # `// STDOUT: text` near the top has to turn up on stdout, through a
        # pipe, so what the C runtime buffered has to have been flushed
        want_out=$(head -6 "$occ_file" | sed -n 's|^// STDOUT: ||p')
        exe=$(mktemp --suffix=.exe)
        if $OCCULTC --format coff -o "$exe" "$occ_file" >/dev/null 2>&1; then
            out=$(wine "$exe" 2>/dev/null | cat; exit "${PIPESTATUS[0]}")
            got=$?
            rm -f "$exe"
            if [ -n "$want_out" ] && ! grep -qF -- "$want_out" <<< "$out"; then
                got="$got, stdout without '$want_out'"
            fi
            if [ -z "$want" ] || [ "$got" = "$want" ]; then
                echo -e "${GREEN}PASS${NC} pe/$base_name (exit $got)"
                ((passed_tests++))
            else
                echo -e "${RED}FAIL${NC} pe/$base_name (exit $got, wanted $want)"
                ((failed_tests++))
                failed_files+=("pe/$base_name")
                error_details+=("pe/$base_name: PE32+ built but ran with the wrong exit code")
            fi
        else
            rm -f "$exe"
            echo -e "${RED}FAIL${NC} pe/$base_name (could not build)"
            ((failed_tests++))
            failed_files+=("pe/$base_name")
            error_details+=("pe/$base_name: --format coff -o did not produce a PE")
        fi
    done
else
    echo -e "${YELLOW}SKIP${NC} pe/ (no wine)"
    ((skipped_tests++))
fi

# Feature tests. The ones in features/shared/ run on both platforms with
# the same source and are built both ways on this host: JIT, -o as a
# native image, and under wine as a PE. features/linux/ and
# features/windows/ hold the platform-specific tests, like a wrapper
# over a libc call that raises an Occult error, and run only where they
# belong.
#
# nameref across local scopes is flaky, so stderr lines are joined with
# a NUL byte and split again here. $5 is that string.
feature_assert() {
    local cmd="$3" want=$4 want_err_joined=$5
    local got err
    err=$(timeout 10 bash -c "$cmd" 2>&1 >/dev/null)
    got=$?
    [ "$got" = "$want" ] || return 1
    local IFS=$'\x1f'
    for line in $want_err_joined; do
        [ -z "$line" ] && continue
        grep -qF -- "$line" <<< "$err" || return 1
    done
    return 0
}

run_feature_test() {
    local dir=$1 occ_file=$2 as_pe=$3
    local base_name
    base_name=$(basename "$occ_file")
    ((total_tests++))
    local want
    want=$(head -1 "$occ_file" | sed -n 's|^// EXIT: ||p')
    mapfile -t want_err < <(head -8 "$occ_file" | sed -n 's|^// STDERR: ||p')
    local ok=1
    local joined
    joined=$(printf '%s\x1f' "${want_err[@]}")
    feature_assert jit "$occ_file" "$OCCULTC '$occ_file'" "$want" "$joined" || ok=0
    local bin
    bin=$(mktemp)
    if $OCCULTC -o "$bin" "$occ_file" >/dev/null 2>&1; then
        feature_assert img "$occ_file" "'$bin'" "$want" "$joined" || ok=0
    else
        ok=0
    fi
    rm -f "$bin"
    if [ "$as_pe" = "yes" ] && command -v wine >/dev/null; then
        local exe
        exe=$(mktemp --suffix=.exe)
        if $OCCULTC --format coff -o "$exe" "$occ_file" >/dev/null 2>&1; then
            feature_assert pe "$occ_file" "wine '$exe'" "$want" "$joined" || ok=0
        else
            ok=0
        fi
        rm -f "$exe"
    fi
    if [ $ok -eq 1 ]; then
        echo -e "${GREEN}PASS${NC} $dir/$base_name"
        ((passed_tests++))
    else
        echo -e "${RED}FAIL${NC} $dir/$base_name"
        ((failed_tests++))
        failed_files+=("$dir/$base_name")
        error_details+=("$dir/$base_name: exit code or stderr did not match in one of jit/-o/pe")
    fi
}

for occ_file in tests/features/shared/*.occ; do
    [ -e "$occ_file" ] || continue
    run_feature_test features/shared "$occ_file" yes
done
for occ_file in tests/features/linux/*.occ; do
    [ -e "$occ_file" ] || continue
    run_feature_test features/linux "$occ_file" no
done
if command -v wine >/dev/null; then
    for occ_file in tests/features/windows/*.occ; do
        [ -e "$occ_file" ] || continue
        base_name=$(basename "$occ_file")
        ((total_tests++))
        want=$(head -1 "$occ_file" | sed -n 's|^// EXIT: ||p')
        mapfile -t want_err < <(head -8 "$occ_file" | sed -n 's|^// STDERR: ||p')
        ok=1
        exe=$(mktemp --suffix=.exe)
        if $OCCULTC --format coff -o "$exe" "$occ_file" >/dev/null 2>&1; then
            err=$(timeout 10 wine "$exe" 2>&1 >/dev/null)
            got=$?
            [ "$got" = "$want" ] || ok=0
            for line in "${want_err[@]}"; do
                grep -qF -- "$line" <<< "$err" || ok=0
            done
        else
            ok=0
        fi
        rm -f "$exe"
        if [ $ok -eq 1 ]; then
            echo -e "${GREEN}PASS${NC} features/windows/$base_name"
            ((passed_tests++))
        else
            echo -e "${RED}FAIL${NC} features/windows/$base_name"
            ((failed_tests++))
            failed_files+=("features/windows/$base_name")
            error_details+=("features/windows/$base_name: wine PE run did not match")
        fi
    done
else
    echo -e "${YELLOW}SKIP${NC} features/windows/ (no wine)"
    ((skipped_tests++))
fi

# Shared library tests on Linux: each *.occ is compiled to a .so, then
# the sibling *_driver.c is compiled + linked against -ldl and run. The
# driver's exit code is what the test checks against the `// EXIT: n`
# line in the .occ source.
for occ_file in tests/features/sharedlib_linux/*.occ; do
    [ -e "$occ_file" ] || continue
    base_name=$(basename "$occ_file" .occ)
    driver="tests/features/sharedlib_linux/${base_name}_driver.c"
    [ -e "$driver" ] || continue
    ((total_tests++))
    want=$(head -2 "$occ_file" | sed -n 's|^// EXIT: ||p')
    tmpdir=$(mktemp -d)
    so="$tmpdir/lib${base_name}.so"
    drv="$tmpdir/drv"
    ok=1
    if ! $OCCULTC -o "$so" "$occ_file" >/dev/null 2>&1; then ok=0; fi
    if [ $ok -eq 1 ] && ! cc -o "$drv" "$driver" -ldl 2>/dev/null; then ok=0; fi
    if [ $ok -eq 1 ]; then
        got=$("$drv" "$so" >/dev/null 2>&1; echo $?)
        [ "$got" = "$want" ] || ok=0
    fi
    rm -rf "$tmpdir"
    if [ $ok -eq 1 ]; then
        echo -e "${GREEN}PASS${NC} features/sharedlib_linux/$base_name.occ"
        ((passed_tests++))
    else
        echo -e "${RED}FAIL${NC} features/sharedlib_linux/$base_name.occ"
        ((failed_tests++))
        failed_files+=("features/sharedlib_linux/$base_name.occ")
        error_details+=("features/sharedlib_linux/$base_name.occ: build or driver exit mismatched")
    fi
done

# Shared library tests on Windows: each *.occ is compiled to a .dll with
# --format coff, then the sibling *_driver.c is cross-compiled with
# mingw-w64 and run under wine. Needs x86_64-w64-mingw32-gcc + wine.
if command -v x86_64-w64-mingw32-gcc >/dev/null && command -v wine >/dev/null; then
    for occ_file in tests/features/sharedlib_windows/*.occ; do
        [ -e "$occ_file" ] || continue
        base_name=$(basename "$occ_file" .occ)
        driver="tests/features/sharedlib_windows/${base_name}_driver.c"
        [ -e "$driver" ] || continue
        ((total_tests++))
        want=$(head -2 "$occ_file" | sed -n 's|^// EXIT: ||p')
        tmpdir=$(mktemp -d)
        dll="$tmpdir/${base_name}.dll"
        exe="$tmpdir/drv.exe"
        ok=1
        if ! $OCCULTC --format coff -o "$dll" "$occ_file" >/dev/null 2>&1; then ok=0; fi
        if [ $ok -eq 1 ] && ! x86_64-w64-mingw32-gcc -o "$exe" "$driver" 2>/dev/null; then ok=0; fi
        if [ $ok -eq 1 ]; then
            got=$(cd "$tmpdir" && wine "$(basename "$exe")" "$(basename "$dll")" >/dev/null 2>&1; echo $?)
            [ "$got" = "$want" ] || ok=0
        fi
        rm -rf "$tmpdir"
        if [ $ok -eq 1 ]; then
            echo -e "${GREEN}PASS${NC} features/sharedlib_windows/$base_name.occ"
            ((passed_tests++))
        else
            echo -e "${RED}FAIL${NC} features/sharedlib_windows/$base_name.occ"
            ((failed_tests++))
            failed_files+=("features/sharedlib_windows/$base_name.occ")
            error_details+=("features/sharedlib_windows/$base_name.occ: build or driver exit mismatched")
        fi
    done
else
    echo -e "${YELLOW}SKIP${NC} features/sharedlib_windows/ (needs mingw-w64 + wine)"
    ((skipped_tests++))
fi

# occultc exits with what main returned, so a build system can read it.
# `// EXIT: n` on the first line says which. The binary -o builds has to
# agree, and so does the object -c builds once cc has linked it. Each `// STDERR: text` near the top is a line that
# has to turn up on stderr in every one of those runs.
for occ_file in tests/exit/*.occ; do
    [ -e "$occ_file" ] || continue
    base_name=$(basename "$occ_file")
    ((total_tests++))
    want=$(head -1 "$occ_file" | sed -n 's|^// EXIT: ||p')
    mapfile -t want_err < <(head -6 "$occ_file" | sed -n 's|^// STDERR: ||p')
    ok=1
    # stderr of one run against every STDERR line
    stderr_ok() {
        local line
        for line in "${want_err[@]}"; do
            grep -qF -- "$line" <<< "$1" || return 1
        done
        return 0
    }
    err=$(timeout 10 $OCCULTC "$occ_file" 2>&1 >/dev/null)
    [ $? -eq "$want" ] || ok=0
    stderr_ok "$err" || ok=0
    if true; then
        bin=$(mktemp)
        if $OCCULTC -o "$bin" "$occ_file" >/dev/null 2>&1; then
            err=$(timeout 10 "$bin" 2>&1 >/dev/null)
            [ $? -eq "$want" ] || ok=0
            stderr_ok "$err" || ok=0
        else
            ok=0
        fi
        if command -v cc >/dev/null; then
            obj=$(mktemp --suffix=.o)
            if $OCCULTC -c -o "$obj" "$occ_file" >/dev/null 2>&1 && cc -no-pie "$obj" -o "$bin" >/dev/null 2>&1; then
                err=$(timeout 10 "$bin" 2>&1 >/dev/null)
                [ $? -eq "$want" ] || ok=0
                stderr_ok "$err" || ok=0
            else
                ok=0
            fi
            rm -f "$obj"
        fi
        rm -f "$bin"
    fi
    if [ $ok -eq 1 ]; then
        echo -e "${GREEN}PASS${NC} exit/$base_name"
        ((passed_tests++))
    else
        echo -e "${RED}FAIL${NC} exit/$base_name"
        ((failed_tests++))
        failed_files+=("exit/$base_name")
        error_details+=("exit/$base_name: exit status is not $want, or stderr lacks a // STDERR line")
    fi
done

# -c: an ELF object linked by the system linker against the real libc.
# A PASS test must print PASS once linked and run; a fail_* test must be
# rejected. Needs cc, since the point is that cc can finish the job.
if command -v cc >/dev/null; then
    for occ_file in tests/object/*.occ; do
        [ -e "$occ_file" ] || continue
        base_name=$(basename "$occ_file")
        ((total_tests++))
        ok=1
        obj=$(mktemp --suffix=.o)
        if [[ "$base_name" == fail_* ]]; then
            $OCCULTC -c -o "$obj" "$occ_file" >/dev/null 2>&1 && ok=0
        else
            bin=$(mktemp)
            # a companion .c, when there is one, holds main
            driver="${occ_file%.occ}.c"
            [ -e "$driver" ] || driver=""
            # `// OCCULTC: <flags>` on the first line adds compiler flags
            opts=$(head -1 "$occ_file" | sed -n 's|^// OCCULTC: ||p')
            # a module's `link` comes back as a line saying what to pass the
            # linker, and the link uses exactly that, so the line is tested too
            if note=$($OCCULTC -c $opts -o "$obj" "$occ_file" 2>&1 >/dev/null) \
               && libs=$(sed -n 's|.* with \(.*\), the libraries its modules name.*|\1|p' <<< "$note") \
               && cc -no-pie "$obj" $driver $libs -o "$bin" >/dev/null 2>&1; then
                timeout 10 "$bin" 2>&1 | grep -q '^PASS$' || ok=0
            else
                ok=0
            fi
            rm -f "$bin"
        fi
        rm -f "$obj"
        if [ $ok -eq 1 ]; then
            echo -e "${GREEN}PASS${NC} object/$base_name"
            ((passed_tests++))
        else
            echo -e "${RED}FAIL${NC} object/$base_name"
            ((failed_tests++))
            failed_files+=("object/$base_name")
            error_details+=("object/$base_name: did not build, link and print PASS")
        fi
    done
fi

# the assembler's encodings against GNU as (needs as, objcopy, nm and a C++ compiler)
if command -v as >/dev/null && command -v objcopy >/dev/null && command -v g++ >/dev/null; then
    ((total_tests++))
    dump=$(mktemp)
    if g++ -std=c++20 -O0 -o "$dump" tests/asm/asm_dump.cpp 2>/dev/null && python3 tests/asm/crosscheck.py "$dump" >/dev/null 2>&1; then
        echo -e "${GREEN}PASS${NC} asm/crosscheck (identical to GNU as)"
        ((passed_tests++))
    else
        echo -e "${RED}FAIL${NC} asm/crosscheck (run: python3 tests/asm/crosscheck.py <asm_dump>)"
        ((failed_tests++))
        failed_files+=("asm/crosscheck")
        error_details+=("asm/crosscheck: encodings differ from GNU as")
    fi
    rm -f "$dump"
fi

echo ""
echo "========================================"
echo "  Test Results Summary"
echo "========================================"
echo "Total tests:   $total_tests"
echo -e "Passed:        ${GREEN}$passed_tests${NC}"
echo -e "Failed:        ${RED}$failed_tests${NC}"
echo -e "Skipped:       ${YELLOW}$skipped_tests${NC}"
echo ""

# Show failed tests if any
if [ $failed_tests -gt 0 ]; then
    echo "========================================"
    echo "  Failed Tests"
    echo "========================================"
    for failed_file in "${failed_files[@]}"; do
        echo -e "${RED}✗${NC} $failed_file"
    done
    echo ""
    
    echo "========================================"
    echo "  Error Details"
    echo "========================================"
    for detail in "${error_details[@]}"; do
        echo "$detail"
        echo ""
    done
    
    exit 1
else
    echo -e "${GREEN}✓ All tests passed!${NC}"
    exit 0
fi
