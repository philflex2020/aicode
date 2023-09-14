Sure thing! Here's the revised directory structure based on your update:

```
test/
|-- test.json 
|-- systems/
    |-- DNP3/
        |-- topics/
            |-- topic1/
                |-- desc.json
                |-- actions.json
                |-- steps/
                    |-- step1/
                        |-- desc.json
                        |-- actions.json
                    |-- step2/
                        |-- desc.json
                        |-- actions.json
            |-- topic2/
                |-- desc.json
                |-- actions.json
                |-- steps/
                    |-- step1/
                        |-- desc.json
                        |-- actions.json
                    |-- step2/
                        |-- desc.json
                        |-- actions.json
    |-- Modbus/
        // Similar structure as DNP3, with its own topics and steps
    |-- Fims/
        // Similar structure as DNP3, with its own topics and steps
    |-- Ess/
        // Similar structure as DNP3, with its own topics and steps
```

- **test.json** describes the main test.
- **systems/** is the new directory that categorizes tests by system types like DNP3, Modbus, etc.
    - Each system (e.g., DNP3, Modbus) will have its own directory.
        - Inside each system directory, there are:
            - **topics/** directory which houses individual topics.
                - For each topic, there is:
                    - **desc.json**: Describes the topic.
                    - **actions.json**: Contains actions related to the topic.
                    - **steps/** directory which contains individual step directories.
                        - Inside each step directory:
                            - **desc.json**: Describes the step.
                            - **actions.json**: Contains actions related to the step.

Let me know if this now aligns with your vision or if any other adjustments are needed!