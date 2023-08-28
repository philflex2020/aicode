
#include "asset.h"
#include "assetVar.h"
#include "dataMaps.h"



// include simulink types
#include "../SL_files/Reference.h"
#include "../SL_files/rtwtypes.h"

extern "C++" {
    int runDataMaps(varsmap &vmap, varmap &amap, const char* aname, fims* p_fims, assetVar* aV);
    int demoThreads(varsmap &vmap, varmap &amap, const char* aname, fims* p_fims, assetVar* aV);

    // int demoExtCode(varsmap &vmap, varmap &amap, const char* aname, fims* p_fims, assetVar* aV);
    // int demoExtCodeSetup(varsmap &vmap, varmap &amap, const char* aname, fims* p_fims, assetVar* aV);
};

extern "C++"{

    int testSigFcn(ess_thread_inst *ess_thread, int sig);
    int testcompFunc(varsmap &vmap, varmap &amap, const char *aname, fims *p_fims, assetVar *aV);

};

int testSigFcn(ess_thread_inst *ess_thread, int sig)
{
    FPS_PRINT_INFO("running {} on  [{}] aname [{}]", __func__, ess_thread->name, sig);

    return 0;
}

int runSigFcn(ess_thread_inst *ess_thread, int sig);

struct DataMap;
std::unordered_map<std::string, DataMap*> dataMaps;     // a map of all dataMaps and their names. used to access dataMaps outside of their creation function


void DataMap::addDataItem(char *name, int offset, char *type, int size)
{
    auto dataItem = new DataItem;
    dataItem->name = name;
    dataItem->offset = offset;
    dataItem->type = type;
    dataItem->size = size;
    dataItems[std::string(name)] = dataItem;
}

void DataMap::addTransferItem(std::string bname, std::string amap, std::string dmap)
{
    std::pair<std::string, std::string> item = std::make_pair(amap, dmap);
    transferBlocks[bname].push_back(item);
}

void DataMap::showTransferItems(std::string bname)
{
    if (transferBlocks.find(bname) != transferBlocks.end())
    {
        std::cout << " Transfer Block name [" << bname << "] " << std::endl;
        std::cout << "  [amap name]  <=> [dmap name]" << std::endl;
        std::cout << "______________________________" << std::endl;
        for (auto xx : transferBlocks[bname])
        {
            std::cout << "  [" << xx.first << "]  <=> [" << xx.second << "]" << std::endl;
        }
        std::cout << std::endl;
    }
}

void DataMap::getFromAmap(std::string bname, asset_manager* am, DataMap *dataMap, uint8_t* dataArea)
{
    if (transferBlocks.find(bname) != transferBlocks.end())
    {
        std::cout << " Get values from Amap using Transfer Block name [" << bname << "] " << std::endl;
        for (auto xx : transferBlocks[bname])
        {
            setDataItem(am, dataMap, xx.first, xx.second, dataArea);
            // FPS_PRINT_INFO("    got dataMap ({}) val ({}): {} from amap field [{}]", dataMap->name, xx.second, *(double *)(&dataArea[dataMap->dataItems[xx.second]->offset]), xx.first );
        }
    }
}

void DataMap::sendToAmap(std::string bname, asset_manager* am, DataMap *dataMap, uint8_t* dataArea)
{
    if (transferBlocks.find(bname) != transferBlocks.end())
    {
        std::cout << " Send values to Amap using Transfer Block name [" << bname << "] " << std::endl;
        for (auto xx : transferBlocks[bname])
        {
            // FPS_PRINT_INFO("    sending dataMap ({}) val ({}): {} to amap field [{}]", dataMap->name, xx.second, *(double *)(&dataArea[dataMap->dataItems[xx.second]->offset]), xx.first );
            getDataItem(am, dataMap, xx.first, xx.second, dataArea);
        }
    }
}

// Function to add a named DataMap object to the map of DataMap objects in asset_manager
void addDataMapObject(asset_manager &assetManager, const std::string &name, DataMap dataMapObject)
{
    assetManager.dataMapObjects[name] = dataMapObject;
}

// Function to map data from the asset_manager to the DataMap data area
// for all simulink types, our naming convention drops the _T from each type (ex: uint32_T is the "uint32" case)
void setDataItem(asset_manager *am, DataMap *dataMap, const std::string &amapName, const std::string &mapName, uint8_t* dataArea)
{
    if (am->amap.find(amapName) != am->amap.end() && dataMap->dataItems.find(mapName) != dataMap->dataItems.end())
    {
        DataItem *dataItem = dataMap->dataItems[mapName];
        if (dataItem->type == "int")
        {
            *(int *)(&dataArea[dataItem->offset]) = am->amap[amapName]->getiVal();
        }
        else if (dataItem->type == "double")
        {
            *(double *)(&dataArea[dataItem->offset]) = am->amap[amapName]->getdVal();
        }
        else if (dataItem->type == "bool")
        {
            *(bool *)(&dataArea[dataItem->offset]) = am->amap[amapName]->getbVal();
        }
        else if (dataItem->type == "uint")
        {
            *(uint_T *)(&dataArea[dataItem->offset]) = am->amap[amapName]->getiVal();
        }
        else if (dataItem->type == "int16")
        {
            *(int16_T *)(&dataArea[dataItem->offset]) = am->amap[amapName]->getiVal();
        }
        else if (dataItem->type == "uint16")
        {
            *(uint32_T *)(&dataArea[dataItem->offset]) = am->amap[amapName]->getiVal();
        }
        else if (dataItem->type == "int32")
        {
            *(int32_T *)(&dataArea[dataItem->offset]) = am->amap[amapName]->getiVal();
        }
        else if (dataItem->type == "uint32")
        {
            *(uint32_T *)(&dataArea[dataItem->offset]) = am->amap[amapName]->getiVal();
        }
        else if (dataItem->type == "int64")
        {
            *(int64_T *)(&dataArea[dataItem->offset]) = am->amap[amapName]->getiVal();
        }
        else if (dataItem->type == "uint64")
        {
            *(uint64_T *)(&dataArea[dataItem->offset]) = am->amap[amapName]->getiVal();
        }
        else if (dataItem->type == "ulong")
        {
            *(ulong_T *)(&dataArea[dataItem->offset]) = am->amap[amapName]->getiVal();
        }
        else if (dataItem->type == "ulonglong")
        {
            *(ulonglong_T *)(&dataArea[dataItem->offset]) = am->amap[amapName]->getiVal();
        }
        else if (dataItem->type == "real")
        {
            *(real_T *)(&dataArea[dataItem->offset]) = am->amap[amapName]->getiVal();
        }
        else if (dataItem->type == "real32")
        {
            *(real32_T *)(&dataArea[dataItem->offset]) = am->amap[amapName]->getiVal();
        }
        else if (dataItem->type == "real64")
        {
            *(real64_T *)(&dataArea[dataItem->offset]) = am->amap[amapName]->getiVal();
        }
        else if (dataItem->type == "time")
        {
            *(time_T *)(&dataArea[dataItem->offset]) = am->amap[amapName]->getiVal();
        }
        // Add more cases for other data types as needed
    }
}

