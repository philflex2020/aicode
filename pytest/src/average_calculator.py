#Python script (average_calculator.py):

def calculate_average(numbers):
    if not numbers:
        raise ValueError("The list is empty.")

    total = sum(numbers)
    average = total / len(numbers)
    return average