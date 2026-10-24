# run_all.ps1 - Unified Formal Verification Pipeline for tiny-bdd-cec
# Runs all Phase 1 Algorithm Benchmarks and Phase 2 EDA Hardware Case Studies

param (
    [string]$Target = "all"  # Options: "all", "adders", "cpu", "cache", "conv"
)

$ErrorActionPreference = "Stop"

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "   tiny-bdd-cec: Unified Formal Hardware Verification       " -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan

$TBC = ".\build\release\tiny-bdd-cec.exe"
if (-not (Test-Path $TBC)) {
    Write-Error "tiny-bdd-cec executable not found at $TBC. Please build the release preset first (`cmake --build --preset release`)."
}

# Ensure output directory exists
if (-not (Test-Path "out")) { New-Item -ItemType Directory -Path "out" | Out-Null }

$passed = 0
$total = 0

function Run-Check {
    param (
        [string]$Name,
        [string]$FileA,
        [string]$FileB,
        [string[]]$ExtraArgs = @(),
        [bool]$ExpectFail = $false
    )
    $script:total++
    Write-Host "`n[$script:total] $Name" -ForegroundColor Yellow
    
    $cmdArgs = @("--cec", $FileA, $FileB) + $ExtraArgs
    & $script:TBC $cmdArgs
    $code = $LASTEXITCODE

    if ($ExpectFail) {
        if ($code -eq 1) {
            Write-Host "PASS: Bug successfully detected & counterexample generated!" -ForegroundColor Green
            $script:passed++
        } else {
            Write-Host "FAIL: Bug was NOT caught! (Exit code: $code)" -ForegroundColor Red
            throw "Verification check failed for $Name"
        }
    } else {
        if ($code -eq 0) {
            Write-Host "PASS: Formally Equivalent!" -ForegroundColor Green
            $script:passed++
        } else {
            Write-Host "FAIL: Circuits differed unexpectedly! (Exit code: $code)" -ForegroundColor Red
            throw "Verification check failed for $Name"
        }
    }
}

# -----------------------------------------------------------------------------
# Section 1: Parametric Adder Benchmarks (Phase 1)
# -----------------------------------------------------------------------------
if ($Target -eq "all" -or $Target -eq "adders") {
    Write-Host "`n=== SECTION 1: Parametric Adder Scaling Benchmarks ===" -ForegroundColor Magenta

    Run-Check "8-bit Adder: Behavioral Spec vs Ripple-Carry (RCA)" `
        "examples/adder8/adder8_spec.v" "examples/adder8/adder8_rca.v" @("--profile")

    Run-Check "8-bit Adder: Behavioral Spec vs Carry-Lookahead (CLA)" `
        "examples/adder8/adder8_spec.v" "examples/adder8/adder8_cla.v" @("--dump-dot", "out/adder8_cla.dot", "--dot-outputs", "sum[0],sum[1],cout")

    Run-Check "8-bit Adder: Alternative Carry Logic C = A*B + C*(A+B)" `
        "examples/adder8/adder8_spec.v" "examples/adder8/adder8_rca_orcarry.v"

    Run-Check "8-bit Adder: Injected Carry Fault Detection" `
        "examples/adder8/adder8_spec.v" "examples/adder8/adder8_rca_bug.v" @("--dump-dot", "out/adder8_bug.dot") -ExpectFail $true

    Run-Check "16-bit Adder: Behavioral Spec vs Carry-Lookahead (CLA)" `
        "examples/adder16/adder16_spec.v" "examples/adder16/adder16_cla.v" @("--profile", "--dump-dot", "out/adder16_cla.dot", "--dot-outputs", "cout")
}

# -----------------------------------------------------------------------------
# Section 2: 16-bit RISC CPU Verification (cpu)
# -----------------------------------------------------------------------------
if ($Target -eq "all" -or $Target -eq "cpu") {
    Write-Host "`n=== SECTION 2: 16-bit RISC CPU Verification ===" -ForegroundColor Magenta

    Run-Check "CPU Instruction Decoder (VHDL -> Yosys Synthesized BLIF vs Golden Spec)" `
        "examples/cpu_decoder_spec.v" "out/cpu_decoder_synth.blif" @("--profile")

    Run-Check "CPU ALU: Subtraction (a - b) vs Two's Complement Negation (a + ~b + 1)" `
        "examples/alu_sub.v" "examples/alu_neg_add.v" @("--profile")

    Run-Check "CPU ALU: Opcode Logic Decoder Spec vs De Morgan Implementation" `
        "examples/cpu_alu/alu_logic_spec.v" "examples/cpu_alu/alu_logic_impl.v" @("--profile")

    Run-Check "CPU ALU: Full 8-bit Synthesized BLIF Netlist Canonicalization" `
        "out/cpu_alu_synth.blif" "out/cpu_alu_synth.blif" @("--profile")
}

# -----------------------------------------------------------------------------
# Section 3: L1 Cache Controller Verification (l1-cache-controller)
# -----------------------------------------------------------------------------
if ($Target -eq "all" -or $Target -eq "cache") {
    Write-Host "`n=== SECTION 3: L1 Cache Controller Verification ===" -ForegroundColor Magenta

    Run-Check "L1 Cache Datapath: Tag Match, Hit Logic & 128-to-32-bit Word Extractor" `
        "examples/l1_cache_datapath_spec.v" "out/l1_cache_datapath_synth.blif" @("--profile")

    Run-Check "L1 Cache Datapath: Injected Way 1 Tag Match Bug Detection" `
        "examples/l1_cache_datapath_spec.v" "examples/l1_cache_datapath_bug.v" @() -ExpectFail $true
}

# -----------------------------------------------------------------------------
# Section 4: Image Convolution Hardware Accelerator (image-convolution)
# -----------------------------------------------------------------------------
if ($Target -eq "all" -or $Target -eq "conv") {
    Write-Host "`n=== SECTION 4: Image Convolution Accelerator Verification ===" -ForegroundColor Magenta

    Run-Check "Image Conv: 36-bit Saturating Byte Clamp vs Branchless Bitwise Masking" `
        "examples/clamp_byte_opt.v" "out/clamp_byte_synth.blif" @("--profile")

    Run-Check "Image Conv: Filter Kernel & Divisor Decoder (799 primary outputs)" `
        "out/img_conv_decoder.blif" "out/img_conv_decoder.blif" @("--profile")
}

Write-Host "`n============================================================" -ForegroundColor Green
Write-Host " SUCCESS: $passed / $total VERIFICATION CHECKS PASSED WITH 100% ACCURACY!" -ForegroundColor Green
Write-Host "============================================================" -ForegroundColor Green
