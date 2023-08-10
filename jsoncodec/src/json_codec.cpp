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
        std::string key;
        std::string dataType;
        auto dictIt = KeyDict.find(it.key());

        if (dictIt == KeyDict.end()) {
            KeyDict[it.key()] = KeyDict.size() + 1;
            dictIt = KeyDict.find(it.key());
        }

        if (dictIt != KeyDict.end()) {
                // Write key, data type, data size, and raw data to the output stream
            //key = std::to_string(dictIt->second);
            key = dictIt->second;
            dataType = it->type_name();
        }
        if (it->is_object()) {

            if (dictIt != KeyDict.end()) {
                // Write key, data type, data size, and raw data to the output stream
                /// Convert numbers and booleans to binary format
                // Recursively compress object
                outputStream << std::hex << key << "o";
                compressJsonData(*it, outputStream);
            }
        } else if (it->is_array()) {
            if (dictIt != KeyDict.end()) {
                // Write key, data type, data size, and raw data to the output stream
                /// Convert numbers and booleans to binary format
                // Recursively compress object
                outputStream << std::hex << key << "a";
                compressJsonData(*it, outputStream);
    
            // Write array marker to the output stream
            //   outputStream << "a";

            // Write the size of the array to the output stream
                size_t st = it->size();
                outputStream.write(reinterpret_cast<const char*>(&st), sizeof(size_t));

            // Recursively compress each element of the array
                for (const auto& arrayElem : *it) {
                    compressJsonData(arrayElem, outputStream);
                }
        }
            // Recursively compress object
            //compressJsonData(*it, outputStream);
        } else if (it->is_boolean()) {
            bool value = *it;
            if (value)
                outputStream << std::hex << key << "T";
            else
                outputStream << key << "F";

                    //outputStream << std::hex << key << "b" << sizeof(bool) << value;
        } else if (it->is_number_integer()) {
            int value = *it;
            char valc = 'p';
            if (value < 0)
            {
                valc = 'n';
                value = - value;
            }
            int siz=sizeof(int);
            if (value == 0 )
                siz = 0;
            else if (value >=0 && value < 1<<8)
                siz = 1;
            else if (value >= 0 && value < 1<<16)
                siz = 2;

            //outputStream << std::hex << key << "n" << sizeof(int);
            outputStream << std::hex << key << valc << siz;
            outputStream.write(reinterpret_cast<const char*>(&value), siz);
        } else if (it->is_number_float()) {
            double value = *it;
            outputStream << std::hex << key << "f";
            outputStream.write(reinterpret_cast<const char*>(&value), sizeof(double));

        } else if (it->is_string()) {
            //double value = *it;
            std::string rawData = it->dump();
            int dataSize = (int)rawData.size();
            printf (" Rawdatasize   %d \n", dataSize);
            std::cout << " Rawdatasize :" << dataSize << std::endl;

            outputStream << std::hex << key << "s";
            outputStream.write(reinterpret_cast<const char*>(&dataSize), 1);
            outputStream << rawData;

        } else {
            // For other data types, write raw data to the output stream
            std::string rawData = it->dump();
            size_t dataSize = rawData.size();
            outputStream << std::hex << key <<  "x" << strlen(it->type_name()) << it->type_name() << dataSize;
            outputStream.write(rawData.c_str(), dataSize);
        }
        //     } else {
        //         // If key doesn't exist in keyDict, write raw data to the output stream
        //         std::string key = it.key();
        //         std::string dataType = it->type_name();
        //         std::string rawData = it->dump();

        //         size_t dataSize = rawData.size();
        //         outputStream << key.size() << key << dataType.size() << dataType << dataSize;
        //         outputStream.write(rawData.c_str(), dataSize);
        //     }
        // }
    }
}
//hexdump -C  /var/jsoncodec/example_uri/2023-08-10T12:34:56/encoded_dakeyc +ta
json JsonCodec::uncompressJsonData(std::istream& inputStream) const{
    json jsonData = json::object();
    while (inputStream) {
        // Read key
        char keyc;

        char valueType;
        inputStream >> keyc >> valueType;
        printf(" Hex Key %x \n", keyc);
        std::string key = std::string(1,keyc+0x30);
        std::cout << "key ["<<(key)<< "] valueType [" << valueType <<"]"<< std::endl;

        if (!inputStream) break;
        if (valueType == 'a') { // Array marker
            size_t arraySize;
            inputStream.read(reinterpret_cast<char*>(&arraySize), sizeof(size_t));

            json jsonArray = json::array();
            for (size_t i = 0; i < arraySize; ++i) {
                jsonArray.push_back(uncompressJsonData(inputStream));
            }
            return jsonArray;
        } else if (valueType == 'o') { // An object
            jsonData[key] = uncompressJsonData(inputStream);
        } else if (valueType == 'F') { // Boolean false
            bool value = false;
            jsonData[key] = value;
        } else if (valueType == 'T') { // Boolean true
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
        } else if ((valueType == 'p') || (valueType == 'n') ){ // Integer p = positive

            char sizec;
            inputStream >> sizec;
            std::cout << " sizec :" << sizec << std::endl;
            std::vector<char>valBuf(sizeof(int));
            valBuf[0] = 0;
            valBuf[1] = 0;
            valBuf[2] = 0;
            valBuf[3] = 0;
            int value = 0;
            if (sizec > 0x30) {
                inputStream.read(valBuf.data(), sizec - 0x30);
                //inputStream.read(valBuf.data(), sizeof(int));
                printf(" valBuf %x . %x . %x . %x \n", valBuf[0],valBuf[1],valBuf[2],valBuf[3]);
                value = *reinterpret_cast<const int*>(valBuf.data());
                if (valueType == 'n')
                    value = - value;
            }
            jsonData[key] = value;
            std::cout << "key ["<<key<< "] value [" << value <<"] sizeof int :" << sizeof(int) << std::endl;

        } else if (valueType == 'f') { // Double
            // char sizec;
            // inputStream >> sizec;
            // std::cout << " sizec :" << sizec << std::endl;
            std::vector<char>valBuf(sizeof(double));
            double value;
            inputStream.read(valBuf.data(), sizeof(double));
            //inputStream.read(reinterpret_cast<char*>(&value), sizeof(double));
            value = *reinterpret_cast<const double*>(valBuf.data());
            jsonData[key] = value;
        } else if (valueType == 's') { // String
            char dataSize[2];
            inputStream.read(&dataSize[0], 1);
            std::cout << " read data size as :" << dataSize[0] << std::endl;
            printf("   datasize : %d\n", (int)dataSize[0]);
            //dataSize;
            std::string rawData;
            rawData.resize(dataSize[0]+1); // +1 to accommodate null-terminator
            inputStream.read(&rawData[0], dataSize[0]);
            rawData[dataSize[0]+1] = '\0'; // Null-terminate the string
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
json JsonCodec::xxuncompressJsonData(std::istream& inputStream) const {
    json jsonData = json::object();
    while (inputStream) {
        // Read key and value type
        char keyc, valueType;
        inputStream >> keyc >> valueType;

        // Convert keyc to string
        std::string key(1, keyc);

        if (!inputStream) break;
        if (valueType == 'a') { // Array marker
            // ...
        } else if (valueType == 'o') { // An object
            jsonData[key] = uncompressJsonData(inputStream);
        } else if (valueType == 'F') { // Boolean false
            jsonData[key] = false;
        } else if (valueType == 'T') { // Boolean true
            jsonData[key] = true;
        } else if (valueType == '3') { // integer zero
            jsonData[key] = 0;
        } else if (valueType == '4') { // double zero
            jsonData[key] = 0.0;
        } else if (valueType == 'b') { // Boolean
            bool value;
            inputStream.read(reinterpret_cast<char*>(&value), sizeof(bool));
            jsonData[key] = value;
        } else if ((valueType == 'p') || (valueType == 'n')) { // Integer
            int value;
            inputStream.read(reinterpret_cast<char*>(&value), sizeof(int));
            jsonData[key] = value;
        } else if (valueType == 'f') { // Double
            double value;
            inputStream.read(reinterpret_cast<char*>(&value), sizeof(double));
            jsonData[key] = value;
        } else if (valueType == 's') { // String
            size_t dataSize;
            inputStream >> dataSize;
            std::string rawData(dataSize, '\0');
            inputStream.read(&rawData[0], dataSize);
            jsonData[key] = nlohmann::json::parse(rawData);
        } else { // Other data types
            size_t typeNameSize, dataSize;
            inputStream >> typeNameSize >> dataSize;
            std::string typeName(typeNameSize, '\0'), rawData(dataSize, '\0');
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