// Function to map data from the DataMap data area to the asset_manager
// for all simulink types, our naming convention drops the _T from each type (ex: uint32_T is the "uint32" case)
void getDataItem(asset_manager* am, DataMap* dataMap, const std::string& amapName, const std::string& mapName, uint8_t* dataArea) {
    if (am->amap.find(amapName) != am->amap.end() && dataMap->dataItems.find(mapName) != dataMap->dataItems.end()) 
    {
        DataItem *dataItem = dataMap->dataItems[mapName];
        if (dataItem->type == "int" || 
            dataItem->type == "uint" ||
            dataItem->type == "int16" ||
            dataItem->type == "uint16" ||
            dataItem->type == "int32" ||
            dataItem->type == "uint32" ||
            dataItem->type == "int64" ||
            dataItem->type == "uint64" ||
            dataItem->type == "ulong" ||
            dataItem->type == "ulonglong")
        {
            am->amap[amapName]->setVal(*(int*)(&dataArea[dataItem->offset]));
        } 
        else if (dataItem->type == "double" || 
                 dataItem->type == "real" ||
                 dataItem->type == "real32" || 
                 dataItem->type == "real64" ||
                 dataItem->type == "time") 
        {
            am->amap[amapName]->setVal(*(double*)(&dataArea[dataItem->offset]));
        }
        else if (dataItem->type == "bool") 
        {
            am->amap[amapName]->setVal(*(bool*)(&dataArea[dataItem->offset]));
        }
        // char case delayed until use case is created and char is needed
    }
}


// gives dataMapObject context in other functions
static Datamaps_test::ExtUPointer_Reference_T dmTestInput;      // reference to the inputs
static Datamaps_test::ExtYPointer_Reference_T dmTestOutput;     // reference to the outputs
static Datamaps_test dmTestObject{ &dmTestInput, &dmTestOutput };

asset_manager *getOrMakeAm(VarMapUtils *vm, varsmap &vmap, const char *pname, const char *amname)
{   
    char* essName = vm->getSysName(vmap);
    auto essam = vm->getaM(vmap, essName);

    // get parent asset manager or make one ourselves
    auto pam = vm->getaM(vmap, pname);
    if (!pam) 
    {
        FPS_PRINT_INFO(" pname [{}]  not found, have to make one", pname );
        pam = new asset_manager(pname);
        vm->setaM(vmap, pname, pam);
        // set up the rest of the am 
        pam->setFrom(essam);            // pam's parent is ess
        essam->addManAsset(pam, pname);
        FPS_PRINT_INFO(" added {} as a new parent asset manager", pname);
    }
    // get asset manager or make one ourselves
    auto am = vm->getaM(vmap, amname);
    if (!am)
    {
        FPS_PRINT_INFO(" amname [{}]  not found, have to make one", amname );
        am = new asset_manager(amname);
        vm->setaM(vmap, amname, am);
        // set up the rest of the am 
        am->setFrom(pam);
        pam->addManAsset(am, amname);
        FPS_PRINT_INFO(" added {} as a new asset manager", amname);
    } 
    return am;
}

void setupDMTest()
{
    FPS_PRINT_INFO("running {}", __func__);

    struct DataMap *dm = new DataMap;
    dm->name = "testDM";

    // adds input field dataItems
    dm->addDataItem((char*)"DMTestDirection",  offsetof(Datamaps_test::ExtUPointer_Reference_T, DMTestDirection),   (char*)"bool", sizeof(boolean_T));

    // output
    dm->addDataItem((char*)"DMTestOut",    offsetof(Datamaps_test::ExtYPointer_Reference_T, DMTestOut),     (char*)"int32", sizeof(int32_T));
    dm->addDataItem((char*)"AdderResult",  offsetof(Datamaps_test::ExtYPointer_Reference_T, AdderResult),   (char*)"int32", sizeof(int32_T));
    dm->addDataItem((char*)"GainResult",   offsetof(Datamaps_test::ExtYPointer_Reference_T, GainResult),    (char*)"int32", sizeof(int32_T));
    dm->addDataItem((char*)"DMTestOutDblNoise",   offsetof(Datamaps_test::ExtYPointer_Reference_T, DMTestOutDblNoise), (char*)"real", sizeof(real_T));
    dm->addDataItem((char*)"DirectionFeedback",   offsetof(Datamaps_test::ExtYPointer_Reference_T, DirectionFeedback), (char*)"bool", sizeof(boolean_T));

    // add this data map to map of all datamaps
    dataMaps[dm->name] = dm;

    // transfer blocks
    dm->addTransferItem("dmTestInputs",     "DMTestDirection", "DMTestDirection");

    dm->addTransferItem("dmTestOutputs", "DMTestOut", "DMTestOut");
    dm->addTransferItem("dmTestOutputs", "AdderResult", "AdderResult");
    dm->addTransferItem("dmTestOutputs", "GainResult", "GainResult");
    dm->addTransferItem("dmTestOutputs", "DMTestOutDblNoise", "DMTestOutDblNoise");
    dm->addTransferItem("dmTestOutputs", "DirectionFeedback", "DirectionFeedback");

}

void setupDMtestAmap(VarMapUtils *vm, varsmap &vmap, asset_manager *am)
{
    FPS_PRINT_INFO("setting up demo datamap amap for /control/{}", am->name);
    int ival = 0;
    double dval = 0;
    bool bval = false;
    
    am->amap["DMTestDirection"]     = vm->setVal(vmap, "/control/dataMapTest", "DMTestDirection", bval);

    am->amap["DMTestOut"]           = vm->setVal(vmap, "/control/dataMapTest", "DMTestOut", ival);
    am->amap["AdderResult"]         = vm->setVal(vmap, "/control/dataMapTest", "AdderResult", ival);
    am->amap["GainResult"]          = vm->setVal(vmap, "/control/dataMapTest", "GainResult", ival);
    am->amap["DMTestOutDblNoise"]   = vm->setVal(vmap, "/control/dataMapTest", "DMTestOutDblNoise", dval);
    am->amap["DirectionFeedback"]   = vm->setVal(vmap, "/control/dataMapTest", "DirectionFeedback", bval);

}


