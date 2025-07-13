# Example Python file with formatting and type issues
import sys
def bad_function(    x,y   ):
    z=x+y
    return z

def type_error_function(name: str) -> int:
    return "not an int"  # Type error

def another_function():
    result = bad_function(1, "two")  # Type error: str + int
    print(   result   )

class   BadClass:
    def __init__(self,value):
        self.value=value
    
    def method(self)->str:
        return self.value