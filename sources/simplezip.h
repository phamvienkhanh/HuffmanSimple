#ifndef _SIMPLEZIP_H_
#define _SIMPLEZIP_H_

#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

class SimpleZip {
   public:
    enum ErrorCode { NONE, OPEN_FILE_ERROR, OPEN_OUT_FILE_ERROR };

    struct HuffmanNode;
    using HuffmanNodePtr = std::shared_ptr<HuffmanNode>;
    struct HuffmanNode {
        char val = 0;
        size_t w = 0;
        HuffmanNodePtr l = nullptr;
        HuffmanNodePtr r = nullptr;
    };

   public:
    std::string fileIn;
    std::string fileOut;

    int zip();
    int uzip();

    SimpleZip(/* args */) = default;
    ~SimpleZip() = default;

    void dumpFreq();
    void dumpTree();

   private:
    void analyze();
    void buildTree();
    void writeFreqMap();
    void readFreqMap();
    void encode();
    void decode();

    void dumpTree(std::string ident, const HuffmanNodePtr& tree);
    void findValueOnTree(char val, const HuffmanNodePtr& tree,
                         std::string& outpath, std::string initPath = "");
    void buildLookupTable(const HuffmanNodePtr& tree, std::string initPath);

   private:
    std::ofstream _fout;
    std::ifstream _fin;
    int _lastError = 0;
    char _numPad = 0;
    std::map<char, size_t> _freq;
    std::map<char, std::string> _lookupTable;
    HuffmanNodePtr _tree = nullptr;
};

#endif