// on scheduler functions
int runDataMaps(varsmap &vmap, varmap &amap, const char* aname, fims* p_fims, assetVar* aV)
{
    FPS_PRINT_INFO("\n\n\nIN RUN DATAMAPS");
    asset_manager *am = aV->am;
    VarMapUtils *vm = am->vm;

    int debug = 1;
    int reload = 0;
    essPerf ePerf(am, aname, __func__);

    auto relname = fmt::format("{}_{}", __func__, "reload");
    assetVar* reloadAV = amap[relname];
    asset_manager *testDMam = getOrMakeAm(vm, vmap, aname, "testDM_asset_manager");

    if (!reloadAV || (reload = reloadAV->getiVal()) == 0)
    {
        reload = 0;  // complete reset  reload = 1 for remap ( links may have changed)
    }

    if (reload < 2)
    {
        if (debug) FPS_PRINT_INFO("Reloading ...");
        // datamap and amap init functions (what used to be setup_demo_rack and setup_demo_rack_amap)
        setupDMTest();

        setupDMtestAmap(vm, vmap, testDMam);
        if (debug) FPS_PRINT_INFO("amap set up");

        reload = 2;
        if(!reloadAV)
        {
            reloadAV = vm->setVal(vmap, "/reload", relname.c_str(), reload);
            amap[relname] = reloadAV;
        }
        else 
        {
            reloadAV->setVal(reload);
        }

        if (debug) FPS_PRINT_INFO("Finished reloading");
    }
    
    // getFromAmap
    DataMap *dm = dataMaps["testDM"];

    dm->getFromAmap("dmTestInputs", testDMam, dm, (uint8_t *)&dmTestInput);
    
    if (debug){
        FPS_PRINT_INFO("Got these values from amap:");
        FPS_PRINT_INFO("dmTestOutput.DirectionFeedback: {}", dmTestOutput.DirectionFeedback);       // NEW! testing
        FPS_PRINT_INFO("dmTestOutput.DMTestOut: {}", dmTestOutput.DMTestOut);
        FPS_PRINT_INFO("dmTestOutput.DMTestOutDblNoise: {}", dmTestOutput.DMTestOutDblNoise);       // NEW! testing
        FPS_PRINT_INFO("dmTestOutput.AdderResult: {}", dmTestOutput.AdderResult);
        FPS_PRINT_INFO("dmTestOutput.GainResult: {}", dmTestOutput.GainResult);        
    }

    FPS_PRINT_INFO("\n\nRunning step function\n");
    dmTestObject.step();

    if (debug){
        FPS_PRINT_INFO("Sending these values back to amap:");
        FPS_PRINT_INFO("dmTestOutput.DirectionFeedback: {}", dmTestOutput.DirectionFeedback);       // NEW! testing
        FPS_PRINT_INFO("dmTestOutput.DMTestOut: {}", dmTestOutput.DMTestOut);
        FPS_PRINT_INFO("dmTestOutput.DMTestOutDblNoise: {}", dmTestOutput.DMTestOutDblNoise);       // NEW! testing
        FPS_PRINT_INFO("dmTestOutput.AdderResult: {}", dmTestOutput.AdderResult);
        FPS_PRINT_INFO("dmTestOutput.GainResult: {}", dmTestOutput.GainResult);
    }

    dm->sendToAmap("dmTestOutputs", testDMam, dm, (uint8_t *)&dmTestOutput);

    FPS_PRINT_INFO("done");
    return 0;
}

// this is the way we set up threads etc.
/*
This sets up the system to process the commands

fims_send -m set -r /$$ -u /ess/demo/threads '{
    "demoThreads":{
        "value":0,
        "actions":{
            "onSet":[{"func":[
                {"func":"demoThreads"}
                ]}]}}}}'
    
    fims_send -m set -r /$$ -u /ess/demo/threads '{"demoThreads":{"value":0,"cmd":"make_thread_class","name":"pcs_clss"}}'
    fims_send -m set -r /$$ -u /ess/demo/threads '{"demoThreads":{"value":0,"cmd":"start_thread","class":"pcs", "name":"pcs_1"}}'

*/
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <map>
#include <unordered_map>



// This section is all about threads.
// we start with starting / stopping signalling threads 
// the EssThread is a generic thread that blongs to  thread class.
// The thread class holds a list of functions and other stuff we'll get to later.
// 
// The first thing we have to do is set up a thread class.
// Just ask for a class and you'll get one.
// We've not done this yet but you can give a class a series of datamaps describing to the rest of the system the data objects 
// that the thread is going to be using.

// next the thread class is given a list of functions.
// there are two kinds of functions .. ones that the thread runs to perform its actions.
// These all assume a common layout ( status, inuts , outputs).
// the other range  of functions are those that the ess controller uses to transfer data into and out of the thread.
// these functions use datamaps.
// 
//
// First lets talk about sending the threads signals.
// we can use the signal_thread function to sent the signal ( we'll call it a wakeup) to the thread.
//
// threads run in a simple state machine.
// On start up the thread asks for the ess_controller to send the thread its initial data.
// the thread creates a scheditem and sends it of the the ess_controller system scheduler.
// The scheditem will have a pointer to a targAv ( target assetvar ) and a pointer to a function to run.
// this is one of the ess_controller functions. The scheduler will execute that function as soon as possible in the ess_controller context.
// this function could initialte a transfer map to pull data out of the ess_controller name space into the threads data area.
// when the thread instance is created from the thread class thread data areas are created for the status input nad output thread data spaces.
// when the thread sends an instruction to get data from the ess_controller the thread will enter a wait state and not 
// process any data in its data areas , allowing the ess_controle free access to that data area.
// the ess_controller schedules the function and then transfers data from the ess_controller data space into the thread area.
// onc the data transfer is completed, the ess_controller function will send a return signal back to the thread to tell 
// the thread that the transfer has completed.
// the thread can then use that data without fear of thread contention with the ess_controller.
// the init , input , output data transfers all follow the same sort of process.

// to make this work a modified scheduler is needed, this will receive the schedule item from the thread and the trigger the function
// to transfer the data. 
// then the function should send the completed return signal to the thread.

