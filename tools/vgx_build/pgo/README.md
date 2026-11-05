# The Vita build's execution profile

`game.profdata` tells the compiler which code runs and how often, for its inlining, branch layout and what it builds for size. It
does not change what the game does. Stale entries are harmless: a function whose code has changed is compiled as if it had no
profile.

Made on the native harness (`armhf/`, built for the host), which compiles the same source with the same defines:

1. Build the harness with `-fprofile-instr-generate` on the game, platform and renderer objects, and call
   `__llvm_profile_write_file()` before its `_exit`.
2. Run it in each galaxy with `LLVM_PROFILE_FILE=<dir>/pgo.profraw`, Mario running, jumping, spinning and shooting from the
   moment the stage is up. Standing still will not do: code the profile has not seen is built for size.
3. `llvm-profdata merge --text` on the harness's compiler, then `llvm-profdata merge -o game.profdata` on clang 18, the build's
   compiler. Their binary formats differ; the text format is what they share.

Keep the zero counts (no `--sparse`): they are what tells the compiler a function is cold, and most of the 1.1 MB of code this saves.
ninja does not track this file: use a new build directory after changing it.
