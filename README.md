# c-dbase

A lightweight, in-memory key-value store written in C99. Uses a hash table with separate chaining for collision resolution and automatic resize based on load factor. Supports insert, get, and delete operations on string keys and values. No external dependencies -- only the C standard library (stdlib.h, string.h, stdio.h).
