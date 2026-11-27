# Code style

## Index

* [Which code](#which-code)
* [C23 as it is written](#c23-as-it-is-written)
* [No undefined behaviour](#no-undefined-behaviour)
* [Where state lives](#where-state-lives)
* [Checking a change](#checking-a-change)

The C of the frontends that build as C23, written the way ScarletNX writes its own (its RFC #142), so a case moves
between the probe and the emulator it checks without being rewritten.

## Which code

The Vita (`vita/`, `vita.mk`), the 3DS and the DS frontends build `-std=c23 -Wall -Wextra -Werror` and follow this
page. The engine (`source/portable.txt`) compiles unchanged under them. Everything else stays C11 for now: `host.mk`,
the NRO, the ExeFS and the mod, Android and `tools/vklayer`; a file of theirs keeps the C11 it has.

Names, comments and layout stay the repository's: `CamelCase` functions (`PlatformLog`, `RunnerSteps`), `///` before
a declaration, the file's own indentation.

## C23 as it is written

- `static_assert(cond, "why")` for a layout or a count, never a negative-size array nor `_Static_assert`.
- Attributes as `[[...]]`: `[[noreturn]]`, `[[maybe_unused]]` on a parameter a callback does not read (not
  `(void)name;`), `[[gnu::x]]` for the compiler's own; `alignas(N)` for an object's alignment. Never
  `__attribute__((x))` nor `_Noreturn`.
- `nullptr`, not `NULL`; `bool`, `true` and `false` without `<stdbool.h>`; `constexpr` for a constant a size or a
  `static_assert` reads.
- A local that starts zeroed is `= {0}` (or `{}` where the initializer has nothing to say), not a `memset` after it;
  an object behind a pointer is zeroed with `*p = (T){0};`; a struct is copied by assignment, not `memcpy`.
  `memset` and `memcpy` stay where the bytes matter (a file, the wire, a GPU buffer, bytes hashed or compared),
  between different types, and for objects over about a kilobyte.
- A local is declared where it first gets its value, in the innermost block that uses it; a loop counter in its
  `for`. No block of bare declarations at the top of a function.
- Atomics are `_Atomic` objects used through `<stdatomic.h>`, each access an `atomic_*_explicit` naming its order.
  Never `__atomic_*` nor `__sync_*`.
- `<stdint.h>` types for sizes the hardware fixes, `%zu` for a `size_t`, `%llu`/`%llx` through
  `(unsigned long long)` for a 64-bit value, `snprintf`, never `sprintf`.

## No undefined behaviour

No undefined and no implementation-defined behaviour, as a case's answer depends on it: a right shift of a signed
value that can be negative goes through a helper made of defined operations; a mask is built unsigned (`~(1u << 31)`);
an offset goes into an integer or a `char *`, never onto a `void *`; a size check is `n > SIZE_MAX / size` before the
multiplication. A case that asks the machine what C leaves to the implementation says so in its `expected_from`.

## Where state lives

1. **One global per file.** A file's mutable state, its function-local `static`s too, is the members of one struct in
   one global, `g_<file>` (`g_vita` in `vita/frontend.c`); its thread state one thread-local struct, `t_<file>`. Not
   for `const` or `constexpr` tables, which are data.
2. **No state another value already says.** A `nullptr` is absent: no `is_present` beside it.
3. **Indices, not pointer chains.** Objects in one array, linked by index; 0 is none, so slot 0 is never an object.
4. **No stored pointers in data.** Data refers to its parts by index or offset, so a copy or a file holds together.
   Handles (a file, a GPU object) stay in a runtime struct beside it.
5. **One allocation per owner.** An owner's arrays come from one block carved by offsets, sizes checked against
   `SIZE_MAX`, one `free`. Not for an array that grows on its own.
6. **Atomics apart.** Atomics that different threads write get a cache line each (`alignas(64)`).

## Checking a change

Before a pull request, for each C23 file it adds or changes:

- it builds with its frontend's compiler at `-std=c23 -Wall -Wextra -Werror`, and again with `-pedantic-errors`,
  which refuses a GNU extension ISO C23 has a form for;
- `grep -nE '\bNULL\b|__attribute__|_Static_assert|_Noreturn|__atomic_|__sync_|\(void\)[a-z_]+;'` over it finds
  nothing;
- a `memset(&x, 0, sizeof x)` of a local, a loose `static` beside a file's `g_<file>`, and a pointer kept in data are
  read for in the diff, as no compiler flags them.
