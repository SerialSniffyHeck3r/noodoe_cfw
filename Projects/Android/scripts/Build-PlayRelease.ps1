param(
    [Parameter(Mandatory = $true)][string]$SigningProperties,
    [Parameter(Mandatory = $true)][string]$AndroidSdk,
    [string]$JavaHome = 'C:\Program Files\Android\Android Studio\jbr',
    [string]$Gradle = (Join-Path $PSScriptRoot '..\..\..\Reversing\.tools\gradle-8.7\bin\gradle.bat'),
    [switch]$Offline
)

$ErrorActionPreference = 'Stop'
$signingFile = (Resolve-Path -LiteralPath $SigningProperties).Path
$sdkPath = (Resolve-Path -LiteralPath $AndroidSdk).Path
$javaPath = (Resolve-Path -LiteralPath $JavaHome).Path
$gradlePath = (Resolve-Path -LiteralPath $Gradle).Path
$projectPath = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$previousJava = $env:JAVA_HOME
$previousSdk = $env:ANDROID_HOME
$previousSdkRoot = $env:ANDROID_SDK_ROOT
try {
    $env:JAVA_HOME = $javaPath
    $env:ANDROID_HOME = $sdkPath
    $env:ANDROID_SDK_ROOT = $sdkPath
    $gradleArguments = @(
        '--no-daemon', '--console=plain', '-p', $projectPath,
        "-PreNudoSigningProperties=$signingFile",
        'testReleaseUnitTest', 'lintRelease', 'assembleRelease', 'bundleRelease'
    )
    if ($Offline) { $gradleArguments += '--offline' }
    & $gradlePath @gradleArguments
    if ($LASTEXITCODE -ne 0) { throw "Release build failed (exit $LASTEXITCODE)." }
    Write-Output "Play upload: $projectPath\app\build\outputs\bundle\release\app-release.aab"
    Write-Output 'The APK is for local verification only; Play signs installed apps with its app-signing key.'
} finally {
    $env:JAVA_HOME = $previousJava
    $env:ANDROID_HOME = $previousSdk
    $env:ANDROID_SDK_ROOT = $previousSdkRoot
}