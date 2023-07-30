**Proposal: AssetManager Enhancements for Ess_controller**

**Introduction:**

The Ess_controller has been successfully controlling various assets, such as Battery Managers and Power Converters, using assetVars contained in a global map of objects sorted by string uris and string ids. These assetVars correspond to input and output data streams utilized by the controller to transfer data throughout the system. However, the need for additional functionality has arisen. Specifically, each Asset Manager may need to execute raw C++ functions using data structures. To address this requirement, we propose the introduction of "datamaps," named data spaces containing C++ structs, which will enable the execution of C++ functions using data values from the assetVar system. Additionally, each Asset Manager will have its own named list of datamaps, providing a structured and modular approach to control and data management.



**Enhancements:**

1. **Introduction of Datamaps:**

   Datamaps will be introduced as named data spaces containing C++ data structures. Each Asset Manager will have its own datamap, and these datamaps will be used to execute raw C++ functions using the relevant data. A datamap will include the following components:
   
   - **Name:** A unique identifier for the datamap within the Asset Manager.
   - **Size:** The size of the data structure contained in the datamap.
   - **Data Area:** A void pointer to the memory area where the data structure will be stored.
   - **Function List:** A named list of void pointers representing functions that translate data to and from the data mapped by the Asset Manager's Amap.
   - **Item List:** A list of Named Items referencng the Amap item name , the offset into the structure, size of the storage for the item and the data type.
   
2. **Data Structure Mapping:**

   The datamaps will enable the Ess_controller to execute C++ functions that require specific data structures. By using the function list provided in the datamap, the Ess_controller will map the data values from the existing assetVar system to the appropriate data structure required by the C++ functions.
   
3. **Asset Manager Integration:**

   Each Asset Manager can contain one or more  datamaps. Each datamap will hold all the relevant information required for executing C++ functions for that particular Asset Manager. This allows different Asset Managers to use the same control code while having their own data mappings to assetVars.

4. **Named Lists of Datamaps:**

   Each Asset Manager will have its own named list of datamaps. This ensures that the datamaps are specific to the requirements and functionalities of each Asset Manager. The named lists facilitate easy access, management, and expansion of datamaps while supporting the hierarchical structure of the Ess_controller system.

5. **Messaging System:**

   To facilitate communication between the AssetControl software and the Ess_controller, the scheduler  messaging system will be used. The AssetControl software, written in raw C++ code, will send messages to the Ess_controller scheduling channels. These channels will act as "go channels" to trigger execution of functions , in a thread safe manner, to enable transfer of data into and out of the new data areas (datamaps) required by the raw C++ code.

**Benefits:**

1. **Flexibility and Reusability:**

   The introduction of datamaps and C++ functions allows for greater flexibility and reusability of the control code. Different assets with unique data mappings can use the same control code, improving code maintenance and reducing redundancy.

2. **Enhanced Control Capabilities:**

   By enabling Asset Managers to execute raw C++ functions, the Ess_controller gains the capability to perform more sophisticated control operations, leading to enhanced overall system performance.

3. **Modularity:**

   The implementation of datamaps and messaging channels promotes modularity in the system. Asset Managers can be updated or added independently without affecting the core functionality of the Ess_controller.

4. **Hierarchical Hierarchy Support:**

   The named lists of datamaps ensure that each Asset Manager has its own dedicated collection of datamaps, maintaining the hierarchical nature of the Ess_controller system.

5. **Scalability:**

   The introduction of named lists of datamaps allows for easy expansion of the system, accommodating future requirements without disrupting the existing setup.

**Implementation Plan:**

1. Conduct a thorough analysis of the existing Ess_controller system to identify areas where the introduction of datamaps, named lists, and C++ functions can be most beneficial.

2. Design the structure of datamaps and named lists, including the definition of data structures and function lists.

3. Modify the Asset Manager class to include a named list data structure for holding datamaps and implement functions for adding, retrieving, and managing datamaps within the named list.

4. Update the messaging system to accommodate the communication between the AssetControl software and the appropriate datamaps within the named lists.

5. Develop mechanisms for converting data between assetVars and the datamap's data structures.

6. Conduct rigorous testing to ensure the enhanced Ess_controller operates flawlessly and the integration with the AssetControl software functions as expected.

7. Provide comprehensive documentation to assist users and future developers in understanding the new enhancements.

**Compatibility:**

The Ess_controller system was designed from day 1 to be extendable and have added enhancements. 
The functions concept, for example,  allows functions to be named in the config file, these function names refer to code segments conforming to a predefined format. A proposed feature would be to allow these functions to be presented as runtime loadable shared objects. The data map enhancements do not affect the current operation of the ess_controller. They are only needed if the asset manager data map system is required in future projects. The current config system will be extended to add the features required to implement these datamaps.


**Conclusion:**

By introducing named lists of datamaps and enabling raw C++ function execution, the proposed enhancements offer increased flexibility, modularity, and enhanced control capabilities to the Ess_controller system. Each Asset Manager having its own dedicated collection of datamaps allows for precise control and data management specific to their unique requirements. The implementation plan ensures a seamless integration of these features into the existing proposal, ultimately resulting in a more robust and adaptable Ess_controller system.
The existing functionality of the ess_controller will not be affected by this enhancement allowing complete backwards compatibility.
