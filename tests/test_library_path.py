import os
import sys
import unittest
from unittest import mock

import paths
sys.path.insert(0, str(paths.ROOT / 'launcher'))
import vw_launcher  # noqa: E402


class RestoreLibraryPath(unittest.TestCase):
    """The packaged Linux launcher hands its children the library path it was started with,
    as PyInstaller's bootloader leaves it (measured on the v1.0.0 AppImage)."""

    def restored(self, env):
        with mock.patch.object(vw_launcher, 'FROZEN', True), mock.patch.object(vw_launcher, 'WINDOWS', False), \
                mock.patch.dict(os.environ, env, clear=True):
            vw_launcher.restore_library_path()
            return {key: os.environ.get(key) for key in ('LD_LIBRARY_PATH', 'LD_LIBRARY_PATH_ORIG')}

    def test_started_without_a_library_path(self):
        self.assertEqual(self.restored({'LD_LIBRARY_PATH': '/app/_internal'}),
                         {'LD_LIBRARY_PATH': None, 'LD_LIBRARY_PATH_ORIG': None})

    def test_started_with_a_library_path(self):
        self.assertEqual(self.restored({'LD_LIBRARY_PATH': '/app/_internal:/steam/lib',
                                        'LD_LIBRARY_PATH_ORIG': '/steam/lib'}),
                         {'LD_LIBRARY_PATH': '/steam/lib', 'LD_LIBRARY_PATH_ORIG': None})


if __name__ == '__main__':
    unittest.main()
