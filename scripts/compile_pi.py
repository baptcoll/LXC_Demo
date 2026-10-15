#!/usr/bin/env python3
"""Compiler sur la Pi sans exécution ; seuls les exécutables restent en sortie."""
from pathlib import Path
import shlex
import subprocess
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[1]
HOST = "pi@192.168.1.14"
DESTINATION = "/home/pi/Test_LXC"


def main():
    sources = sorted((ROOT / "common").glob("*.c"))
    sources += sorted((ROOT / "common").glob("*.h"))
    sources += [ROOT / name / "main.c" for name in ("Ext", "Intf_Ext", "Interner")]
    for source in sources:
        if not source.is_file():
            raise FileNotFoundError(source)

    remote = '''set -eu
cd /home/pi/Test_LXC
task_build=$(mktemp -d /home/pi/Test_LXC/.compile-XXXXXXXX)
trap 'rm -rf -- "$task_build"' EXIT HUP INT TERM
tar -xf - -C "$task_build"
cd "$task_build"
for source in common/*.c; do
    gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -Icommon -c "$source" -o "$(basename "$source" .c).o"
done
ar rcs libcommon.a ./*.o
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -Icommon Ext/main.c libcommon.a -lrt -o ext
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -Icommon Intf_Ext/main.c libcommon.a -lrt -o intf_ext
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -Icommon Interner/main.c libcommon.a -lrt -o interner
mv -- ext intf_ext interner /home/pi/Test_LXC/
file /home/pi/Test_LXC/ext /home/pi/Test_LXC/intf_ext /home/pi/Test_LXC/interner
'''
    with tempfile.TemporaryFile() as archive:
        with tarfile.open(fileobj=archive, mode="w") as tar:
            for source in sources:
                tar.add(source, arcname=str(source.relative_to(ROOT)))
        archive.seek(0)
        subprocess.run(
            ["ssh", "-o", "BatchMode=yes", "-o", "ConnectTimeout=10", HOST,
             "sh -c " + shlex.quote(remote)], stdin=archive, check=True,
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
