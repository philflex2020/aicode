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

    //hexdump -C  /var/jsoncodec/example_uri/2023-08-10T12:34:56/encoded_dakeyc +ta

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
    std::ofstream ostr("ostr.json");

    keyv.reserve(5);
    printf( "\nwrite  127\n");
    idx  = jsonCodec.writeKeyV(127, 128, 3, keyv);
    jsonCodec.outKeyV(keyv, ostr);
    keyv.clear();
    printf( "\nwrite  128\n");
    idx  = jsonCodec.writeKeyV(128, 128, 3, keyv);
    jsonCodec.outKeyV(keyv, ostr);
    keyv.clear();
    printf( "\nwrite  4043\n");
    idx  = jsonCodec.writeKeyV(4043, 128, 3, keyv);
    jsonCodec.outKeyV(keyv, ostr);
    printf( "\nwrite  4043000\n");
    idx  = jsonCodec.writeKeyV(4043000, 128, 6, keyv);
    jsonCodec.outKeyV(keyv, ostr);
    ostr.close();
    std::ifstream istr("ostr.json", std::ios::binary);

    printf( "\n");
    bool done = false;
    while (!done)
    {
        int x1 = jsonCodec.getKey(istr);
        printf( "x1  %d\n", x1);
        if (x1 < 0) done = true;

    }
    // x1 = jsonCodec.getKey(istr);
    // printf( "\n x1  %d\n", x1);
    // x1 = jsonCodec.getKey(istr);
    // printf( "\n x1  %d\n", x1);
    // x1 = jsonCodec.getKey(istr);
    // printf( "\n x1  %d\n", x1);
 
    return 0;
}