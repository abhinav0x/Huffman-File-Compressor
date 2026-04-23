#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <queue>
#include <vector>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <filesystem>

namespace fs = std::filesystem;

// ─────────────────────────────────────────────────────────────
// Huffman Tree Node
// ─────────────────────────────────────────────────────────────
struct Node {
    uint8_t  ch;
    uint64_t freq;
    std::shared_ptr<Node> left, right;

    Node(uint8_t c, uint64_t f)
        : ch(c), freq(f), left(nullptr), right(nullptr) {}

    bool isLeaf() const { return !left && !right; }
};

struct NodeCmp {
    bool operator()(const std::shared_ptr<Node>& a,
                    const std::shared_ptr<Node>& b) const {
        return a->freq > b->freq;
    }
};

// ─────────────────────────────────────────────────────────────
// Build frequency table from raw bytes
// ─────────────────────────────────────────────────────────────
std::unordered_map<uint8_t, uint64_t>
buildFrequency(const std::vector<uint8_t>& data) {
    std::unordered_map<uint8_t, uint64_t> freq;
    for (uint8_t b : data) freq[b]++;
    return freq;
}

// ─────────────────────────────────────────────────────────────
// Build Huffman Tree
// ─────────────────────────────────────────────────────────────
std::shared_ptr<Node>
buildTree(const std::unordered_map<uint8_t, uint64_t>& freq) {
    std::priority_queue<std::shared_ptr<Node>,
                        std::vector<std::shared_ptr<Node>>,
                        NodeCmp> heap;

    for (auto& [ch, f] : freq)
        heap.push(std::make_shared<Node>(ch, f));

    if (heap.size() == 1) {
        auto only = heap.top(); heap.pop();
        auto root = std::make_shared<Node>(0, only->freq);
        root->left = only;
        return root;
    }

    while (heap.size() > 1) {
        auto left  = heap.top(); heap.pop();
        auto right = heap.top(); heap.pop();
        auto merged = std::make_shared<Node>(0, left->freq + right->freq);
        merged->left  = left;
        merged->right = right;
        heap.push(merged);
    }
    return heap.top();
}

// ─────────────────────────────────────────────────────────────
// Generate code table
// ─────────────────────────────────────────────────────────────
void generateCodes(const std::shared_ptr<Node>& node,
                   const std::string& prefix,
                   std::unordered_map<uint8_t, std::string>& codes) {
    if (!node) return;
    if (node->isLeaf()) {
        codes[node->ch] = prefix.empty() ? "0" : prefix;
        return;
    }
    generateCodes(node->left,  prefix + "0", codes);
    generateCodes(node->right, prefix + "1", codes);
}

// ─────────────────────────────────────────────────────────────
// Encode bytes → packed bytes
// ─────────────────────────────────────────────────────────────
struct EncodeResult {
    std::vector<uint8_t> bytes;
    uint8_t padding;
};

EncodeResult encode(const std::vector<uint8_t>& data,
                    const std::unordered_map<uint8_t, std::string>& codes) {
    std::string bits;
    bits.reserve(data.size() * 4);
    for (uint8_t b : data)
        bits += codes.at(b);

    uint8_t padding = static_cast<uint8_t>((8 - bits.size() % 8) % 8);
    for (uint8_t i = 0; i < padding; i++) bits += '0';

    std::vector<uint8_t> result;
    result.reserve(bits.size() / 8);
    for (size_t i = 0; i < bits.size(); i += 8) {
        uint8_t byte = 0;
        for (int b = 0; b < 8; b++)
            if (bits[i + b] == '1') byte |= (1 << (7 - b));
        result.push_back(byte);
    }
    return { result, padding };
}

// ─────────────────────────────────────────────────────────────
// Decode packed bytes → raw bytes
// ─────────────────────────────────────────────────────────────
std::vector<uint8_t> decode(const std::vector<uint8_t>& data,
                             uint8_t padding,
                             const std::shared_ptr<Node>& root,
                             uint64_t originalSize) {
    std::string bits;
    bits.reserve(data.size() * 8);
    for (uint8_t byte : data)
        for (int b = 7; b >= 0; b--)
            bits += ((byte >> b) & 1) ? '1' : '0';

    if (padding) bits.resize(bits.size() - padding);

    std::vector<uint8_t> result;
    result.reserve(originalSize);
    auto node = root;
    for (char bit : bits) {
        node = (bit == '0') ? node->left : node->right;
        if (node->isLeaf()) {
            result.push_back(node->ch);
            node = root;
            if (result.size() == originalSize) break; // exact size guard
        }
    }
    return result;
}

