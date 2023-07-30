**Proposal: AssetManager Enhancements for Ess_controller**

**Introduction:**

The current Ess_controller system has been operational for two years and has been successful in controlling various assets, such as Battery Managers and Power Converters, using assetVars and asset managers. However, there is a need to introduce new functionality to allow Asset Managers to execute raw C++ functions using data structures. This proposal aims to enhance the Ess_controller by introducing the concept of "datamaps." Datamaps will contain C++ data structures and functions to map data values from the existing assetVar system into these data structs. The proposal also includes the implementation of a messaging system for seamless communication between the AssetControl software (written in raw C++ code) and the Ess_controller.

**Enhancements:**

1. **Introduction of Datamaps:**

   Datamaps will be introduced as named data spaces containing C++ data structures. Each Asset Manager will have its own named list of datamaps, and these datamaps will be used to execute raw C++ functions using the relevant data. A datamap will include the following components:
   
   - **Name:** A unique identifier for the datamap within the Asset Manager.
   - **Size:** The size of the data structure contained in the datamap.
   - **Data Area:** A void pointer to the memory area where the data structure will be stored.
   - **Function List:** A named list of void pointers representing functions that translate data to and from the data mapped by the Asset Manager's Amap.
   
2. **Data Structure Mapping:**

   The datamaps will enable the Ess_controller to execute C++ functions that require specific data structures. By using the function list provided in the datamap, the Ess_controller will map the data values to and from the existing assetVar system to the appropriate data structure required by the C++ functions.
   
3. **Asset Manager Integration:**

   Each Asset Manager will be associated with a list of named  datamaps. Theese datamaps will hold all the relevant information required for executing C++ functions for that particular Asset Manager. This allows different Asset Managers to use the same control code while having their own data mappings to assetVars.
   
4. **Messaging System:**

   To facilitate communication between the AssetControl software and the Ess_controller, the scheduler  messaging system will be used. The AssetControl software, written in raw C++ code, will send messages to the Ess_controller scheduling channels. These channels will act as "go channels" to trigger transfers of data into and out of the new data areas (datamaps) associated with the Asset Managers.

**Benefits:**

1. **Flexibility and Reusability:**

   The introduction of datamaps and C++ functions allows for greater flexibility and reusability of the control code. Different assets with unique data mappings can use the same control code, improving code maintenance and reducing redundancy.

2. **Enhanced Control Capabilities:**

   By enabling Asset Managers to execute raw C++ functions, the Ess_controller gains the capability to perform more sophisticated control operations, leading to enhanced overall system performance.

3. **Modularity:**

   The implementation of datamaps and messaging channels promotes modularity in the system. Asset Managers can be updated or added independently without affecting the core functionality of the Ess_controller.

**Implementation Plan:**

1. Conduct a thorough analysis of the existing Ess_controller system to identify areas where the introduction of datamaps and C++ functions can be most beneficial.

2. Design the structure of datamaps, including the definition of data structures and function lists.

3. Modify the Asset Manager class to include the  datamap mapping, associating each Asset Manager with its specific list of datamap.

4. Integrate with the  messaging system for communication between the AssetControl software and the Ess_controller, using channels to trigger data transfers.

5. Develop mechanisms for converting data between assetVars and the datamap's data structures.

6. Conduct rigorous testing to ensure the enhanced Ess_controller operates flawlessly, and the integration with the AssetControl software functions as expected.

7. Provide comprehensive documentation to assist users and future developers in understanding the new enhancements.

**Conclusion:**

The proposed enhancements to the Ess_controller through the introduction of datamaps and raw C++ function execution offer increased flexibility, modularity, and enhanced control capabilities. By empowering Asset Managers with the ability to utilize unique data mappings and execute C++ functions, the system will be better equipped to handle complex control operations effectively. The implementation plan will ensure a smooth integration of these enhancements while maintaining the reliability and stability of the existing system.

**Additional Enhancement: Named Lists of Datamaps for Asset Managers**

In addition to the proposed enhancements, each Asset Manager will have its own named list of datamaps. This additional feature will further enhance the modularity and customization capabilities of the Ess_controller system.

**Named Lists of Datamaps:**

1. **Uniqueness for Each Asset Manager:**

   With the introduction of named lists of datamaps, each Asset Manager will have its own dedicated collection of datamaps. This ensures that the datamaps are specific to the requirements and functionalities of each Asset Manager.

2. **Easy Access and Management:**

   The named lists of datamaps will provide a structured and organized approach to accessing and managing datamaps within an Asset Manager. Developers can easily reference specific datamaps by their unique names when executing control functions.

3. **Facilitates Expansion:**

   By having named lists of datamaps, the Ess_controller system becomes more scalable and adaptable. New datamaps can be easily added to an Asset Manager's named list to accommodate future requirements without disrupting the existing setup.

4. **Hierarchical Hierarchy Support:**

   The hierarchical nature of the Ess_controller system is maintained through the named lists of datamaps. Each Asset Manager in the system, such as BMS, PCS, etc., will have its own set of datamaps, allowing for a clear representation of the asset hierarchy.

5. **Encapsulation and Encouragement of Best Practices:**

   Encapsulating datamaps within the named lists promotes encapsulation, making it easier to manage and update datamaps for each Asset Manager independently. This encourages the adoption of best practices and avoids unintended interference between different Asset Managers.

**Implementation Plan:**

1. Extend the Asset Manager class to include a named list data structure for holding datamaps.

2. Implement functions to add, retrieve, and manage datamaps within the named lists for each Asset Manager.

3. Ensure that the messaging system can communicate with the appropriate datamaps within the named list.

4. Update the documentation to reflect the new feature, providing clear guidelines on how developers can utilize the named lists of datamaps.

**Conclusion:**

By introducing named lists of datamaps for each Asset Manager, the Ess_controller system gains an additional layer of customization and flexibility. This enhancement ensures that each Asset Manager can have its own set of datamaps, allowing for precise control and data management specific to their unique requirements. The implementation plan will seamlessly integrate this feature into the existing proposal, ultimately resulting in a more robust and adaptable Ess_controller system.