// when the thread is set up , default thread functions are defined.
// these functions can be overridden to adjust the thread operation to suit the thread class design.
//
// In addition to the basic "init" , "input" , "output" and "step" functions. the thread can be given othe functions to perform on exceptions.
// to do this the functions will have to be registerd with the thread calss or the thread instance.
// the class functions can be overriddent by specific functions for a thread instance.
// Sinnalling the thread with a reference to one of thise special functons, or any other signal , will cause the thread to execute 
//that function , as an exception, when the signal is received .

// the threads can also send a fims message back to the ess_controller.
// the thread will have to construct a fims_message using the helper functions, and the message will be treated as if it had been received 
// using the fims input thread.
//
// direct data input from the ess_controller. 
// A data map item transferring data from the ess_controller can also be sent as a message to the thread.
// so, for example, if we have a fault and alarm monitoring thread the incoming data from the peripheral can be remapped to a thread item.
// INfact it will be remapped to a thread transfer item . This process is used as an alternative method to the block data transfer . 
// This transfer is better suited to processing the input data as it arrives on the fims_channel.




// and ones 

// struct DataMap *dm = new DataMap;
//     dm->name = "testDM";

//     // adds input field dataItems
//     dm->addDataItem((char*)"DMTestDirection",  offsetof(Datamaps_test::ExtUPointer_Reference_T, DMTestDirection),   (char*)"bool", sizeof(boolean_T));

//     // output
//     dm->addDataItem((char*)"DMTestOut",    offsetof(Datamaps_test::ExtYPointer_Reference_T, DMTestOut),     (char*)"int32", sizeof(int32_T));
//     dm->addDataItem((char*)"AdderResult",  offsetof(Datamaps_test::ExtYPointer_Reference_T, AdderResult),   (char*)"int32", sizeof(int32_T));
//     dm->addDataItem((char*)"GainResult",   offsetof(Datamaps_test::ExtYPointer_Reference_T, GainResult),    (char*)"int32", sizeof(int32_T));
//     dm->addDataItem((char*)"DMTestOutDblNoise",   offsetof(Datamaps_test::ExtYPointer_Reference_T, DMTestOutDblNoise), (char*)"real", sizeof(real_T));
//     dm->addDataItem((char*)"DirectionFeedback",   offsetof(Datamaps_test::ExtYPointer_Reference_T, DirectionFeedback), (char*)"bool", sizeof(boolean_T));

//     // add this data map to map of all datamaps
//     dataMaps[dm->name] = dm;

//     // transfer blocks

// enum class Type {
//     INT,
//     DOUBLE,
//     INT16,
//     UINT16,
//     INT32,
//     UINT32,
//     BOOL,
//     UNKNOWN
// };

// struct threadDataItem {
//     Type type;
//     std::string name;
//     int offset;
//     int size;
// };

//using threadDataItems = std::unordered_map<std::string, threadDataItem>;
//using threadDataFields = std::vector<threadDataItem>;
void EssThread(ess_thread_inst* ess);


// struct threadDataMap {
//     std::string name;
//     //ResultFields fields;
//     threadDataItems items;
//     //int offset;
//     //int size;
// };

//using threadDataMaps = std::map<std::string, threadDataMap>;
ess_thread_class *makeEssThreadClass(char *cname);


Type stringToType(const std::string& s) {
    static const std::unordered_map<std::string, Type> typeMap = {
        {"int", Type::INT},
        {"double", Type::DOUBLE},
        {"int16", Type::INT16},
        {"uint16", Type::UINT16},
        {"int32", Type::INT32},
        {"uint32", Type::UINT32},
        {"bool", Type::BOOL}
    };

    auto it = typeMap.find(s);
    if (it != typeMap.end()) {
        return it->second;
    }

    return Type::UNKNOWN;
}

size_t stringToSize(Type type) {
    switch(type) {
        case Type::INT: return sizeof(int);
        case Type::DOUBLE: return sizeof(double);
        case Type::INT16: return sizeof(int16_t);
        case Type::UINT16: return sizeof(uint16_t);
        case Type::INT32: return sizeof(int32_t);
        case Type::UINT32: return sizeof(uint32_t);
        case Type::BOOL: return sizeof(bool);
        default: return 0;  // unknown type size
    }
}

std::string typeToString(Type type) {
    switch(type) {
        case Type::INT: return "int";
        case Type::DOUBLE: return "double";
        case Type::INT16: return "int16";
        case Type::UINT16: return "uint16";
        case Type::INT32: return "int32";
        case Type::UINT32: return "uint32";
        case Type::BOOL: return "bool";
        default: return "unknown";
    }
}


// bool createDataItemsMap(ResultItems &resultItems, const std::vector<IncludeField>& fields) {
//     for (const auto& field : fields) {
//         resultItems[field.name] = field;
//     }
//     return true;
// }
void showMap(threadDataMap &resultMap)
{
    // for ( auto &item : resultMap) {

    //     showMap(item);
    // }

}

void showMaps(threadDataMaps &resultMap)
{
    for ( auto &item : resultMap) {

        showMap(item.second);
    }

}

std::string removeTrailingSemicolon(const std::string& str) {
    if (!str.empty() && str.back() == ';') {
        return str.substr(0, str.size() - 1);
    }
    return str;
}

bool parseIncludeString(threadDataMaps &resultMap, const std::string& s) {
    std::istringstream iss(s);
    std::string line;

    while (std::getline(iss, line)) {
        //std::cout << " line  1:[" << line << "]" << std::endl;

        std::istringstream lineStream(line);
        std::string word;
        lineStream >> word;

        if (word == "struct") {
            std::string structName;
            lineStream >> structName;

            //std::vector<IncludeField> fields;

            // Parse the struct until the closing };
            threadDataMap map;

            int offset = 0;

            while (std::getline(iss, line) && line != "};") {
                //std::cout << " line 2:[" << line << "]" << std::endl;
                std::istringstream structStream(line);
                threadDataItem field;
                std::string typeStr;
                if (structStream >> typeStr >> field.name) {
                    field.name = removeTrailingSemicolon(field.name);
                    //std::cout << "Typestr :[" << typeStr << "] Field name :["<< field.name <<"]"<< std::endl;
                    field.type = stringToType(typeStr);
                    field.size = stringToSize(field.type);
                    field.offset = offset;
                    offset += field.size;
                    map.items[field.name] = field; 
                }
            }
            resultMap[structName] =  map;

        }
    }

    return true;
}
// send_fims_message internally
int send_fims_message(asset_manager *am, char *tclass, char *name, char *method , char *uri, char *body)
{
    //auto ess_class = makeEssThreadClass(tclass);
    //char* buf = vm->fimsToBuffer(method, uri, replyto, body);
    //char* buf = vm->fimsToBuffer(method, uri, nullptr, body);
    //free((void *)body);
    //fims_message* msg = vm->bufferToFims(buf);
    fims_message* msg = (fims_message*)malloc(sizeof(fims_message));
    //free((void*)buf);
    if (msg)
    {
        msg->method = strdup(method);
        msg->replyto = nullptr;
        msg->uri = strdup(uri);
        msg->body = strdup(body);
        msg->process_name = strdup(tclass);
        msg->username = strdup(name);
        //std::cout << "sending fims message\n";
        am->fimsChan->put(msg);
        am->wakeChan->put(0);   // but this does not get serviced immediatey
    }
    else
    {
        std::cout << "not sending fims message\n";
    }
    return 0;
}