// ─────────────────────────────────────────────────────────────
// Write / read primitives
// ─────────────────────────────────────────────────────────────
static void writeU32(std::ofstream& out, uint32_t v) {
    uint8_t buf[4] = {
        static_cast<uint8_t>(v >> 24), static_cast<uint8_t>(v >> 16),
        static_cast<uint8_t>(v >>  8), static_cast<uint8_t>(v)
    };
    out.write(reinterpret_cast<char*>(buf), 4);
}

static uint32_t readU32(std::ifstream& in) {
    uint8_t buf[4];
    in.read(reinterpret_cast<char*>(buf), 4);
    return (static_cast<uint32_t>(buf[0]) << 24) |
           (static_cast<uint32_t>(buf[1]) << 16) |
           (static_cast<uint32_t>(buf[2]) <<  8) |
            static_cast<uint32_t>(buf[3]);
}

static void writeU64(std::ofstream& out, uint64_t v) {
    uint8_t buf[8];
    for (int i = 7; i >= 0; i--) { buf[i] = v & 0xFF; v >>= 8; }
    out.write(reinterpret_cast<char*>(buf), 8);
}

static uint64_t readU64(std::ifstream& in) {
    uint8_t buf[8];
    in.read(reinterpret_cast<char*>(buf), 8);
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v = (v << 8) | buf[i];
    return v;
}

// ─────────────────────────────────────────────────────────────
// Serialize / deserialize code table
// ─────────────────────────────────────────────────────────────
void writeCodeTable(std::ofstream& out,
                    const std::unordered_map<uint8_t, std::string>& codes) {
    writeU32(out, static_cast<uint32_t>(codes.size()));
    for (auto& [ch, code] : codes) {
        out.put(static_cast<char>(ch));
        out.put(static_cast<char>(code.size()));
        uint8_t padding = static_cast<uint8_t>((8 - code.size() % 8) % 8);
        std::string padded = code + std::string(padding, '0');
        for (size_t i = 0; i < padded.size(); i += 8) {
            uint8_t byte = 0;
            for (int b = 0; b < 8; b++)
                if (padded[i + b] == '1') byte |= (1 << (7 - b));
            out.put(static_cast<char>(byte));
        }
    }
}

std::shared_ptr<Node> readCodeTable(std::ifstream& in) {
    uint32_t count = readU32(in);
    auto root = std::make_shared<Node>(0, 0);
    for (uint32_t i = 0; i < count; i++) {
        uint8_t ch      = static_cast<uint8_t>(in.get());
        uint8_t codeLen = static_cast<uint8_t>(in.get());
        uint8_t byteCount = static_cast<uint8_t>((codeLen + 7) / 8);
        std::vector<uint8_t> buf(byteCount);
        in.read(reinterpret_cast<char*>(buf.data()), byteCount);

        std::string bits;
        for (uint8_t byte : buf)
            for (int b = 7; b >= 0; b--)
                bits += ((byte >> b) & 1) ? '1' : '0';
        bits.resize(codeLen);

        auto node = root;
        for (char bit : bits) {
            if (bit == '0') {
                if (!node->left)  node->left  = std::make_shared<Node>(0, 0);
                node = node->left;
            } else {
                if (!node->right) node->right = std::make_shared<Node>(0, 0);
                node = node->right;
            }
        }
        node->ch = ch;
    }
    return root;
}

// ─────────────────────────────────────────────────────────────
// Pretty-print
// ─────────────────────────────────────────────────────────────
std::string fmtSize(uint64_t bytes) {
    std::ostringstream ss;
    if (bytes < 1024)         ss << bytes << " B";
    else if (bytes < 1 << 20) ss << std::fixed << std::setprecision(2) << bytes / 1024.0       << " KB";
    else                      ss << std::fixed << std::setprecision(2) << bytes / (1024.0*1024) << " MB";
    return ss.str();
}

