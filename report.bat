@echo off
setlocal EnableDelayedExpansion

set report=%1

set options=(COMPILE)

for %%o in %options% do if "%%o" == "%report%" goto :%%o

echo Available reports: %options%
goto :EXIT


:COMPILE
PowerShell -Command "& {(Get-Content report-parts/README.part.md)|Foreach-Object{if($_-match"""@@(.*part.md)@@"""){Get-Content report-parts/$($Matches.1)}else{$_}}|Set-Content README.md}"

:EXIT
