@echo off
rem Run build.py from any directory; all command-line options are forwarded.
rem Usage: build.bat --vc7 "C:\...\Vc7" --strict --identical
rem Prefer a working Python 3 launcher, then try python from PATH.
setlocal
rem A launcher can be installed even when its selected interpreter is incomplete.
py -3 -c "import sys" >nul 2>nul
if errorlevel 1 goto try_python
py -3 "%~dp0build.py" %*
exit /b %errorlevel%

:try_python
python -c "import sys; sys.exit(0 if sys.version_info[0] == 3 else 1)" >nul 2>nul
if errorlevel 1 goto no_python
python "%~dp0build.py" %*
exit /b %errorlevel%

:no_python
echo Python 3 could not start. Install Python 3 and add it to PATH, then retry. 1>&2
exit /b 1
