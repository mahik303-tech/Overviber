// Included before everything else in the MSVC builds of the scenario tests
// (CMakeLists.txt, /FI): assert() also checks in optimized builds. GCC and
// Clang get -UNDEBUG instead; MSVC warns (D9025) when /UNDEBUG overrides
// the /DNDEBUG of the Release flags.
#undef NDEBUG
