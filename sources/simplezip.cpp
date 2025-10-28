
#include "simplezip.h"

#include <algorithm>
#include <iostream>
#include <queue>

#define READ_BUFF_SIZE 4096
#define WRITE_BUFF_SIZE 4096

class BitStream {
    char numBits = 0;
    char bits = 0;
    int numBytes = 0;
    char bytes[WRITE_BUFF_SIZE];
    int readSize = 0;
    std::ofstream* fout = nullptr;
    std::ifstream* fin = nullptr;
    bool isEof = false;
    bool isEndBitStream = false;
    std::streampos eofPos = 0;
    char numPad = 0;

   public:
    void setNumPad(char pad) { numPad = pad; }
    void setFileOut(std::ofstream* ofout) { fout = ofout; }
    void setFileIn(std::ifstream* ofin) { 
        fin = ofin; 
        std::streampos curPos = fin->tellg();
        fin->seekg(0, std::ios::end);
        eofPos = fin->tellg();
        fin->clear();
        fin->seekg(curPos);
    }

    void write(char bit) {
        bit &= 1;
        bits |= (bit << (7 - numBits));

        numBits++;
        if (numBits > 7) {
            numBits = 0;
            bytes[numBytes] = bits;
            bits = 0;

            numBytes++;
            if (numBytes > WRITE_BUFF_SIZE - 1) {
                numBytes = 0;
                fout->write(bytes, WRITE_BUFF_SIZE);
            }
        }
    }

    char read() {
        if (numBytes > readSize - 1) {
            fin->read(bytes, READ_BUFF_SIZE);
            readSize = fin->gcount();
            numBytes = 0;
            
            if (eofPos == fin->tellg() || fin->eof()) {
                isEof = true;
            }
        }

        char bits = bytes[numBytes];
        char bit = (bits >> (7 - numBits)) & 1;

        numBits++;
        if (numBits > 7) {
            numBits = 0;
            numBytes++;
        }

        return bit;
    }

    bool isEnd() { 
        return isEof && (numBytes >= readSize - 1) && (numBits >= 8 - numPad); 
    }

    char flush() {
        if (numBytes) {
            fout->write(bytes, numBytes);
        }

        char numPad = 0;
        if (numBits) {
            numPad = 8 - numBits;
            fout->write(&bits, 1);
        }
        fout->flush();

        return numPad;
    }
};

int SimpleZip::zip() {
    analyze();
    buildTree();
    writeFreqMap();
    buildLookupTable(_tree, "");
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
    if(_freq.empty()) {
        return;
    }

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

void SimpleZip::buildLookupTable(const HuffmanNodePtr& tree, std::string initPath) {
    if(!tree) {
        return;
    }
    
    if (!tree->l && !tree->r) {
        _lookupTable.insert({tree->val, initPath});
        return;
    }

    if (tree->l) {
        buildLookupTable(tree->l, initPath + '0');
    }

    if (tree->r) {
        buildLookupTable(tree->r, initPath + "1");
    }
}

void SimpleZip::writeFreqMap() {
    _fout.open(fileOut, std::ios::binary);

    if (!_fout.good()) {
        _lastError = OPEN_OUT_FILE_ERROR;
        return;
    }

    _fout.write((char*)&_numPad, 1);

    size_t mapSz = _freq.size();
    _fout.write((char*)&mapSz, sizeof(mapSz));

    int headerSize = 8;
    for (auto&& it : _freq) {
        _fout.write((char*)&it.first, 1);
        _fout.write((char*)&it.second, sizeof(size_t));

        headerSize = headerSize + 1 + sizeof(size_t);
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
        // findValueOnTree(val, _tree, path);

        if(_lookupTable.find(val) != _lookupTable.end()) {
            path = _lookupTable.at(val);
        }

        for (auto&& it : path) {
            if (it == '0') {
                bitstream.write(0);
            } else {
                bitstream.write(1);
            }
        }
    }
    _numPad = bitstream.flush();

    _fout.seekp(0);
    _fout.write((char*)&_numPad, 1);

    _fout.close();
}

void SimpleZip::decode() {
    _fout.open(fileOut, std::ios::binary);

    if (!_fout.good()) {
        _lastError = OPEN_OUT_FILE_ERROR;
        return;
    }

    if(!_tree) {
        _fout.close();
        return;
    }

    BitStream bitstream;
    bitstream.setFileIn(&_fin);
    bitstream.setNumPad(_numPad);

    auto node = _tree;
    while (!bitstream.isEnd()) { 
        if (!node->l && !node->r) {
            _fout.write(&node->val, 1);

            if(node != _tree) {
                node = _tree;
                continue;
            }
        }

        char bit = bitstream.read();
        if (bit) {
            node = node->r;
        } else {
            node = node->l;
        }

        if(!node) {
            break;
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

    _fin.read((char*)&_numPad, 1);

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
