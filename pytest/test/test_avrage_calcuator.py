#pytest script (test_average_calculator.py):

#python
#Copy code

import pytest

# assume running form the root dir
import sys
sys.path.append('./src')

from average_calculator import calculate_average

def test_calculate_average():
    numbers = [1, 2, 3, 4, 5]
    expected_average = 3

    result = calculate_average(numbers)
    assert result == pytest.approx(expected_average)

def test_calculate_average_empty_list():
    numbers = []

    with pytest.raises(ValueError):
        calculate_average(numbers)

def test_calculate_average_within_range():
    numbers = [10, 20, 30, 40, 50]
    expected_average = 30
    tolerance = 1e-6

    result = calculate_average(numbers)
    assert result >= expected_average - tolerance
    assert result <= expected_average + tolerance
#Make sure to save the first script as average_calculator.py and the second script as test_average_calculator.py in the same directory.

#To run the pytest script, execute the following command:

#bash
#Copy code
#pytest test_average_calculator.py
#Make sure you have the pytest package installed, which you can install using pip:
