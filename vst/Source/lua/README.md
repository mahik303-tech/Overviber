# Lua 5.4.7

Unmodified sources of Lua 5.4.7 (tag `v5.4.7` of https://github.com/lua/lua),
MIT license (`LICENSE.txt`). Used by the Lua tab of the Modern skin
(`vst/Source/ui/lua`).

Left out on purpose, so that a script cannot reach them even by mistake:
the interpreter and compiler (`lua.c`, `luac.c`, `onelua.c`), the test
library (`ltests.*`) and the libraries for files, the operating system,
modules and debugging (`liolib.c`, `loslib.c`, `loadlib.c`, `ldblib.c`)
together with `linit.c`, which would open them.

CMake builds the sources as C++ (`OverviberLua`), so a Lua error unwinds
the C++ stack of the binding functions with an exception instead of a
`longjmp`, and destructors run.
