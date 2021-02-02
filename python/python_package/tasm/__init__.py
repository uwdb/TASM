# TODO: actually install this library.
from tasm._tasm import *

def numpy_array(self):
    return self.array().reshape(self.height(), self.width(), -1)[:,:,:3]

Image.numpy_array = numpy_array