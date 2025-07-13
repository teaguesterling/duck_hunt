# Complex example with multiple types of issues
from typing import Optional, List, Union
import   json,   os,   sys

# Badly formatted function
def    badly_formatted(   a:int,b:str,c:Optional[List[str]]=None   )   ->   Union[str,int]:
    if c   is  None:
        c=[]
    result = a+len(b)
    for     item     in     c:
        result+=len(item)
    return   result

def undefined_variable_error():
    x = undefined_var  # NameError

def attribute_error_example():
    s = "hello"
    return s.nonexistent_method()  # AttributeError

def    function_with_unused_import():
    # os is imported but not used
    return   json.dumps({"test":True})

class   MyClass:
    def   __init__(self,   value:int):
        self.value=value
    
    def   method_with_wrong_return_type(self)->str:
        return   self.value   # Should return str, not int
    
    def   method_with_type_mismatch(self,   other:   'MyClass'   )   ->   int:
        return   self.value   +   other.nonexistent_attr   # Attribute doesn't exist

# Function call with wrong argument types
result = badly_formatted("not_an_int", 42, ["a", "b"])  # Wrong types

# Missing type annotations
def untyped_function(x, y):
    return x + y

# Type annotation inconsistency  
def inconsistent_types(items: List[str]) -> List[int]:
    return items  # Wrong return type