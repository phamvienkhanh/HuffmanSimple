#include <iostream>

#include "simplezip.h"

int main(int argc, char const *argv[]) {
    SimpleZip zip;
    zip.fileIn = "../rfc2616.txt";
    zip.fileOut = "../rfc2616.txt.z";
    zip.zip();
    // zip.dumpFreq();
    // zip.dumpTree();

    SimpleZip uzip;
    uzip.fileOut = "../rfc2616.txt.u";
    uzip.fileIn = "../rfc2616.txt.z";
    uzip.uzip();
    // uzip.dumpFreq();
    // uzip.dumpTree();

    return 0;
}
