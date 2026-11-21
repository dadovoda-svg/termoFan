"""Keep the installation's encryption key across clean builds."""
from pathlib import Path
import os
import secrets
import tempfile

Import("env")
root = Path(env.subst("$PROJECT_DIR"))
key_dir = root / ".termofan"
key_dir.mkdir(mode=0o700, exist_ok=True)
header = key_dir / "ConfigKey.h"
if not header.exists():
    with tempfile.NamedTemporaryFile(mode="w", dir=key_dir, delete=False) as temp:
        values = ", ".join(str(value) for value in secrets.token_bytes(32))
        temp.write("static constexpr unsigned char configKey[32] = {" + values + "};\n")
    try:
        os.link(temp.name, header)
    except FileExistsError:
        pass
    finally:
        os.unlink(temp.name)
env.Append(CPPPATH=[str(key_dir)])
