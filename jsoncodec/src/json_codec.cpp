#include "json_codec.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <zlib.h>

JsonCodec::JsonCodec(const std::string& uri, const std::string& timestamp, const std::string& jsonBody)
    : uri(uri), timestamp(timestamp), jsonBody(jsonBody) {
    // Load keyDict (assuming it's already available)
    KeyDict["key1"] = 1;
    KeyDict["key2"] = 2;
    outputDir = "/var/jsoncodec/" + uri + "/";
    keyFile = outputDir + "keyfile.json";
    timestampDir = outputDir + timestamp + "/";
    encodedFile = timestampDir + "encoded_data";

}

std::string JsonCodec::encode() {
    // Parse JSON data
    json jsonData = json::parse(jsonBody);

    // Compress JSON data using zlib
    std::ostringstream compressedData;
    compressJsonData(jsonData, compressedData);

    // Create directory structure
    //std::string keyFile = outputDir + "keyfile.json";
    //std::string timestampDir = outputDir + timestamp + "/";
    //std::string encodedFile = timestampDir + "encoded_data";

    // Write compressed data to the file
    writeCompressedData(encodedFile, compressedData.str());

    // Return the encoded file path
    return encodedFile;
}

// void JsonCodec::compressJsonData(const json& jsonData, std::ostream& outputStream) {
//     for (auto it = jsonData.begin(); it != jsonData.end(); ++it) {
//         if (it->is_object()) {
//             // Recursively compress object
//             compressJsonData(*it, outputStream);
//         } else {
//             // Check if key exists in keyDict
//             auto dictIt = KeyDict.find(it.key());

//             if (dictIt == KeyDict.end()) {
//                 KeyDict[it.key()] = KeyDict.size()+1;
//                 dictIt = KeyDict.find(it.key());
//             }

//             if (dictIt != KeyDict.end()) {
//                 // Write key + raw data to the output stream
//                 outputStream << dictIt->second;
//                 outputStream.write(reinterpret_cast<const char*>(&(*it)), sizeof(json::value_type));
//             } else {
//                 // If key doesn't exist in keyDict, write raw data to the output stream
//                 outputStream << it.key();
//                 outputStream.write(reinterpret_cast<const char*>(&(*it)), sizeof(json::value_type));
//             }
//         }
//     }
// }

