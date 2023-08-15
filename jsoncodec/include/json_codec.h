#ifndef JSON_CODEC_H
#define JSON_CODEC_H

#include <string>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <fstream>


using json = nlohmann::json;

class JsonCodec {
private:
    std::string uri;
    std::string timestamp;
    std::string jsonBody;
    std::string encodedFile;
    std::string timestampDir;
    std::string keyFile;
    std::string outputDir;

    std::unordered_map<std::string, int> KeyDict;
    json jKeyDict;
    std::unordered_map<int, std::string> ReverseKeyDict;
    // Helper function to populate the reverse lookup
    void populateReverseKeyDict();
;


public:
    JsonCodec(const std::string& uri, const std::string& timestamp, const std::string& jsonBody);
    std::string encode();
    std::unordered_map<std::string, int>* getKeyDict()
    {
        return &KeyDict;
    }
    int loadKeyFile(const std::string& keyFilePath);
    int saveKeyFile(const std::string& keyFilePath);
    json extractJsonData(const std::string& encodedData) const;
    json uncompressJsonData(std::istream& inputStream);
    json xxuncompressJsonData(std::istream& inputStream);
//    std::istream& readFileToStream(const std::string& filePath, std::istream& inputStream);

    // Function to get the index from a key
    int getIndex(std::string& key);

    // Function to get the key name from an index
    std::string getName(int index) const;

    // Function to read KeyDict from a JSON file
    void readKeyDictFromFile(const std::string& keyDictFilePath);
    int  savejKeyFile(const std::string& keyFilePath);
    int setKeyDict(const std::string& key);
    void writeKey(int key, std::ostream& outputStream);
    void writeKeyVal(int key, char val, std::ostream& outputStream);
    int getKeyVal(char &keyc, char &valueType, std::istream& inputStream);
    int getKey(std::istream& inputStream);
    int writeKeyV(int key, int val, int idx, std::vector<char>&keyv);
    void outKeyV(std::vector<char>&keyv, std::ostream& outputStream);




private:
    void compressJsonData(const json& jsonData, std::ostream& outputStream);
    void writeCompressedData(const std::string& filename, const std::string& data);
};

#endif // JSON_CODEC_H
