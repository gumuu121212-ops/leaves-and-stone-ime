[CmdletBinding()]
param(
  [string]$Thumbprint,
  [string]$PfxPath,
  [securestring]$PfxPassword,
  [string]$TimestampUrl = 'http://timestamp.digicert.com',
  [string[]]$Path,
  [switch]$SkipVerify,
  [switch]$DryRun
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot

. (Join-Path $PSScriptRoot 'verify-toolchain.ps1') -NoExit
$toolchain = Get-LeavesStoneToolchainStatus
# SignTool ships with the Windows SDK, but Authenticode signing over the same
# APIs is built into PowerShell, so a machine without the SDK can still sign
# with a certificate from its own store.
$signTool = $toolchain.WindowsSdk.Path
$useBuiltInSigning = -not $signTool
if ($useBuiltInSigning -and $PfxPath) {
  throw 'The built-in signing path only accepts -Thumbprint. Install the Windows SDK to sign from a PFX file.'
}

if (-not $Thumbprint -and -not $PfxPath) {
  throw 'Provide either -Thumbprint (certificate in the current user store) or -PfxPath (certificate file kept outside this repository).'
}

$signArguments = @('sign', '/fd', 'sha256', '/tr', $TimestampUrl, '/td', 'sha256')

if ($Thumbprint) {
  $thumb = ($Thumbprint -replace '\s', '').ToUpperInvariant()
  $certificate = @(
    Get-ChildItem -Path Cert:\CurrentUser\My, Cert:\LocalMachine\My -CodeSigningCert -ErrorAction SilentlyContinue |
      Where-Object { $_.Thumbprint -eq $thumb }
  ) | Select-Object -First 1
  if (-not $certificate) {
    throw "No code-signing certificate with thumbprint $thumb was found in the current user or machine store."
  }
  if (-not $certificate.HasPrivateKey) {
    throw "Certificate $thumb has no private key in this store; signing would fail."
  }
  $signArguments += @('/sha1', $thumb)
}
else {
  $resolvedPfx = (Resolve-Path -LiteralPath $PfxPath).Path
  if ($resolvedPfx.StartsWith($repoRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to sign with a private key stored inside the repository: $resolvedPfx. Keep the PFX outside $repoRoot."
  }
  if (-not $PfxPassword) {
    throw 'A password is required when signing with -PfxPath. Pass -PfxPassword (SecureString).'
  }
  $plainPassword = [System.Net.NetworkCredential]::new('', $PfxPassword).Password
  $signArguments += @('/f', $resolvedPfx, '/p', $plainPassword)
}

if ($Path) {
  $targets = foreach ($item in $Path) {
    $resolved = (Resolve-Path -LiteralPath $item -ErrorAction SilentlyContinue).Path
    if (-not $resolved) { throw "Signing target not found: $item" }
    $resolved
  }
}
else {
  $archives = Join-Path $repoRoot 'output\archives'
  $targets = @(
    Get-ChildItem -Path (Join-Path $repoRoot 'output\*') -Include '*.exe', '*.dll' -File -ErrorAction SilentlyContinue |
      Select-Object -ExpandProperty FullName
    Get-ChildItem -Path (Join-Path $repoRoot 'output\Win32\*') -Include '*.exe', '*.dll' -File -ErrorAction SilentlyContinue |
      Select-Object -ExpandProperty FullName
    if (Test-Path -LiteralPath $archives) {
      Get-ChildItem -Path $archives -Filter '*-installer.exe' -File -ErrorAction SilentlyContinue |
        Select-Object -ExpandProperty FullName
    }
  )
}

$targets = @($targets | Where-Object { $_ } | Select-Object -Unique)
if ($targets.Count -eq 0) {
  throw 'No signing targets were found. Build the binaries first, or pass -Path explicitly.'
}

$failures = @()
foreach ($target in $targets) {
  $relative = $target.Replace($repoRoot + '\', '')
  if ($DryRun) {
    Write-Output ('sign ' + $relative)
    continue
  }

  if ($useBuiltInSigning) {
    $signature = Set-AuthenticodeSignature -FilePath $target `
      -Certificate $certificate -TimestampServer $TimestampUrl -HashAlgorithm SHA256
    if ($signature.Status -ne 'Valid' -and $signature.Status -ne 'UnknownError') {
      $failures += "$relative (status $($signature.Status): $($signature.StatusMessage))"
      continue
    }
    if (-not $SkipVerify) {
      $check = Get-AuthenticodeSignature -FilePath $target
      if ($check.Status -ne 'Valid' -and $check.Status -ne 'UnknownError') {
        $failures += "$relative (verify status $($check.Status))"
        continue
      }
    }
    Write-Output ('signed ' + $relative + ' (' + $signature.Status + ')')
    continue
  }

  & $signTool @signArguments $target
  if ($LASTEXITCODE -ne 0) {
    $failures += "$relative (sign exit $LASTEXITCODE)"
    continue
  }

  if (-not $SkipVerify) {
    & $signTool verify /pa /q $target
    if ($LASTEXITCODE -ne 0) {
      $failures += "$relative (verify exit $LASTEXITCODE)"
      continue
    }
  }

  Write-Output ('signed ' + $relative)
}

if ($failures.Count -gt 0) {
  throw ('Signing failed for: ' + ($failures -join '; '))
}

Write-Output "Signed $($targets.Count) file(s)."
