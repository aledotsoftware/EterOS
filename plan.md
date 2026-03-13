1. **Refactor the `kernel_print_banner` function in `kernel/main.c`**.
    - Enhance the ASCII art logo and styling to feel more "premium" using the Austral Aurora theme (TrueColor ANSI escape codes).
    - Ensure it fits the visual guidelines requested for éterOS.
2. **Optimize `#include`s in `kernel/main.c`**.
    - Clean up duplicate or unnecessary includes in the header block of `kernel/main.c`.
3. **Add Doxygen comments to scheduler functions in `kernel/task.c`**.
    - Document critical scheduler API functions like `schedule`, `scheduler_init`, `task_yield`, `task_sleep`, and `task_exit` using Doxygen-style block comments (`@brief`, `@param`, etc.).
4. **Pre-commit step**.
    - Run the pre-commit script or checks required to ensure testing, verifications, reviews, and reflections are performed.
5. **Submit the changes**.
    - Submit the changes using the specified branch name and a descriptive commit message.