// Thread safe, TODO get p_fims from tclass
int export_fims_message(fims *p_fims, char *tclass, char *name, char *method , char *uri, char *body)
{
    //auto ess_class = makeEssThreadClass(tclass);
    //char* buf = vm->fimsToBuffer(method, uri, replyto, body);
    //char* buf = vm->fimsToBuffer(method, uri, nullptr, body);
    //free((void *)body);
    //fims_message* msg = vm->bufferToFims(buf);
    if (p_fims)
    {
        p_fims->Send(method, uri, nullptr, body, name);
    }
    else
    {
        std::cout << "not exporting fims message\n";
    }
    // fims_message* msg = (fims_message*)malloc(sizeof(fims_message));
    // //free((void*)buf);
    // if (msg)
    // {
    //     msg->method = strdup(method);
    //     msg->replyto = nullptr;
    //     msg->uri = strdup(uri);
    //     msg->body = strdup(body);
    //     msg->process_name = strdup(tclass);
    //     msg->username = strdup(name);
    //     //std::cout << "sending fims message\n";
    //     //am->fimsChan->put(msg);
    //     //am->wakeChan->put(0);   // but this does not get serviced immediatey
    // }
    // else
    // {
    //     std::cout << "not sending fims message\n";
    // }
    return 0;
}

// TODO this will have to recover the class and then send the av plus value to the thread
int testcompFunc(varsmap &vmap, varmap &amap, const char *aname, fims *p_fims, assetVar *aV)
{

    FPS_PRINT_INFO("     >>> running {}  for aV [{}] ", __func__, aV->getfName());
    return 0;

}


// TODO ge the func from the class 
void setup_compFunc(VarMapUtils *vm, varsmap& vmap, asset_manager *am, char *tclass , char *name, char *func, char *aV)
{

    assetVar *av = vm->getVar(vmap, aV, nullptr);
    if (!av)
    {
        double dval = 0.0;
        av = vm->setVal(vmap, aV, nullptr,  dval);
        av->am = am;
    }
    if(!av)
    {
        FPS_PRINT_INFO("     >>> running {}  no aV [{}] found", __func__, aV);
        return;
    }
    if(!av->extras)
    {
        av->extras = new assetExtras;
    }
    void *compFunc = vm->getFunc(vmap, "comp", func);
    if (!compFunc)
    {
        compFunc = (void*)testcompFunc;
    }
    av->extras->compFunc = compFunc;
    av->setParam("_thread_class",tclass);
    av->setParam("_thread_inst",name);
    
    // auto ess_thread = makeEssThreadInst(tclass, name);
    // if(ess_thread && ess_thread->init)
    // {
    //     //ess_thread->wakeChan.put(signal);
    // }
}


// test for auto parse
int test_maps(char *tclass, char *structc) 
{
    // std::string includeString = R"(
    //         struct mystruct {
    //             int integerName;
    //             double doubleName;
    //             int16 int16Name;
    //             uint16 uint16Name;
    //             int32 int32Name;
    //             uint32 uint32Name;
    //             bool boolName;
    //         };
    // )";
    auto ess_class = makeEssThreadClass(tclass);

    std::string includeString(structc);
    //threadDataMaps resultMap;
    
    parseIncludeString(ess_class->tdataMaps, includeString);

    for (const auto& entry : ess_class->tdataMaps) {
        std::cout << "Struct Name: " << entry.first << std::endl;
        threadDataMap map = entry.second; 
        for (const auto& fieldit : map.items) {
            threadDataItem field = fieldit.second;
            std::cout << "  Type: " << typeToString(field.type) 
                      << ", Name: " << field.name 
                      << ", Size: " << field.size << " bytes" 
                      << ", Offset: " << field.offset << " bytes" << std::endl;
        }
    }

// Clean up allocated memory
    // for (auto& entry : resultMap) {
    //     delete entry.second;
    // }

    return 0;
}
// todo put this somewhere 
std::unordered_map<std::string, ess_thread_class*> ess_classes; 

ess_thread_class *makeEssThreadClass(char *cname)
{
    if (!cname)
        return nullptr;
    if (!ess_classes[cname]) {
        FPS_PRINT_INFO("     running {}  creating  class name [{}] ", __func__, cname);
        ess_classes[cname] = new ess_thread_class(cname);
        ess_classes[cname]->init = false;
    }
    return ess_classes[cname];
}

ess_thread_inst *makeEssThreadInst(char *cname, char *name)
{
    if (!cname || !name)
        return nullptr;

    ess_thread_class *ess_class = makeEssThreadClass(cname);
    if (!ess_class->threads[name]) {
        FPS_PRINT_INFO("     running {}  creating  thread name [{}] ", __func__, name);
        ess_class->threads[name] = new ess_thread_inst(name);
        ess_class->threads[name]->init = false;

    }
    return ess_class->threads[name];
}
// this needs to be moved to the class def
// well start a thread as a class instance
extern "C++"
{
    int threadExtInitFcn(varsmap &vmap, varmap &amap, const char* aname, fims* p_fims, assetVar* aV);
    int threadExtInputFcn(varsmap &vmap, varmap &amap, const char* aname, fims* p_fims, assetVar* aV);
    int threadExtOutputFcn(varsmap &vmap, varmap &amap, const char* aname, fims* p_fims, assetVar* aV);
    int threadExtSetupFcn(varsmap &vmap, varmap &amap, const char* aname, fims* p_fims, assetVar* aV);
};

void run_thread_func_p(ess_thread_inst *ess, char *name, char *funname , int retsig);

