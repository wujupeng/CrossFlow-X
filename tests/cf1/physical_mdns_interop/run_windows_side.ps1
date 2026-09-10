# CrossFlow-X Physical mDNS Interoperability Test - Windows Side
# Run on Windows machine: powershell .\run_windows_side.ps1

Write-Host "=== CrossFlow-X Physical mDNS Interoperability Test (Windows) ==="
Write-Host ""
Write-Host "This script runs on Windows."
Write-Host "Ensure macOS machine is running its side simultaneously."
Write-Host ""

$toolDir = "$PSScriptRoot\..\..\build\tests\cf1\Debug"

Write-Host "=== Test A: macOS Announcer -> Windows Listener ==="
Write-Host "Ensure macOS is running: ./mdns_announcer_mac.sh"
Write-Host "Starting Windows listener..."
Write-Host ""

& "$toolDir\mdns_listener_tool.exe" --timeout 10

Write-Host ""
Write-Host "=== Test B: Windows Announcer -> macOS Listener ==="
Write-Host "Ensure macOS is running: ./mdns_listener_mac.sh"
Write-Host "Starting Windows announcer..."
Write-Host ""

& "$toolDir\mdns_announcer_tool.exe" --service "CFX-WinTest" --port 5353 --topology "phy-test" --node-high 1 --node-low 200

Write-Host ""
Write-Host "=== Test Complete ==="
Write-Host "Collect evidence from both sides and attach to Evidence Report v8."