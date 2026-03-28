# validate_tests.ps1 - Occult Test Suite Validation

$RED    = "`e[0;31m"
$GREEN  = "`e[0;32m"
$YELLOW = "`e[1;33m"
$NC     = "`e[0m"

$total_tests   = 0
$passed_tests  = 0
$failed_tests  = 0
$skipped_tests = 0
$failed_files  = @()
$error_details = @()

Write-Host "========================================"
Write-Host "  Occult Test Suite Validation"
Write-Host "========================================"
Write-Host ""

if (-not (Test-Path ".\build\Release\occultc.exe")) {
    Write-Host "${RED}Error: Compiler not found at .\build\Release\occultc.exe${NC}"
    Write-Host "Please run build.bat first"
    exit 1
}

Write-Host "Compiler found: .\build\Release\occultc.exe"
Write-Host ""

foreach ($test_file in Get-ChildItem -Path "tests\*.occ") {
    $base_name = $test_file.Name

    if ($base_name -eq "test.occ") {
        Write-Host "${YELLOW}SKIP${NC} $base_name (known parser issues)"
        $skipped_tests++
        continue
    }

    $total_tests++

    # Run compiler, capture stderr, suppress stdout, 5s timeout
    $proc = Start-Process `
        -FilePath ".\build\Release\occultc.exe" `
        -ArgumentList $test_file.FullName `
        -NoNewWindow -PassThru `
        -RedirectStandardOutput "NUL" `
        -RedirectStandardError  "temp_error.txt"

    $finished = $proc.WaitForExit(5000)
    if (-not $finished) {
        $proc.Kill()
        Write-Host "${YELLOW}TIMEOUT${NC} $base_name"
        $failed_tests++
        $failed_files  += $base_name
        $error_details += "${base_name}: Compilation timeout"
        if (Test-Path "temp_error.txt") { Remove-Item "temp_error.txt" }
        continue
    }

    $compile_result = $proc.ExitCode
    $error_output   = ""
    if (Test-Path "temp_error.txt") {
        $error_output = (Get-Content "temp_error.txt" -Raw) -replace "`r`n", " " -replace "`n", " "
        Remove-Item "temp_error.txt"
    }

    if ($error_output -match "PARSE ERROR|Function '[^']*' not found|Function `"[^`"]*`" not found") {
        Write-Host "${RED}FAIL${NC} $base_name (compilation error)"
        $failed_tests++
        $failed_files  += $base_name
        $error_details += "${base_name}: $error_output"
    } elseif ($compile_result -eq 139 -or $error_output -match "Segmentation fault") {
        Write-Host "${RED}FAIL${NC} $base_name (segmentation fault)"
        $failed_tests++
        $failed_files  += $base_name
        $error_details += "${base_name}: Segmentation fault (exit code $compile_result)"
    } elseif ($compile_result -ne 0) {
        Write-Host "${RED}FAIL${NC} $base_name (exit code $compile_result)"
        $failed_tests++
        $failed_files  += $base_name
        $error_details += "${base_name}: Non-zero exit code $compile_result"
    } else {
        Write-Host "${GREEN}PASS${NC} $base_name"
        $passed_tests++
    }
}

Write-Host ""
Write-Host "========================================"
Write-Host "  Test Results Summary"
Write-Host "========================================"
Write-Host "Total tests: $total_tests"
Write-Host "Passed:  ${GREEN}${passed_tests}${NC}"
Write-Host "Failed:  ${RED}${failed_tests}${NC}"
Write-Host "Skipped: ${YELLOW}${skipped_tests}${NC}"
Write-Host ""

if ($failed_tests -gt 0) {
    Write-Host "========================================"
    Write-Host "  Failed Tests"
    Write-Host "========================================"
    foreach ($f in $failed_files) {
        Write-Host "  ${RED}x${NC} $f"
    }
    Write-Host ""
    Write-Host "========================================"
    Write-Host "  Error Details"
    Write-Host "========================================"
    foreach ($d in $error_details) {
        Write-Host "  $d"
        Write-Host ""
    }
    exit 1
} else {
    Write-Host "${GREEN}All tests passed!${NC}"
    exit 0
}
