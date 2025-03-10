found out what a pack_soc (soh) etc is ...  well at least for the CAN system
 
ASW/src/CAN0_PC_Analysis.c
 
The can system sends out 120  bytes of data in 19 can messages  each with 6 data bytes
the packnum selects the cell starting offset: k
 
k = (packNum - 1) * 120;
so given a packnum within a certain range 
if ((packNum >0) && (packNum <= ((MAX_BAT_NUM + 119)/120)))
you'll get 19 can messages each with 6 values.
 
 
However, the  sbmu modbus inputs refer to an array  of 40 "pack soc" numbers.
 
I can only assume that this is the same name (Pack) used to refer to Module SOC.
 
The GOLD design did not start with a data definition layout and, as such , names like this tend to be arbitrary and different.
 
about 30% of my time in the last week has been spent trying to understand their layout and find the  shared memory locations for the variables to make sure I can force values and test results
 
for example I force the data into shared memory (on rack 7 which is not connected)
 
{"action":"set rtos:sm16:7:0x5404 1670 23",  "desc": "set acc single_charge_cap to 1670 23  ",    "data":[1670, 23]},
 
and then check the aggregated result seen by the sbmu
 
Note: the values have been altered by the Gold Modbus code.
 
{"action":"get sbmu:input:7:Accumulated_single_charge_capacity 2",   "desc": "get single_charge_cap mb   ",  "data":[2,26281]},
 
for this to work, rack 7 has to be forced online in test mode and the data update from the rbms disabled ( to stop forced shared memory being overwritten by the incoming data).
I can then set values into rack 7 shared memory and use the Gold Access routines to check the values.
the sets and gets use the websocket (qt) app for the tests but I can also test the modbus interface on the sbmu 
 
getmb sbmu:input:21143 2  "data":[2, 26281]}
 
 in this case the address of rack 0 Accumulated_single_charge_capacity is 143
the extra offset for rack 7 data (when using inputs ) is 21000
 
 I will finish the complete test scan of the aggregated sbmu inputs by the end of the day tomorrow.
 I think it has taken me about 100 hours to get this complete. I started April 7th.
The holding is mapped directly with no accumulated data.
the bits ( discrete inputs) have been coded but using an older technique. 
I will port that code to this mapping/test  system. ( about 24 hours)
 
two json files hold the mapping and test information.
data_definition.json
test_definition.json
 
The test code is not yet checked into fractal bms space. It will not be part of the build for deployment ( It could be if we wanted built in test capability)  
(It is, currently,  maintained in a private github repo)
I will check it in to a fractalbms repo during this week.
 
 