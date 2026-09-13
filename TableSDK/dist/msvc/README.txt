Run scripts\build_msvc.bat TWICE from two different prompts:

  1. "x64 Native Tools Command Prompt for VS"  -> produces dist\msvc\x64\table.lib
  2. "x86 Native Tools Command Prompt for VS"  -> produces dist\msvc\x86\table.lib

Then copy each into the matching payload folder before rebuilding installer.exe:

  copy /Y dist\msvc\x64\table.lib payload\lib\msvc\x64\table.lib
  copy /Y dist\msvc\x86\table.lib payload\lib\msvc\x86\table.lib

Verify each with dumpbin before trusting it:

  dumpbin /linkermember:1 payload\lib\msvc\x64\table.lib | findstr "vSortTable bFilterTable vTransformTable vDropArena"
  dumpbin /linkermember:1 payload\lib\msvc\x86\table.lib | findstr "vSortTable bFilterTable vTransformTable vDropArena"

Neither can be cross-compiled or verified outside a real Windows+MSVC machine.