// ─────────────────────────────────────────────────────────────
// COMPRESS  — any file type
// ─────────────────────────────────────────────────────────────
void compressFile(const std::string& inPath, const std::string& outPath) {
    // 1. Read all bytes
    std::ifstream fin(inPath, std::ios::binary);
    if (!fin) throw std::runtime_error("Cannot open input: " + inPath);

    std::vector<uint8_t> data(
        (std::istreambuf_iterator<char>(fin)),
         std::istreambuf_iterator<char>());
    fin.close();

    if (data.empty()) throw std::runtime_error("Input file is empty.");

    // Original filename (just the name, no directory)
    std::string origName = fs::path(inPath).filename().string();
    if (origName.size() > 255)
        origName = origName.substr(0, 255);  // clamp to 1-byte length field

    std::cout << "  Reading   : " << inPath << "\n";
    std::cout << "  File type : " << fs::path(inPath).extension().string() << "\n";
    std::cout << "  Size      : " << fmtSize(data.size()) << "  (" << data.size() << " bytes)\n";

    // 2. Build tree & codes
    auto freq = buildFrequency(data);
    auto tree = buildTree(freq);
    std::unordered_map<uint8_t, std::string> codes;
    generateCodes(tree, "", codes);

    // 3. Encode
    auto [payload, padding] = encode(data, codes);

    // 4. Write .huff
    std::ofstream fout(outPath, std::ios::binary);
    if (!fout) throw std::runtime_error("Cannot open output: " + outPath);

    fout.write("HUF2", 4);                                    // magic v2
    fout.put(static_cast<char>(origName.size()));             // filename length
    fout.write(origName.data(), origName.size());             // filename
    fout.put(static_cast<char>(padding));                     // padding bits
    writeCodeTable(fout, codes);                              // code table
    writeU64(fout, static_cast<uint64_t>(data.size()));       // original size
    fout.write(reinterpret_cast<const char*>(payload.data()),
               static_cast<std::streamsize>(payload.size())); // payload
    fout.close();

    // 5. Stats
    uint64_t origSize = data.size();
    uint64_t compSize = payload.size();
    double   ratio    = 100.0 * (1.0 - static_cast<double>(compSize) / origSize);

    std::cout << "\n  ✓ Compressed successfully!\n";
    std::cout << "  Output    : " << outPath << "\n";
    std::cout << "  Original  : " << fmtSize(origSize) << "\n";
    std::cout << "  Compressed: " << fmtSize(compSize) << "\n";
    std::cout << "  Ratio     : " << std::fixed << std::setprecision(1)
              << ratio << "% smaller\n";
    std::cout << "  Unique b  : " << freq.size() << " / 256\n";

    double entropy = 0.0;
    for (auto& [b, f] : freq) {
        double p = static_cast<double>(f) / origSize;
        entropy -= p * std::log2(p);
    }
    double bitsPerByte = (compSize * 8.0) / origSize;

    std::cout << "\n  --- Compression Report ---\n";
    std::cout << "  Shannon Entropy : " << std::fixed << std::setprecision(4)
              << entropy << " bits/byte\n";
    std::cout << "  Achieved        : " << std::setprecision(4)
              << bitsPerByte << " bits/byte\n";
    std::cout << "  Efficiency      : " << std::setprecision(1)
              << (entropy / bitsPerByte * 100.0) << "%\n";
}

// ─────────────────────────────────────────────────────────────
// DECOMPRESS — restores any file type
// ─────────────────────────────────────────────────────────────
// Returns the original filename stored in the header.
std::string decompressFile(const std::string& inPath, const std::string& outPath) {
    std::ifstream fin(inPath, std::ios::binary);
    if (!fin) throw std::runtime_error("Cannot open input: " + inPath);

    // Magic
    char magic[4];
    fin.read(magic, 4);
    if (std::strncmp(magic, "HUF2", 4) != 0) {
        // Backward-compat check for old v1 files
        if (std::strncmp(magic, "HUFF", 4) == 0)
            throw std::runtime_error(
                "This is a v1 .huff file (text-only). "
                "Re-compress with the updated tool.");
        throw std::runtime_error("Not a valid .huff file (bad magic bytes).");
    }

    // Original filename
    uint8_t nameLen = static_cast<uint8_t>(fin.get());
    std::string origName(nameLen, '\0');
    fin.read(origName.data(), nameLen);

    // Padding
    uint8_t padding = static_cast<uint8_t>(fin.get());

    // Code table → tree
    auto tree = readCodeTable(fin);

    // Original size
    uint64_t originalSize = readU64(fin);

    // Payload
    std::vector<uint8_t> payload(
        (std::istreambuf_iterator<char>(fin)),
         std::istreambuf_iterator<char>());
    fin.close();

    std::cout << "  Reading   : " << inPath << "\n";
    std::cout << "  Orig name : " << origName << "\n";
    std::cout << "  Payload   : " << fmtSize(payload.size()) << "\n";

    // Decode
    std::vector<uint8_t> data = decode(payload, padding, tree, originalSize);

    // Write output
    std::ofstream fout(outPath, std::ios::binary);
    if (!fout) throw std::runtime_error("Cannot open output: " + outPath);
    fout.write(reinterpret_cast<const char*>(data.data()),
               static_cast<std::streamsize>(data.size()));
    fout.close();

    std::cout << "\n  ✓ Decompressed successfully!\n";
    std::cout << "  Output    : " << outPath << "\n";
    std::cout << "  Recovered : " << fmtSize(data.size())
              << "  (" << data.size() << " bytes)\n";

    return origName;
}