void JsonCodec::compressJsonData(const json& jsonData, std::ostream& outputStream) {
    for (auto it = jsonData.begin(); it != jsonData.end(); ++it) {
        if (it->is_object()) {
            // Recursively compress object
            compressJsonData(*it, outputStream);
        } else {
            // Check if key exists in keyDict
            auto dictIt = KeyDict.find(it.key());

            if (dictIt == KeyDict.end()) {
                KeyDict[it.key()] = KeyDict.size() + 1;
                dictIt = KeyDict.find(it.key());
            }

            if (dictIt != KeyDict.end()) {
                // Write key, data type, data size, and raw data to the output stream
                std::string key = std::to_string(dictIt->second);
                std::string dataType = it->type_name();
                std::string rawData = it->dump();
                /// Convert numbers and booleans to binary format
                if (it->is_boolean()) {
                    bool value = *it;
                    if (value)
                        outputStream << key << 0x2;
                    else
                        outputStream << key << 0x1;

                    //outputStream << key << "b" << sizeof(bool) << value;
                } else if (it->is_number_integer()) {
                    int value = *it;
                    outputStream << key << "n" << sizeof(int);
                    outputStream.write(reinterpret_cast<const char*>(&value), sizeof(int));
                } else if (it->is_number_float()) {
                    double value = *it;
                    outputStream << key << "f" << sizeof(double);
                    outputStream.write(reinterpret_cast<const char*>(&value), sizeof(double));

                } else if (it->is_string()) {
                    //double value = *it;
                    std::string rawData = it->dump();
                    size_t dataSize = rawData.size();
                    outputStream << key << "s" << dataSize << " " << rawData;

                } else {
                    // For other data types, write raw data to the output stream
                    std::string rawData = it->dump();
                    size_t dataSize = rawData.size();
                    outputStream << key <<  "x" << strlen(it->type_name()) << it->type_name() << dataSize;
                    outputStream.write(rawData.c_str(), dataSize);
                }
            //     std::string dt = "u";
            //     if (dataType == "boolean")
            //             dt = "b";
            //     else if (dataType == "number")
            //             dt = "n";
            //    else if (dataType == "string")
            //              dt = "s";


            //     size_t dataSize = rawData.size();
            //     //outputStream << key.size() << " "<< key <<" "<< dataType.size() << " " << dataType << " " << dataSize;
            //     outputStream << key <<" "<< " " << dt << " " << dataSize;
            //     outputStream.write(rawData.c_str(), dataSize);
            } else {
                // If key doesn't exist in keyDict, write raw data to the output stream
                std::string key = it.key();
                std::string dataType = it->type_name();
                std::string rawData = it->dump();

                size_t dataSize = rawData.size();
                outputStream << key.size() << key << dataType.size() << dataType << dataSize;
                outputStream.write(rawData.c_str(), dataSize);
            }
        }
    }
}
//hexdump -C  /var/jsoncodec/example_uri/2023-08-10T12:34:56/encoded_data
json JsonCodec::uncompressJsonData(std::istream& inputStream) const{
    json jsonData = json::object();
    while (inputStream) {
        // Read key
        char keyc;
        char valueType;
        inputStream >> keyc >> valueType;
        std::cout << "key ["<<keyc<< "] valueType [" << valueType <<"]"<< std::endl;
        std::string key = std::string(1,keyc);

        if (!inputStream) break;

        if (valueType == '1') { // Boolean false
            bool value = false;
            jsonData[key] = value;
        } else if (valueType == '2') { // Boolean true
            bool value = true;
            jsonData[key] = value;
        } else if (valueType == '3') { // integer zero
            int value = 0;
            jsonData[key] = value;
        } else if (valueType == '4') { // double zero
            double value = 0;
            jsonData[key] = value;
        } else if (valueType == 'b') { // Boolean
            char sizec;
            inputStream >> sizec;
            bool value;
            inputStream.read(reinterpret_cast<char*>(&value), sizeof(bool));
            jsonData[key] = value;
        } else if (valueType == 'n') { // Integer

            char sizec;
            inputStream >> sizec;
            std::cout << " sizec :" << sizec << std::endl;
            std::vector<char>valBuf(sizeof(int));
            //int value;
            inputStream.read(valBuf.data(), sizeof(int));
            printf(" valBuf %x . %x . %x . %x \n", valBuf[0],valBuf[1],valBuf[2],valBuf[3]);
            int value = *reinterpret_cast<const int*>(valBuf.data());
            jsonData[key] = value;
            std::cout << "key ["<<key<< "] value [" << value <<"] sizeof int :" << sizeof(int) << std::endl;

        } else if (valueType == 'f') { // Double
            char sizec;
            inputStream >> sizec;
            std::cout << " sizec :" << sizec << std::endl;
            std::vector<char>valBuf(sizeof(double));
            double value;
            inputStream.read(valBuf.data(), sizeof(double));
            //inputStream.read(reinterpret_cast<char*>(&value), sizeof(double));
            value = *reinterpret_cast<const double*>(valBuf.data());
            jsonData[key] = value;
        } else if (valueType == 's') { // String
            size_t dataSize;
            inputStream >> dataSize;
            std::cout << " read data size as :" << dataSize << std::endl;
            //dataSize;
            std::string rawData;
            rawData.resize(dataSize+2); // +1 to accommodate null-terminator
            inputStream.read(&rawData[0], dataSize+1);
            rawData[dataSize+1] = '\0'; // Null-terminate the string
            std::cout << " read data as [" << rawData<< "]" << std::endl;
            jsonData[key] = nlohmann::json::parse(rawData);
        } else { // Other data types
            std::cout << " valueType " << valueType << std::endl;
            size_t typeNameSize, dataSize;
            inputStream >> typeNameSize >> dataSize;
            std::string typeName, rawData;
            typeName.resize(typeNameSize);
            std::cout << " resize " << dataSize << std::endl;
            if (dataSize > 128)
                dataSize = 128;
            rawData.resize(dataSize);
            inputStream.read(&typeName[0], typeNameSize);
            inputStream.read(&rawData[0], dataSize);
            jsonData[key] = nlohmann::json::parse(rawData);
        }
    }
    return jsonData;
}

// json JsonCodec::uncompressJsonData(std::istream& inputStream) const {
//     nlohmann::json jsonData = nlohmann::json::object();
//     std::string key;

//     while (inputStream) {
//         // Read the key ID (represented as an integer)
//         std::getline(inputStream, key, '\0');
//         if (key.empty()) break; // End of data reached

//         // Check if key exists in keyDict
//         //auto dictIt = KeyDict.find(std::stoi(key));
//         //if (dictIt != KeyDict.end()) 
//         if (true)
//         {
//             // Convert the key ID back to the original key
//             //std::string originalKey = dictIt->second;
//             std::string originalKey = "foo";

//             // Read the raw data corresponding to the key
//             json::value_type value;
//             inputStream.read(reinterpret_cast<char*>(&value), sizeof(json::value_type));

//             // Add the key-value pair to the jsonData object
//             jsonData[originalKey] = value;
//         } else {
//             // Key doesn't exist in keyDict, read the raw key
//             std::string rawKey = key;

//             // Read the raw data corresponding to the key
//             json::value_type value;
//             inputStream.read(reinterpret_cast<char*>(&value), sizeof(json::value_type));

