
import enum
from pathlib import Path

class GenFileType(str, enum.Enum):
    JAVASCRIPT = "js"
    RESTRUCTURED_TEXT = "rst"

class GeneratedDir():
    def __init__(self, root, file_type):
        if not isinstance(file_type, GenFileType):
            raise TypeError("GeneratedDir file_type not GenFileType type")

        if not isinstance(root, (Path, str)):
            raise TypeError("GeneratedDir root not pathlib.Path or string type")

        self._type = file_type
        self._path = Path(root) / "_static" / "generated" / self._type.value
        self._path.mkdir(parents=True, exist_ok=True)

    @property
    def path(self):
        return self._path

    def __truediv__(self, rhs):
        return self._path / rhs

def generate_javascript(root, filename):
    gen_dir = GeneratedDir(root, GenFileType.JAVASCRIPT)
    return gen_dir / filename

def generate_rst(root, filename):
    gen_dir = GeneratedDir(root, GenFileType.RESTRUCTURED_TEXT)
    return gen_dir / filename
