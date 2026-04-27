cat << 'MD' >> .jaa/state.md

## EterOS Scheduler & IPC Basic Fixes (Current Run)
- Addressed scheduler bugs in `kernel/task.c` (`task_sleep`) and `kernel/futex.c` (`futex_wait`) by replacing `task_yield()` with `schedule()` to properly block tasks without immediately returning them to the ready queue.
- Added native support for handling `CLONE_CHILD_SETTID` in `kernel/task.c:task_clone` to correctly set the child TID directly in the user-provided pointer after access validation, enhancing POSIX compatibility.
- Updated `test_futex_logic.c` and `test_futex_timeout.c` tests to mock `schedule()` correctly, passing the modified IPC logic.
- Conducted full `make all` build and executed `tests/run_tests.sh` and QEMU boot test to verify stability.
MD
