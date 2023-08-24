### ess_controller AutoLoad Options Documentation

**Author:** p. wilshire  
**Date:** 08_23_2023

#### Overview
`ess_controller` typically loads its configuration files through the dbi database interface. A new revision now allows the loader to first load standard configuration files (via the `std_config` option) from disk and then revert to the dbi interface for project-specific files.

#### Key Concepts
1. **Asset Variable Definition**: The asset variable `/config/autoLoad:enable` can be set to `true`, which means that the configuration files will be fetched from known locations in the file system instead of dbi.
2. **Functions Handling**: Functions such as `handleCfile`, `handleTmpl`, and `handleFfile` will still be utilized but will gather data from the `std_config` files rather than dbi.
3. **Load Sequencing**: The `ess_init` config file allows for multiple load sequences. There can be a sequence for `std_config` load and another for project load. The project load sequence awaits the loading of the `ess_final` file as part of the second load sequence.
4. **Template Specifications**: Revisions to the `std_config` file templates specs are introduced through `rangeVars`, providing an override for the template specifications.

#### Detailed Configuration

##### Asset Variable
- **Path**: `/config/autoLoad:enable`
- **Values**: "true" or "false"

##### Load Sequencing
The structure contains two main sequences:
- `ess_controller` sequence
- `ess_project` sequence

Here is an example structure:
```json
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

##### Range Variable Configuration
Override specs for the template expansion using `rangeVar`. Example:
```json
{
    "/config/rangeVar": {
        "pcs": {"value": false, "from":1, "to":3, "step":1},
        "bms": {"value": false, "from":1, "to":6, "step":1}
    }
}
```

##### Template File Configuration
In the template file, the optional `rangeVar` can override the built-in spec for the template expansion. Example:
```json
{
    "/config/load": {
        "risen_bms_manager": {
            ...
            "options": [
                {
                    "tmpl": "risen_bms_template",
                    "rangeVar": "/config/rangeVar:bms",
                    ...
                }
            ]
        }
    }
}
```

### Conclusion
This revision to the `ess_controller` provides more flexibility in how configuration files are loaded and templates are expanded. By allowing for local file loading and customized templating, the system can be more easily configured to suit various project needs.

Please refer to the provided JSON examples for specific implementation details.