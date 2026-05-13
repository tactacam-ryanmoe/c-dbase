# c-dbase

A lightweight, in-memory key-value store written in C99. Uses a hash table with separate chaining (djb2 hashing) built entirely from the C standard library — no external dependencies.

## Operations

- **Insert** — add or update a key-value pair.
- **Get** — retrieve the value for a given key.
- **Delete** — remove a key-value pair.

## Build

```bash
make
```

## Run Tests

```bash
./c-dbase
```

The built-in test harness exercises all operations, auto-resize behavior, and memory leak detection. Clean output shows `[PASS]` lines and a summary count at the end.
