#include "json_codec.h"
#include <iostream>
#include <fstream>

int main() {
    // Sample JSON data
    std::string uri = "example_uri";
    std::string timestamp = "2023-08-10T12:34:56";
    std::string jsonBody = R"(
        {
            "key1": "value1",
            "key2": -42,
            "key3": true,
            "key4": {
                "nested_key1": "nested_value1",
                "nested_key2": 3.14
            }
        }
    )";
    //std::unordered_map<std::string, int> keyDict;
    // Create a JsonCodec object
    JsonCodec jsonCodec(uri, timestamp, jsonBody);

    // Encode the JSON data and get the encoded file path
    std::string encodedFilePath = jsonCodec.encode();

    // Print the encoded file path
    std::cout << "Encoded file path: " << encodedFilePath << std::endl;

    std::ifstream inputFileStream(encodedFilePath, std::ios::binary);
    //JsonCodec.readFileToStream(encodedFilePath, inputFileStream);
    if(1)
    {
        json decodedData = jsonCodec.uncompressJsonData(inputFileStream);

    /// Print the decoded JSON data
        std::cout << "Decoded JSON data:" << std::endl;
        std::cout << decodedData.dump(4) << std::endl;
    }

    std::unordered_map<std::string, int> * keyDict = jsonCodec.getKeyDict();
    json jsonRoot;

    for ( auto entry : *keyDict)
    {
        const std::string& key = entry.first;
        //const std::string& rawValue = entry.second;
        jsonRoot[key] = entry.second;

        std::cout <<"Key [" << entry.first << "] => "<< entry.second << std::endl;
    }

    // Open the file for writing
    std::ofstream outputFile("output.json");

    if (!outputFile) {
        std::cerr << "Error opening the file for writing." << std::endl;
        return 1;
    }

    // Write the jsonRoot to the file
    outputFile << jsonRoot.dump(4); // The number 4 specifies the indentation level (optional)

    // Close the file
    outputFile.close();

    jsonCodec.readKeyDictFromFile("output.json");
    std::string skey = "nested_key2";
    int idx = jsonCodec.getIndex(skey);
    std::cout << " nested_key2 idx = "<< idx << std::endl;
    skey = "nested_key4";
    idx = jsonCodec.getIndex(skey);
    std::cout << " nested_key4 idx = "<< idx << std::endl;
    
    skey = jsonCodec.getName(4);
    std::cout << " skey [4] = "<< skey << std::endl;

    jsonCodec.savejKeyFile("output2.json");
    std::vector<char>keyv;
    keyv.reserve(5);
    printf( "write  127\n");
    idx  = jsonCodec.writeKeyV(127, 128, 3, keyv);
    for ( int xx = 0 ; xx < (int)keyv.size(); xx++)
    {
        printf( "[%d] %x ", xx, (unsigned int)keyv.at(xx));
    }
    keyv.clear();
    printf( " idx %d \n", idx);
    printf( "write  128\n");
    idx  = jsonCodec.writeKeyV(128, 128, 3, keyv);
    for ( int xx = 0 ; xx < (int)keyv.size(); xx++)
    {
        printf( "[%d] %x ", xx, (unsigned int)keyv.at(xx));
    }
    keyv.clear();
    printf( " idx %d \n", idx);
    printf( "write  4043\n");
    idx  = jsonCodec.writeKeyV(4043, 128, 3, keyv);
    for ( int xx = 0 ; xx < (int)keyv.size(); xx++)
    {
        printf( "[%d] %x ", (int)(xx&0x7f), (unsigned int)(keyv.at(xx)& 0x7f));
    }
    printf( " idx %d \n", idx);

    return 0;
}