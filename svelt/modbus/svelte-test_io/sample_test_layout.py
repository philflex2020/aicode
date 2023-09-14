import os
import json

# Directory structure blueprint
systems_data = [
    {'name': 'DNP3', 'description': 'Distributed Network Protocol 3', 'version': '1.0'},
    {'name': 'Modbus', 'description': 'Modbus Communication Protocol', 'version': '1.0'},
    {'name': 'Fims', 'description': 'Fims Protocol', 'version': '1.0'},
    {'name': 'Ess', 'description': 'Ess Controller', 'version': '1.0'},
    {'name': 'GoMetrics', 'description': 'Go_metrics', 'version': '1.0'}
]

topics_data = [
    {'name': 'topic1', 'description': 'Description for topic1', 'version': '1.0'},
    {'name': 'topic2', 'description': 'Description for topic2', 'version': '1.0'}
]

steps_data = [
    {'name': 'step1', 'description': 'Description for step1', 'version': '1.0'},
    {'name': 'step2', 'description': 'Description for step2', 'version': '1.0'}
]

actions_data = [
    {'name': 'action1', 'description': 'Description for action1', 'ip':'127.0.0.1', 'function':'function1','args':'args1','expected':'result1', 'version': '1.0'},
    {'name': 'action2', 'description': 'Description for action2', 'ip':'172.17.0.4', 'function':'function2','args':'args2','expected':'result2', 'version': '1.0'}
]

# Create root directory and systems.json
os.makedirs('test/systems', exist_ok=True)

with open('test/systems.json', 'w') as systems_file:
    json.dump({"systems": systems_data}, systems_file, indent=4)

# Create systems and their related directories
for system_info in systems_data:
    system_name = system_info['name']
    system_path = os.path.join('test/systems', system_name)

    # Create topics directory and topics.json
    os.makedirs(os.path.join(system_path, 'topics'), exist_ok=True)
    with open(os.path.join(system_path, 'topics.json'), 'w') as topics_file:
        json.dump({"topics": topics_data}, topics_file, indent=4)

    for topic_info in topics_data:
        topic_name = topic_info['name']
        topic_path = os.path.join(system_path, 'topics', topic_name)

        # Create directory for each topic
        os.makedirs(topic_path, exist_ok=True)

        # Create desc.json and actions.json for each topic
        # with open(os.path.join(topic_path, 'desc.json'), 'w') as desc_file:
        #     json.dump({"description": topic_info['description'], "version": topic_info['version']}, desc_file, indent=4)
        with open(os.path.join(topic_path, 'actions.json'), 'w') as actions_file:
            json.dump({"actions": actions_data}, actions_file, indent=4)

        # Create steps directory and steps.json
        os.makedirs(os.path.join(topic_path, 'steps'), exist_ok=True)
        with open(os.path.join(topic_path, 'steps.json'), 'w') as steps_file:
            json.dump({"steps": steps_data}, steps_file, indent=4)

        for step_info in steps_data:
            step_name = step_info['name']
            step_path = os.path.join(topic_path, 'steps', step_name)

            # Create directory for each step
            os.makedirs(step_path, exist_ok=True)

            # Create desc.json and actions.json for each step
            # with open(os.path.join(step_path, 'desc.json'), 'w') as desc_file:
            #     json.dump({"description": step_info['description'], "version": step_info['version']}, desc_file, indent=4)
            with open(os.path.join(step_path, 'actions.json'), 'w') as actions_file:
                json.dump({"actions": actions_data}, actions_file, indent=4)
