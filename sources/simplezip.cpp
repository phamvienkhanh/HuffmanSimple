
#include "simplezip.h"

#include <algorithm>
#include <iostream>
#include <queue>

#define READ_BUFF_SIZE 4096
#define WRITE_BUFF_SIZE 4096

class BitStream {
    char bitIdx = 0;
    char bits = 0;
    int byteIdx = 0;
    char bytes[WRITE_BUFF_SIZE];
    int readSize = 0;
    std::ofstream* fout = nullptr;
    std::ifstream* fin = nullptr;
    bool isEof = false;

   public:
    void setFileOut(std::ofstream* ofout) { fout = ofout; }
    void setFileIn(std::ifstream* ofin) { fin = ofin; }

    void write(char bit) {
        bit &= 1;
        bits |= (bit << (8 - bitIdx - 1));

        bitIdx++;
        if (bitIdx > 7) {
            bitIdx = 0;
            bytes[byteIdx] = bits;
            bits = 0;

            byteIdx++;
            if (byteIdx > WRITE_BUFF_SIZE - 1) {
                byteIdx = 0;
                fout->write(bytes, WRITE_BUFF_SIZE);
            }
        }
    }

    char read() {
        if (byteIdx > readSize - 1) {
            bool isg = fin->good();
            fin->read(bytes, READ_BUFF_SIZE);
            readSize = fin->gcount();
            std::streamsize bytes = fin->gcount();
            byteIdx = 0;

            if (!readSize) {
                isEof = true;
            }
        }

        char bits = bytes[byteIdx];
        char bit = (bits >> (8 - bitIdx - 1)) & 1;

        bitIdx++;
        if (bitIdx > 7) {
            bitIdx = 0;

            byteIdx++;
        }

        return bit;
    }

    bool isEnd() { return isEof; }

    void flush() {
        if (byteIdx) {
            fout->write(bytes, byteIdx);
        }

        if (bitIdx) {
            fout->write(&bits, 1);
        }
        fout->flush();
    }
};

int SimpleZip::zip() {
    analyze();
    buildTree();
    writeFreqMap();
    encode();

    return 0;
}

int SimpleZip::uzip() {
    readFreqMap();
    buildTree();
    decode();

    return 0;
}

void SimpleZip::dumpFreq() {
    for (auto it = _freq.begin(); it != _freq.end(); it++) {
        std::cout << it->first << " : " << it->second << "\n";
    }
}

void SimpleZip::dumpTree() { dumpTree("", _tree); }

void SimpleZip::analyze() {
    std::ifstream fin;
    fin.open(fileIn, std::ios::binary);
    if (!fin.good()) {
        _lastError = OPEN_FILE_ERROR;
        return;
    }

    size_t fileSize = 0;
    char chunked[READ_BUFF_SIZE];
    while (!fin.eof()) {
        fin.read(chunked, READ_BUFF_SIZE);
        auto numRead = fin.gcount();
        fileSize += numRead;

        for (size_t i = 0; i < numRead; i++) {
            if (_freq.find(chunked[i]) == _freq.end()) {
                _freq[chunked[i]] = 1;
            } else {
                _freq[chunked[i]] += 1;
            }
        }
    }
}

void SimpleZip::buildTree() {
    auto compare = [](const HuffmanNodePtr& l, const HuffmanNodePtr& r) {
        return l->w > r->w;
    };
    std::priority_queue<HuffmanNodePtr, std::vector<HuffmanNodePtr>,
                        decltype(compare)>
        nodes(compare);

    for (const auto& it : _freq) {
        auto node = std::make_shared<HuffmanNode>();
        node->w = it.second;
        node->val = it.first;
        nodes.push(node);
    }

    while (nodes.size() != 1) {
        auto n1 = nodes.top();
        nodes.pop();

        auto n2 = nodes.top();
        nodes.pop();

        auto newNode = std::make_shared<HuffmanNode>();
        newNode->w = n1->w + n2->w;
        newNode->l = n1;
        newNode->r = n2;

        nodes.push(newNode);
    }

    _tree = nodes.top();
    nodes.pop();
}

void SimpleZip::findValueOnTree(char val, const HuffmanNodePtr& tree,
                                std::string& outpath, std::string initPath) {
    if (!tree->l && !tree->r && tree->val == val) {
        outpath = initPath;
        return;
    }

    if (tree->l) {
        findValueOnTree(val, tree->l, outpath, initPath + '0');
    }

    if (tree->r) {
        findValueOnTree(val, tree->r, outpath, initPath + "1");
    }
}

void SimpleZip::writeFreqMap() {
    _fout.open(fileOut, std::ios::binary);

    if (!_fout.good()) {
        _lastError = OPEN_OUT_FILE_ERROR;
        return;
    }

    size_t mapSz = _freq.size();
    _fout.write((char*)&mapSz, sizeof(mapSz));

    int headerSize = 8;
    for (auto&& it : _freq) {
        _fout.write((char*)&it.first, 1);
        _fout.write((char*)&it.second, sizeof(size_t));

        headerSize = headerSize + 1 + 8;
    }
}

void SimpleZip::encode() {
    std::ifstream fin;
    fin.open(fileIn, std::ios::binary);

    if (!fin.good()) {
        _lastError = OPEN_FILE_ERROR;
        return;
    }

    BitStream bitstream;
    bitstream.setFileOut(&_fout);
    while (!fin.eof()) {
        char val;
        fin.read(&val, 1);
        std::string path;
        findValueOnTree(val, _tree, path);

        for (auto&& it : path) {
            if (it == '0') {
                bitstream.write(0);
            } else {
                bitstream.write(1);
            }
        }
    }
    bitstream.flush();

    _fout.close();
}

void SimpleZip::decode() {
    _fout.open(fileOut, std::ios::binary);

    if (!_fout.good()) {
        _lastError = OPEN_OUT_FILE_ERROR;
        return;
    }

    BitStream bitstream;
    bitstream.setFileIn(&_fin);

    auto node = _tree;
    while (!bitstream.isEnd()) {
        if (!node->l && !node->r) {
            _fout.write(&node->val, 1);
            node = _tree;
            continue;
        }

        char bit = bitstream.read();
        if (bit) {
            node = node->r;
        } else {
            node = node->l;
        }
    }

    _fout.close();
}

void SimpleZip::dumpTree(std::string ident, const HuffmanNodePtr& tree) {
    if (tree->val && !tree->l && !tree->r) {
        std::cout << "\n";
        std::cout << ident;
        std::cout << tree->val;
    }

    if (tree->l) {
        dumpTree(ident + "0", tree->l);
    }

    if (tree->r) {
        dumpTree(ident + "1", tree->r);
    }
}

void SimpleZip::readFreqMap() {
    _fin.open(fileIn, std::ios::binary);

    if (!_fin.good()) {
        _lastError = OPEN_FILE_ERROR;
        return;
    }

    size_t mapSize = 0;
    _fin.read((char*)&mapSize, sizeof(mapSize));

    for (size_t i = 0; i < mapSize; i++) {
        char val = 0;
        size_t freq = 0;
        _fin.read(&val, 1);
        _fin.read((char*)&freq, sizeof(freq));

        _freq.insert({val, freq});
    }
}