void EssThread(ess_thread_inst* ess)
{
    FPS_PRINT_INFO("Essthread {} started", ess->name);

    double delay = 1.0; // Sec
    while (ess->running)
    {
        FPS_PRINT_INFO("Essthread {} waiting", ess->name);
        bool bf = ess->wakeChan.timedGet(ess->wakeup, delay);
        FPS_PRINT_INFO("wakeup {}: state {} bf {}", ess->wakeup, ess->state, bf);
        //ess->state++;

        if(bf)  // bf == false if this is a timeout
        {
            if(ess->wakeup == -1)  // stop
            {
                 ess->running = 0;
            }
            else {
                runSigFcn(ess, ess->wakeup);

                if(ess->wakeup == 100) // transfer_in complete 100 -> 101
                    ess->state = 101;
                if(ess->wakeup == 200) // transfer_out complete 200 -> 201
                    ess->state = 201;
                if(ess->wakeup == 50){ /* ask for transfer in */
                    run_thread_func_p(ess, (char*)ess->name.c_str(), (char *)"initFcn", 101);
                    ess->state = 100;
                }
            }
        }

        if(ess->state == 101){/* transfer in complete, run stuff */      ess->state = 102;}
        if(ess->state == 102){/* run stuff */                            ess->state = 102;}
        if(ess->state == 200){/* run stuff done ask for transfer out */  ess->state = 200;}
        if(ess->state == 201){/* transfer out done wait for next run  */ ess->state = 201;}

    }

    FPS_PRINT_INFO("thread shutting down");
    return;
}

//void signal_thread(char * tclass, char *name, int signal);
void signal_thread(VarMapUtils *vm, varsmap& vmap, asset_manager *am,char *tclass , char *name, int signal);


int threadExtInitFcn(varsmap &vmap, varmap &amap, const char* aname, fims* p_fims, assetVar* aV)
{
    asset_manager *am = aV->am;
    if (am && aname)
    {
        FPS_PRINT_INFO("running thread fcn {}  am {} , aname {},  av {}", __func__, am->name, aname, aV->getfName());

    }
    else if (am)
    {
        FPS_PRINT_INFO("running thread fcn {}  am {},  no aname av {}", __func__, am->name, aV->getfName());
    }
    else
    {
        FPS_PRINT_INFO("running thread fcn {}  no am and no aname av {}", __func__, aV->getfName());
    }

    // this tells the thread that we are done 
    int retSig = aV->getiParam("retSig");
    char *tname = aV->getcParam("thread_name");
    char *tclass = aV->getcParam("thread_class");
    if (tclass && tname)
    {
        signal_thread(am->vm, vmap, am, tclass, tname, retSig);
    }

    // TODO return the signal to the thread.

    return 0;
}

int threadExtInputFcn(varsmap &vmap, varmap &amap, const char* aname, fims* p_fims, assetVar* aV)
{
    FPS_PRINT_INFO("running thread fcn {}", __func__);
    return 0;
}

int threadExtOutputFcn(varsmap &vmap, varmap &amap, const char* aname, fims* p_fims, assetVar* aV)
{
    FPS_PRINT_INFO("running thread fcn {} av {}", __func__, aV->getfName());
    return 0;
}
int threadExtSetupFcn(varsmap &vmap, varmap &amap, const char* aname, fims* p_fims, assetVar* aV)
{
    FPS_PRINT_INFO("running thread fcn {}", __func__);
    return 0;
}

long int get_tsc_us();


void run_thread_func_p(ess_thread_inst *ess, char *name, char *funname , int retsig)
{
    //ess_thread *ess = threadMaps[name];
    if (!ess) {
        FPS_PRINT_ERROR(" cant find thread {}", name);
        return;

    }
    void *fptr =  ess->threadMap[funname];
    if (!fptr) {
        fptr = ess->thread_class->threadMap[funname];
        if (!fptr) {
            FPS_PRINT_ERROR(" cant find thread  function {}", funname);
            return;
        }
    }
    void *targAv =  ess->threadMap["targAv"];
    if (!targAv) {
        FPS_PRINT_ERROR(" cant find thread  function {}", funname);
        return;
    }
    asset_manager *am =  (asset_manager *)ess->threadMap["am"];
    if (!am) {
        FPS_PRINT_ERROR(" cant find thread am {}", name);
        return;
    }

    //may need to have multiple retsigs
    ess->retSig = retsig;
    
    schedItem* as = nullptr;
    as = new schedItem();
    as->id = strdup(name);    
    as->fast = 2;
    // as->nodelete = true;
    as->fcnptr = fptr;
    
    as->targaVp = (assetVar *) targAv;
    as->av = as->targaVp;

    as->am = am;
    //return  (double)get_time_us() / 1000000.0 - base_time;
    double tNow = (double)get_tsc_us() / 1000000.0 - *ess->base_timep;
    as->runTime = tNow;
    // Done access the param in a non thread safe way.
    // TODO add direct runtime to as management.
    //as->av->setParam("runTime",tNow);
    //as->av->setParam("retSig",retsig);
    
    as->thread = true;
    as->thread_runTime = tNow;
    as->thread_retSig = retsig;


    // as->am also set  we can go fast
    // also need as->av
    if (am->reqChan)
    {
        channel <schedItem*>* reqChan = (channel <schedItem*>*)am->reqChan;
        reqChan->put(as);
        if (am->wakeChan)
        {
            am->wakeChan->put(0);
        }
    }
}

void run_thread_func(char * tclass, char *name, char *funname , int retsig)
{
    auto ess_thread = makeEssThreadInst(tclass, name);

    if (ess_thread)
    {
        run_thread_func_p(ess_thread, name, funname, retsig);

    }
}
void addThreadSigFcn(VarMapUtils *vm, varsmap& vmap, asset_manager *am,char *tclass, char *name, int signal, void* fcn);

