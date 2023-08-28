#pragma once

#include <string>
#include <unordered_map>

// dataMaps.h
#include "fims/libfims.h"
#include "scheduler.h"
#include "channel.h"

enum class Type {
    INT,
    DOUBLE,
    INT16,
    UINT16,
    INT32,
    UINT32,
    BOOL,
    UNKNOWN
};

struct threadDataItem {
    Type type;
    std::string name;
    int offset;
    int size;
};

using threadDataItems = std::unordered_map<std::string, threadDataItem>;
using threadDataFields = std::vector<threadDataItem>;
//void EssThread(ess_thread_inst* ess);


struct threadDataMap {
    std::string name;
    //ResultFields fields;
    threadDataItems items;
    //int offset;
    //int size;
};

using threadDataMaps = std::map<std::string, threadDataMap>;


class ess_thread_inst;
typedef void (*EssThreadFun)(ess_thread_inst*);

class ess_thread_class {
    public:
    ess_thread_class(char *name):name(name) {
    }
    ~ess_thread_class() {
    }
    bool init;
    std::string name;
    std::map<std::string,void*>threadMap;

    struct DataMap;
    std::unordered_map<std::string, DataMap*> dataMaps;  
    std::unordered_map<std::string, ess_thread_inst *> threads;
    std::unordered_map<int, void*> datasigs;  
    threadDataMaps tdataMaps;// = std::map<std::string, threadDataMap>;

    EssThreadFun ess_thread_fun;

    channel <schedItem*>* as_reqChan;
    channel <int>* as_wakeChan;
    
};


class ess_thread_inst {
    public:
    ess_thread_inst(char *name):name(name) {
    }
    ~ess_thread_inst() {
    }
    ess_thread_class *thread_class;
    bool init;

    double *base_timep;

    std::thread id;
    std::string name;

    // this is our incoming event channel
    channel <int> wakeChan;

    // this is the sched item we''l use it will be a non delete item

    // TODO add a param to the scheduri to cause it to NOT be deleted.
    schedItem *sItem;
    // we are going to run a fast sched option and we wont delete the schedItem
    
    
    // These things are used to trigger a fast schedule operation  
    // they are all set up when the thread is setup.
    // this is the name of the sched item.
    // char *schedid;
    // // this is the name of the i/ouri
    // assetVar *scheduri;
    // assetVar* schedAv;
    // assetVar* targAv;  //note that the thread cannot access the targav.
    // asset_manager *am;
    
    // these are the names of the functions we'll use to trigger the setup, input and output operations 
    // they have to be registered when the thread is set up.
    // everything goes in here
    std::map<std::string,void*>threadMap;

    // data  init name
    //void *initfcn;
    // data in names
    //void *inputfcn;
    //data out name
    //void *outputfcn;


    // these talk to the scheduler , they could be obtained from the am 
    channel <schedItem*>* as_reqChan;
    channel <int>* as_wakeChan;


    int retSig;
    int wakeup;
    int state;
    int running;
    double delay;
    std::unordered_map<int, void*> datasigs;  

};

typedef void (*ThreadFuncType)(ess_thread_inst*, int);


