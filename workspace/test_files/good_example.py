"""
A well-formatted Python file with proper types and formatting.
"""

from typing import List, Optional


def well_formatted_function(x: int, y: str, z: Optional[List[str]] = None) -> int:
    """A properly formatted function with type hints."""
    if z is None:
        z = []

    result = x + len(y)
    for item in z:
        result += len(item)

    return result


class MyClass:
    """A well-formatted class."""

    def __init__(self, value: int) -> None:
        """Initialize the class."""
        self.value = value

    def get_value(self) -> int:
        """Return the stored value."""
        return self.value

    def add_to_value(self, other: int) -> int:
        """Add a value to the stored value."""
        return self.value + other


# Proper function call with correct types
result = well_formatted_function(10, "hello", ["world", "test"])
print(f"Result: {result}")