void start_thread(VarMapUtils *vm, varsmap& vmap, asset_manager *am, char *tclass, char *name)
{
    auto ess_class = makeEssThreadClass(tclass);
    auto ess_thread = makeEssThreadInst(tclass, name);
    if (!ess_class->init)
    {
        // the class holds the thread functions

        FPS_PRINT_INFO("setting up  class {} 1", tclass);
        ess_class->threadMap["initFcn"]     = (void*) threadExtInitFcn;
        ess_class->threadMap["setupFcn"]    = (void*) threadExtSetupFcn;
        ess_class->threadMap["inputFcn"]    = (void*) threadExtInputFcn;
        ess_class->threadMap["outputFcn"]   = (void*) threadExtOutputFcn;

        double dval = 0.0;
        auto mystr          = fmt::format("/thread/{}", tclass, name);


        ess_class->threadMap["threadUri"]    = (void*) vm->setVal(vmap, mystr.c_str(), "threadUri",  dval);
        assetVar *threadAv = (assetVar *) ess_class->threadMap["threadUri"];
        
        threadAv->am = am;
        threadAv->setParam("thread_class", tclass);

        // the class holds the asset manager
        ess_class->threadMap["am"]          = (void*)am;

        // the class holds the data maps
        // todo

        // the class holds the thead function
        ess_class->ess_thread_fun = EssThread;
        ess_class->init = true;
    }
    if (!ess_thread->init)
    {
        FPS_PRINT_INFO("setting up  class {}  thread {} ", tclass, name);
        ess_thread->thread_class = ess_class;

        // TODO use class for this 
        // char *schedid;
        // // this is the name of the i/ouri
        // assetVar *scheduri;
        // assetVar* schedAv;
        // assetVar* targAv;  //note that the thread cannot access the targav.
        // asset_manager *am;
        ess_thread->base_timep = &vm->base_time;

        std::string mystr             = fmt::format("thread_{}_{}", tclass, name);
        ess_thread->threadMap["schedId"]     = (void*)strdup(mystr.c_str());
        ess_thread->threadMap["am"]          = (void*)am;
        double dval = 0.0;
        mystr          = fmt::format("/thread/{}_{}", tclass, name);

        ess_thread->threadMap["schedUri"]    = (void*) vm->setVal(vmap, mystr.c_str(), "schedUri",  dval);
        ess_thread->threadMap["schedAv"]     = (void*) vm->setVal(vmap, mystr.c_str(), "schedAv",   dval);
        ess_thread->threadMap["targAv"]      = (void*) vm->setVal(vmap, mystr.c_str(), "targAv",    dval);
        //ess_thread->threadMap["initFcn"]     = (void*) threadExtInitFcn;
        //ess_thread->threadMap["setupFcn"]    = (void*) threadExtSetupFcn;
        //ess_thread->threadMap["inputFcn"]    = (void*) threadExtInputFcn;
        //ess_thread->threadMap["outputFcn"]   = (void*) threadExtOutputFcn;
        // data  init name
        assetVar *targAv = (assetVar *) ess_thread->threadMap["targAv"];
        
        targAv->am = am;

        targAv->setParam("thread_name", name);
        targAv->setParam("thread_class", tclass);

        //void *initfcn;
        // data in names
        //void *inputfcn;
        //data out name
        //void *outputfcn;
        addThreadSigFcn(am->vm, vmap, am, tclass, name, 4321, (void*)testSigFcn);

        ess_thread->init = true;

    }
    else
    {
        FPS_PRINT_INFO("starting thread {}_{}", tclass, name);
    }
    //std::thread id(EssThread, ess);
    ess_thread->running = 1;
    ess_thread->delay = 1.0;
    //ess_thread->wakeChan = &ess_wakeChan;

    ess_thread->state = 0;
    ess_thread->id = std::thread(ess_class->ess_thread_fun, ess_thread);
    //ess_thread->id.detach();
    
    FPS_PRINT_INFO(" func thread {} started", name);
//    threadMaps[name]=ess;
}


void stop_thread(VarMapUtils *vm, varsmap& vmap, asset_manager *am, char *tclass, char *name)
{
    auto ess_thread = makeEssThreadInst(tclass, name);
    //ess_thread *ess = threadMaps[name];
    if(ess_thread && ess_thread->init)
    {
        ess_thread->running = 0;
        //ess_thread->wakeup = -1;
        ess_thread->wakeChan.put(-1);

        //ess_thread->id = id;
        ess_thread->id.join();
        //delete(ess);
    }

    // may need a lock here
    //threadMaps[name]=nullptr;
}

void signal_thread(VarMapUtils *vm, varsmap& vmap, asset_manager *am,char *tclass , char *name, int signal)
{
    auto ess_thread = makeEssThreadInst(tclass, name);
    if(ess_thread && ess_thread->init)
    {
        ess_thread->wakeChan.put(signal);
    }
}


int runSigFcn(ess_thread_inst *ess_thread, int sig)
{
    FPS_PRINT_INFO("{} seeking   [{}] sig [{}]", __func__, ess_thread->name, sig);
    if ( ess_thread->datasigs.find(sig) != ess_thread->datasigs.end()) 
    {
        FPS_PRINT_INFO("{} running   [{}] sig [{}]", __func__, ess_thread->name, sig);
        
        //typedef void (*ThreadFuncType)(ess_thread_inst*, int);
        void *fcn = ess_thread->datasigs[sig];
        ThreadFuncType funcPtr = reinterpret_cast<ThreadFuncType>(fcn);
        funcPtr(ess_thread , sig);
    }

    return 0;
}

void addThreadSigFcn(VarMapUtils *vm, varsmap& vmap, asset_manager *am,char *tclass, char *name, int signal, void* fcn)
{
    ess_thread_inst *ess_thread;
    ess_thread_class *ess_class;
    
    if (name)
    {
        ess_thread = makeEssThreadInst(tclass, name);
        ess_thread->datasigs[signal]=fcn;
    }
    else
    {
        ess_class = makeEssThreadClass(tclass);
        ess_class->datasigs[signal]=fcn;
    }
    return;
}

