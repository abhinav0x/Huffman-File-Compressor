# Huffman File Compressor — C++

A command-line tool that compresses `.txt` files using **Huffman Coding**
and decompresses them back to the original text. No external libraries needed —
pure C++17 standard library only.

---

## Build

```bash
# Using make
make

# Or manually
g++ -std=c++17 -O2 -Wall -o huffman huffman.cpp
```

Requires a C++17-capable compiler (GCC 8+, Clang 7+, MSVC 2017+).

---

## Usage

```bash
# Compress
./huffman compress   input.txt    output.huff

# Decompress
./huffman decompress output.huff  recovered.txt
```

---

## How It Works

### Step-by-step

1. **Frequency Table** — Count how often each character appears in the file.
2. **Min-Heap** — Insert every character as a leaf node, ordered by frequency.
3. **Build Tree** — Repeatedly merge the two lowest-frequency nodes into a
   parent, until only the root remains.
4. **Generate Codes** — Walk the tree; left edge = `0`, right edge = `1`.
   Frequent characters get shorter codes.
5. **Encode** — Replace every character with its bit-code; pack bits into bytes.
6. **Write `.huff`** — Save magic header + code table + compressed payload.
7. **Decompress** — Read the code table, rebuild the tree, walk it bit-by-bit
   to recover the original text.

### .huff File Format

```
┌─────────────┬──────────────────────────────────────────┐
│  4 bytes    │ Magic: "HUFF"                            │
│  1 byte     │ Padding bits appended to last byte (0–7) │
│  4 bytes    │ Number of entries in code table (uint32) │
│  per entry  │ 1B char + 1B code-length + packed bits   │
│  rest       │ Compressed payload                       │
└─────────────┴──────────────────────────────────────────┘
```

---

## Example

```bash
echo "this is an example of a huffman tree" > sample.txt

./huffman compress   sample.txt  sample.huff
./huffman decompress sample.huff recovered.txt

diff sample.txt recovered.txt   # no output = perfect match
```

---

## Complexity

| Step            | Time         | Space   |
|-----------------|-------------|---------|
| Frequency count | O(n)        | O(σ)    |
| Build heap      | O(σ log σ)  | O(σ)    |
| Encode          | O(n)        | O(n)    |
| Decode          | O(n)        | O(n)    |

`n` = input size, `σ` = alphabet size (≤ 256 for bytes).

---

## What's Next?

See the next steps section below the project for ideas on extending this tool.
