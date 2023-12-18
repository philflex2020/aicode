import time

from pyutils import get_store, get_last, start_thread

# Define another thread function for example
def bms_master(arg1):
    print(f" Bms_master {arg1}")
    myStore = get_store(arg1)
    myStore["bms"] = {}
    num_units =  myStore.get("num_units")
    print(f" num_units {num_units}")
    for x in range(num_units):
        #arg2 = arg1+"/bms"
        arg3=f"{arg1}/bms/bms_{x}"
        print(f" Bms_master {arg3}")
        start_thread("bms_unit", arg3)


def bms_unit(arg1):
    myStore = get_store(arg1)
    myStore["name"] = get_last(arg1)
    #print(f" bms myStore {myStore}")
    myStore["uri"] = arg1
    myStore["status"] = {}
    myStore["commands"] = {}
    myStore["status"]["state"] = "off"
    myStore["commands"]["command"] = "off"
    myStore["status"]["SOC"] = 100
    myStore["status"]["capacity"] = 40000
    myStore["max_capacity"] = 40000
    myStore["status"]["power"] = 0
    myStore["maxpower"] = 400
    myStore["commands"]["powerRequest"] = 0



    count = 0
    if "count" not in myStore:
        myStore["count"] = 0
    else:
        count = myStore["count"]

    while True:  # 'True' should be capitalized
        mycount = myStore.get("mycount", 0)
        count = myStore["count"]
        count += 1
        myStore["count"] = count
        myname = myStore["name"]
        mycap = myStore["status"]["capacity"]
        mystate = myStore["status"]["state"]
        mycommand = myStore["commands"]["command"]
        prequest = myStore["commands"]["powerRequest"]
        if prequest > myStore["maxpower"]:
            prequest = myStore["maxpower"]
        elif prequest < -myStore["maxpower"]:
            prequest = -myStore["maxpower"]
        if mystate in ["off" ,"charge", "discharge"] :
            if mycommand == "standby": 
                print(f" changing state to standby")
                myStore["status"]["power"] = 0
                myStore["status"]["state"] = "standby"
        elif mystate in ["standby" ] :
            if mycommand == "charge":
                print(f" changing state to charge")
                myStore["status"]["state"] = "charge"
            elif mycommand == "discharge": 
                print(f" changing state to discharge")
                myStore["status"]["state"] = "discharge"
        mystate = myStore["status"]["state"]
        if mystate in ["charge"] :
            if mycap <  myStore["max_capacity"] and prequest > 0:
                myStore["status"]["capacity"] += prequest
                myStore["status"]["power"] = prequest
            else:
                myStore["status"]["power"] = 0

        if mystate in ["discharge"] :
            if mycap >  0 and prequest < 0:
                myStore["status"]["capacity"] += prequest
                myStore["status"]["power"] = prequest
            else:
                myStore["status"]["power"] = 0


        print(f"             bms_running : {myname}  state  {mystate}  capacity {mycap}")
        time.sleep(5)
    # Implement bms_master logic here