Please document this
ess_controller AutoLoad Options

p. wilshire
08_23_2023

Ess_controller nrnmally loads its config files  using the dbi database interface.
The std_config option will use a series of files saves on disk to preload standard coniguratoins.

The ess_contrller config loader ia a very flexible mechanism to define and load its configuration files and expand template files.

This revision allows the ess_controller loader to load the standard config files as needed and then revert to the dbi interface to load the project specific files.

we define an assetVar 
/config/autoLoad:enable 

If this is set to "true"  then the config file loaders will get the config file data from known locations in the file system rather than from dbi.

The functions 
handleCfile
handleTmpl
handleFfile

will still be used but the body data will come from the std_config files and not dbi.
One of the config options in the ess_final ( the last file to be loaded in this config phase) will turn off the /config/autoLoad:enable flag.



The ess_init config file can request multiple load file sequences. one sequence can be used as the std_config load and a second sequence used as the project load.

the final file to be loaded in the sdt_config load sequence can be used as a file request can the second project load sequence.
The project load sequence will not progress unitl the ess_final file is loaded as part of the second load sequence.

```
{
    "amname": "ess",
    "/config/load": {       
     
       "ess_controller": {
          "value":false,
          "file":"ess_controller",
          "aname":"ess",
          "final":"ess_std_final",
          "new_options":[
             { "load":"std_bms_manager",             "aname":"bms",  "pname":"ess" },
             { "load":"std_bms_modbus_data",         "aname":"bms",  "pname":"ess" },
             { "load":"std_bms_controls",            "aname":"bms",  "pname":"ess" },

             { "file":"std_pcs_manager",             "aname":"pcs",  "pname":"ess" },
             { "file":"std_pcs_modbus_data",         "aname":"pcs",  "pname":"ess" },
             { "file":"std_pcs_controls",            "aname":"pcs",  "pname":"ess" }
          ]
        },
        "ess_project": {
            "value":false,
            "file":"ess_project",
            "aname":"ess",
            "final":"ess_project_final",
            "new_options":[
               { "file":"ess_std_final"},
               { "load":"lg_bms_manager",             "aname":"bms",  "pname":"ess" },
               { "load":"lg_bms_modbus_data",         "aname":"bms",  "pname":"ess" },
               { "load":"lg_bms_controls",            "aname":"bms",  "pname":"ess" },
  
               { "file":"lg_pcs_manager",             "aname":"pcs",  "pname":"ess" },
               { "file":"lg_pcs_modbus_data",         "aname":"pcs",  "pname":"ess" },
               { "file":"lg_pcs_controls",            "aname":"pcs",  "pname":"ess" }
            ]
          }
      }

    } 
    ```

The second concept to consider is the revisions required to the std_config file templates specs ( from,to, step, mult, add)
This are defined in the std_config files for bms.pcs etc systems.

the ess_controller introduces an override for these tempplate specs in the form of rangeVars.
These can be defined in the system in thie intial config file

```
{
    "/config/rangeVar": {
        "pcs" { "value" false, "from":1 , "to":3, "step"1"},
        "bms" { "value" false, "from":1 , "to":6, "step"1"},

    }
}
```

in the template file the addition of the optional rangevar will , if found, override the built in spec for the template expansion.

```
{
    "/config/load": {       
      "risen_bms_manager": {
          "value":false,
          "file":"risen_bms_manager",
          "pname":"ess",
          "aname":"bms",
          "options":[
            {
               "tmpl":"risen_bms_template",  "pname":"bms", "amname":"##RACK_ID##",
                "from":1, "to":18, 
                "rangeVar":"/config/rangeVar:bms",
                "reps":[
                        {"replace":"##RACK_ID##",   "with":"rack_{:02d}"},
                        {"replace":"##RACK_NUM##",  "with":"{:02d}"},
                        {"replace": "##AC_1_ID##",  "with": "hvac_{:02d}", "mult":2         },
                        {"replace": "##AC_1_NUM##", "with": "{:02d}",      "mult":2         },
                        {"replace": "##AC_2_ID##",  "with": "hvac_{:02d}", "mult":2, "add":1},
                        {"replace": "##AC_2_NUM##", "with": "{:02d}",      "mult":2, "add":1}
                    ]
                }
            ]
      }
    }
}
```

