# Mandelbrot Agent Notes

Follow the formatting already established in `mandelbrot`, `mandelbrot-cli`, `common`, and `frontend` when working on the GUI project.

For function-call formatting, do not use the empty-open / hanging-close style:

```cpp
call(
    arg1,
    arg2);
```

Prefer one of these instead:

```cpp
call(arg1,
    arg2);
```

```cpp
call(
    arg1, arg2
);
```

```cpp
call(
    arg1,
    arg2
);
```

Prefer the first form when it stays reasonably close to the 80-column target. If that pushes the line too far, use a fully vertical form and keep the closing `)` on its own line.

Do not run formatting scripts over the whole tree unless the user explicitly asks for that.

Never use anonymous namespaces. Use `static` for internal linkage instead.

To do builds, locate msbuild then call it at the solution level with the gui project. do not mess with dependencies if it doesnt work. just build the way visual studio does it
