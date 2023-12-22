


python3 DataMeshServer.py  345


python3 pyclient.py -mget -u/mysys -b'{"type":"bms_master", "data":"data for bms2"}'
{"ess": {"ess_1": {"bms": {"bms_2": {"type": "bms_master", "data": "data for bms2"}}}}}
python3 pyclient.py -mget -u/mysys -b'{"type":"bms_master", "data2":"databms2"}'
{"ess": {"ess_1": {"bms": {"bms_2": {"type": "bms_master", "data": "data for bms2"}}}}}
python3 pyclient.py -mget -u/mysys -b'{"type":"bms_master", "data":"data for bms2"}'
{"ess": {"ess_1": {"bms": {"bms_2": {"type": "bms_master", "data": "data for bms2"}}}}}
python3 pyclient.py -mset -u/mysys/ess/ess_1/bms/bms_2 -b'{"type":"bms_master", "data2":"databms2"}'
set ok
python3 pyclient.py -mget -u/mysys -b'{"type":"bms_master", "data":"data for bms2"}'
{"ess": {"ess_1": {"bms": {"bms_2": {"type": "bms_master", "data": "data for bms2", "data2": "databms2"}}}}}