//             // Add the raw key and value to the jsonData object
//             jsonData[rawKey] = value;
//         }
//     }

//     return jsonData;
// }
// std::istream& JsonCodec::readFileToStream(const std::string& filePath, std::istream& inputStream) {
//     // std::ifstream fileStream(filePath, std::ios::binary);
//     // if (!fileStream) {
//     //     throw std::runtime_error("Failed to open file: " + filePath);
//     // }

//     // // Read the contents of the file into the input stream
//     // inputStream << fileStream.rdbuf();

//     return inputStream;
// }
void JsonCodec::writeCompressedData(const std::string& filename, const std::string& data) {
    std::ofstream outFile(filename, std::ios::binary);
    if (outFile) {
        outFile.write(data.c_str(), data.size());
        outFile.close();
    } else {
        std::cerr << "Error writing to file: " << filename << std::endl;
    }
}

int JsonCodec::loadKeyFile(const std::string& keyfile) {
    std::ifstream inputFile(keyfile);
    if (!inputFile) {
        std::cerr << "Failed to open keyfile: " << keyfile << std::endl;
        return -1;
    }

    json jsonData;
    inputFile >> jsonData;

    // Parse the JSON data and populate the KeyDict
    KeyDict = jsonData.get<std::unordered_map<std::string, int>>();
    return 0;
}


// int JsonCodec::loadKeyFile(const std::string& keyFilePath) {
//     std::ifstream file(keyFilePath);
//     if (file.is_open()) {
//         //file >> KeyDict;
//         file.close();
//         return 0;
//     } else {
//         std::cerr << "Failed to open key file: " << keyFilePath << std::endl;
//         return -1;
//     }
// }

int  JsonCodec::saveKeyFile(const std::string& keyFilePath) {
    std::ofstream file(keyFilePath);
    if (file.is_open()) {
        for ( auto it : KeyDict)
        {
            file <<it.first << ":"<< it.second << std::endl;
        }
        file.close();
        return 0;
    } else {
        std::cerr << "Failed to open key file: " << keyFilePath << std::endl;
        return -1;
    }
}

int  JsonCodec::savejKeyFile(const std::string& keyFilePath) {
// Open the file for writing
    std::ofstream outputFile(keyFilePath);

    if (!outputFile) {
        std::cerr << "Error opening the file for writing." << std::endl;
        return -1;
    }

    // Write the jsonRoot to the file
    outputFile << jKeyDict.dump(4); // The number 4 specifies the indentation level (optional)

    // Close the file
    outputFile.close();

    return 0;
}
// json JsonCodec::extractJsonData(const std::string& encodedData) const {
//     // Load the keyfile and create the keyDict
//     // KeyDict keyDict;
//     // loadKeyFile(keyfile, keyDict);

//     json jsonData = nlohmann::json::object();

//     // Start decoding the encodedData
//     std::istringstream dataStream(encodedData);
//     std::string key;
//     while (dataStream) {
//         // Read the key ID (represented as an integer)
//         std::getline(dataStream, key, '\0');
//         if (key.empty()) break; // End of data reached

//         // Convert the key ID back to the original key
//         std::string originalKey = "foo" ; //keyDict.getKey(std::stoi(key));

//         // Read the raw data corresponding to the key
//         std::string rawData;
//         std::getline(dataStream, rawData, '\0');

//         // Decode the raw data into the original JSON value
//         json jsonValue;
//         decodeData(rawData, jsonValue);

//         // Add the key-value pair to the jsonData object
//         jsonData[originalKey] = jsonValue;
//     }

//     rjeturn jsonData;
// }
//#include <fstream>

void JsonCodec::populateReverseKeyDict() {
    for (auto it = jKeyDict.begin(); it != jKeyDict.end(); ++it) {
        ReverseKeyDict[it.value()] = it.key();
    }
}

void JsonCodec::readKeyDictFromFile(const std::string& keyDictFilePath) {
    std::ifstream keyDictFile(keyDictFilePath);
    keyDictFile >> jKeyDict;
    keyDictFile.close();

    // Populate the reverse lookup
    populateReverseKeyDict();
}

int JsonCodec::getIndex(std::string& key) {
    auto it = jKeyDict.find(key);
    if (it != jKeyDict.end()) {
        return it.value();
    } else {
        int idx = jKeyDict.size();
        idx++;
        jKeyDict[key] = idx;
        ReverseKeyDict[idx] = key;

        // Return a default value (you can choose -1 or throw an exception, depending on your use case)
        return idx;
    }
}

std::string JsonCodec::getName(int index) const {
    auto it = ReverseKeyDict.find(index);
    if (it != ReverseKeyDict.end()) {
        return it->second;
    } else {
        // Return an empty string (you can choose to throw an exception, depending on your use case)
        return "";
    }
}