int demoThreads(varsmap &vmap, varmap &amap, const char* aname, fims* p_fims, assetVar* aV)
{

    if (!aV->am) {
        std::cout <<">>>> running func " << __func__ << " with aV [" << aV->getfName()<< "]  no asset_manager "<< std::endl;
        return -1;

    }

    asset_manager *am = aV->am;
    VarMapUtils *vm = am->vm;

    double tNow = vm->get_time_dbl();
    std::string cmd;

    FPS_PRINT_INFO("running {}  with aV  [{}] aname [{}] time [{}]", __func__,aV->getfName(), aname ,tNow);
    auto ccmd = aV->getcParam("cmd");
    if(ccmd)
    {
        cmd = ccmd;
        FPS_PRINT_INFO("     running {}  with cmd  [{}]", __func__, cmd );
        //ess_thread_class * ess_class;
        //ess_thread_inst * ess_inst;

    
        if (cmd == "make_thread_class") {
            char *cname = aV->getcParam("class");
            makeEssThreadClass(cname);
        }
        if (cmd == "make_thread_inst") {
            char *name = aV->getcParam("name");
            char *cname = aV->getcParam("class");
            makeEssThreadInst(cname, name);
        }
        if (cmd == "start_thread") {
            char *name = aV->getcParam("name");
            char *cname = aV->getcParam("class");
            start_thread(vm, vmap, am, cname, name);
//            makeEssThreadInst(cname, name);
        }
        if (cmd == "stop_thread") {
            char *name = aV->getcParam("name");
            char *cname = aV->getcParam("class");
            stop_thread(vm, vmap, am, cname, name);
//            makeEssThreadInst(cname, name);
        }
        if (cmd == "signal_thread") {
            char *name = aV->getcParam("name");
            char *cname = aV->getcParam("class");
            int sig = aV->getiParam("signal");
            signal_thread(vm, vmap, am, cname, name, sig);
//            makeEssThreadInst(cname, name);
        }
        if (cmd == "test_maps") {
            char *sname = aV->getcParam("struct");
            char *cname = aV->getcParam("class");
            //int sig = aV->getiParam("signal");
            test_maps(cname, sname);
            //signal_thread(vm, vmap, am, cname, name, sig);
//            makeEssThreadInst(cname, name);
        }
        if (cmd == "send_fims_message") {
            char *cname = aV->getcParam("class");
            char *name = aV->getcParam("name");
            char *method = aV->getcParam("method");
            char *uri = aV->getcParam("uri");
            char *body = aV->getcParam("body");
            if(cname && name && method && uri && body)
            {
                send_fims_message(am, cname, name, method , uri, body);
            }
        }
        if (cmd == "export_fims_message") {
            char *cname = aV->getcParam("class");
            char *name = aV->getcParam("name");
            char *method = aV->getcParam("method");
            char *uri = aV->getcParam("uri");
            char *body = aV->getcParam("body");
            if(cname && name && method && uri && body)
            {
                export_fims_message(p_fims, cname, name, method , uri, body);
            }
        }
        if (cmd == "setup_compFunc") {
            char *cname = aV->getcParam("class");
            char *name = aV->getcParam("name");
            char *func = aV->getcParam("func");
            char *aVname = aV->getcParam("aV");
            if(cname && name && func && aVname)
            {
                setup_compFunc (vm , vmap, am, cname, name, func , aVname);
            }
        }
    }
    return 0;
}





/*
sh-4.2# fims_send -m set -r /$$ -u /ess/demo/threads '{                                                                     "demoThreads":{
        "value":0,
        "actions":{
            "onSet":[{"func":[
                {"func":"demoThreads"}
                ]}]}}}}'
sh-4.2# fims_send -m set -r /$$ -u /ess/demo/threads '{
    "demoThreads":{
        "value":0,
        "cmd":"test_maps",
        "class":"pcs", 
        "name":"pcs_1", 
        "struct": "
struct mystruct {
                int integerName;
                double doubleName;
                int16 int16Name;
                uint16 uint16Name;
                int32 int32Name;
                uint32 uint32Name;
                bool boolName;
            };"}}'



sh-4.2# fims_send -m set -r /$$ -u /ess/demo/threads '{
    "demoThreads":{
        "value":0,
        "cmd":"send_fims_message",
        "class":"pcs",
        "name":"pcs_1",
        "method":"set",
        "uri": "/test/fims_test/var",
        "body":"{\"value\":1203}"}}'

sh-4.2# fims_send -m set -r /$$ -u /ess/demo/threads '{
    "demoThreads":{
        "value":0,
        "cmd":"send_fims_message",
        "class":"pcs",
        "name":"pcs_1",
        "method":"set",
        "uri": "/test/fims_test/var2",
        "body":"{\"value\":\"it worked\"}"}}'

         char *cname = aV->getcParam("class");
            char *name = aV->getcParam("name");
            char *func = aV->getcParam("func");
            char *aV = aV->getcParam("aV");
            if(cname && name && func && aV)
            {
                setup_compFunc (vm , vmap, cname, name, func , aV);
            }

sh-4.2# fims_send -m set -r /$$ -u /ess/demo/threads '{
    "demoThreads":{
        "value":0,
        "cmd":"setup_compFunc",
        "class":"pcs",
        "name":"pcs_1",
        "func":"testFunc",
        "aV": "/test/comp_test/myvar"
        
        }}'


TO RUN THINGS

    This runs the func twice (change inValue to != 0 if you only want it to run once)
fims_send -m set -r /$$ -u /ess/demo/code '{
    "runExtCode":{
        "value":0,
        "actions":{
            "onSet":[
                {"func":
                    [
                        {"func":"demoExtCode", "inValue":0},
                        {"func":"demoExtCode"}
                    ]
                }
            ]
        }
    }
}'


    this wil set up two bms_racks each with 2 modules
fims_send -m set -r /$$ -u /ess/demo/code '{"runExtCode":{"value":15,"cmd":"setup_demo","pname":"rack","num_racks":2, "num_modules":2}}'


    to test set_vlink
fims_send -m set -r /$$ -u /ess/demo/code '{"runExtCode":{"value":0,"cmd":"set_vlink","amap":"someVar","amname":"ext_1", "uri":"/components/ext_1:someVar","val":1111}}'
    

    to test set_amap
fims_send -m set -r /$$ -u /ess/demo/code '{"runExtCode":{"value":0,"cmd":"set_amap","amap":"someVar","amname":"ext_1", "uri":"/sys/status:someVar","val":2222}}'


    this sets up the parent and the asset  using set_mapping
fims_send -m set -r /$$ -u /ess/demo/code '{"runExtCode":{"value":3,"cmd":"set_mapping","pname":"ext","amname":"ext_1"}}'
fims_send -m set -r /$$ -u /ess/demo/code '{"runExtCode":{"value":3,"cmd":"set_mapping","pname":"ext","amname":"ext_2"}}'


    use these to inspect system status, and specific parent or asset
fims_send -m get -r /$$ -u /ess/components/ext_1 | jq
fims_send -m get -r /$$ -u /ess/full/sys/status | jq




    to view the whole amap 
fims_send -m get -r /$$ -u /ess/amap | jq
fims_send -m get -r /$$ -u /ess/amap/control | jq 
    (with objects)

    to view the values in each rack
fims_send -m get -r /$$ -u /ess/control | jq
    add /rack_x_y to look at a specific rack
    add /field to look at specific value (fims_send -m get -r /$$ -u /ess/control/rack_0/max_charge_capacity | jq for example)


    running setup_demo creates an amap of racks/modules with all zero values
    this will set values to the amap
fims_send -m set -u /ess/control/rack_X_Y/field_to_be_set 15
    only fields rn are charge_capacity, charge_current, max_charge_capacity, & soc



use /functional_ess/ scripts to runDataMaps


*/