// ─────────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    const std::string usage =
        "Huffman File Compressor v2 — binary-safe\n"
        "Usage:\n"
        "  ./huffman compress   <input.*>    <output.huff>\n"
        "  ./huffman compress   <input.*>    <output.huff> --verify\n"
        "  ./huffman decompress <input.huff> <output.*>\n"
        "  ./huffman info       <input.huff>\n";

    if (argc < 3) { std::cerr << usage; return 1; }

    std::string mode    = argv[1];
    std::string inPath  = (argc >= 3) ? argv[2] : "";
    std::string outPath = (argc >= 4) ? argv[3] : "";

    std::cout << "\n╔══════════════════════════════════╗\n";
    std::cout <<   "║  Huffman Coder v2 — C++          ║\n";
    std::cout <<   "╚══════════════════════════════════╝\n\n";

    try {
        if (mode == "compress") {
            if (argc < 4) { std::cerr << usage; return 1; }
            compressFile(inPath, outPath);

            if (argc == 5 && std::string(argv[4]) == "--verify") {
                std::cout << "\n  Running verification...\n";

                std::ifstream orig(inPath, std::ios::binary);
                std::vector<uint8_t> original(
                    (std::istreambuf_iterator<char>(orig)),
                     std::istreambuf_iterator<char>());

                std::string tmpPath = outPath + ".verify_tmp";
                decompressFile(outPath, tmpPath);

                std::ifstream rec(tmpPath, std::ios::binary);
                std::vector<uint8_t> recovered(
                    (std::istreambuf_iterator<char>(rec)),
                     std::istreambuf_iterator<char>());
                rec.close();
                std::remove(tmpPath.c_str());

                if (original == recovered)
                    std::cout << "  >> Verification PASSED — output is lossless!\n";
                else
                    std::cout << "  >> Verification FAILED — mismatch detected!\n";
            }

        } else if (mode == "decompress") {
            if (argc < 4) { std::cerr << usage; return 1; }
            decompressFile(inPath, outPath);

        } else if (mode == "info") {
            // Print header info without decompressing
            std::ifstream fin(inPath, std::ios::binary);
            if (!fin) throw std::runtime_error("Cannot open: " + inPath);
            char magic[4]; fin.read(magic, 4);
            if (std::strncmp(magic, "HUF2", 4) != 0)
                throw std::runtime_error("Not a valid HUF2 file.");
            uint8_t nameLen = static_cast<uint8_t>(fin.get());
            std::string origName(nameLen, '\0');
            fin.read(origName.data(), nameLen);
            fin.get(); // padding
            uint32_t tableEntries = readU32(fin);
            // skip table entries to get to size field
            for (uint32_t i = 0; i < tableEntries; i++) {
                fin.get();
                uint8_t codeLen = static_cast<uint8_t>(fin.get());
                fin.ignore((codeLen + 7) / 8);
            }
            uint64_t origSize = readU64(fin);
            uint64_t compSize = fs::file_size(inPath);

            std::cout << "  Original file : " << origName << "\n";
            std::cout << "  Original size : " << fmtSize(origSize) << "\n";
            std::cout << "  Compressed    : " << fmtSize(compSize) << "\n";
            std::cout << "  Ratio         : " << std::fixed << std::setprecision(1)
                      << (100.0 * (1.0 - (double)compSize / origSize)) << "% smaller\n";
            std::cout << "  Unique bytes  : " << tableEntries << " / 256\n";

        } else {
            std::cerr << "Unknown mode: " << mode << "\n\n" << usage;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "\n  Error: " << e.what() << "\n";
        return 1;
    }

    std::cout << "\n";
    return 0;
}