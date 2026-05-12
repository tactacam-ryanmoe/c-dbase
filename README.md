# c-dbase

Lightweight in-memory key-value store written in C99. Stores string values mapped to string keys using a hash table with separate chaining and djb2 hashing. No external dependencies — only the C Standard Library (`stdlib.h`, `string.h`, `stdio.h`).

## Build

```bash
make
```

## Run

```bash
./c-dbase
```

## Clean

```bash
make clean
```

## Specification

See [SPECv3.md](SPECv3.md) for the full design specification.
