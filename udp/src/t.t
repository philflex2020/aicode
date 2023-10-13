# BMS status variables
/usr/local/bin/fims_send -m set -r /$$ -u /ess/status/bms  '
{
    "DCClosed": {
        "value": 0
    },
    "FaultCnt": 0,
    "IsFaulted": {
        "enable": "/config/bms:enable",
        "expression": "{1} or {2} > 0",
        "numVars": 2,
        "useExpr": true,
        "value": false,
        "variable1": "/status/bms:SystemFault",
        "variable2": "/status/bms:TotalFaultCnt"
    },
    "SystemFault": false,
    "TotalFaultCnt": {
        "enable": "/config/bms:enable",
        "numVars": 2,
        "operation": "+",
        "value": 0,
        "variable1": "bms:TotalFaultCnt",
        "variable2": "/status/bms:FaultCnt"
    },
    "Status": "INIT"
} '
