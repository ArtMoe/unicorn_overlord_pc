import os
import tempfile
import unittest
from pathlib import Path

import paths
import sys
sys.path.insert(0, str(paths.ROOT / 'launcher'))
import vw_launcher  # noqa: E402


class InstallTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        root = Path(self.temp.name)
        self.source, self.target = root / 'CUSA36381', root / 'data' / 'game'
        (self.source / 'sce_sys').mkdir(parents=True)
        (self.source / 'eboot.bin').write_bytes(b'e' * 1000)
        (self.source / 'sce_sys' / 'param.sfo').write_bytes(b's' * 10)
        (self.source / 'Unicorn.CPK').write_bytes(b'c' * 20000)

    def tearDown(self):
        self.temp.cleanup()

    def test_installs_every_file_and_reports_progress(self):
        seen = []
        self.assertTrue(vw_launcher.install_files(self.source, self.target, lambda d, t, l: seen.append((d, t))))
        self.assertEqual((self.target / 'Unicorn.CPK').read_bytes(), b'c' * 20000)
        self.assertEqual((self.target / 'sce_sys' / 'param.sfo').read_bytes(), b's' * 10)
        self.assertEqual(seen[-1], (21010, 21010))
        self.assertFalse(self.target.with_name('game.partial').exists())

    def test_cancelled_install_leaves_no_game_folder_and_resumes(self):
        calls = iter([False, True])
        self.assertFalse(vw_launcher.install_files(self.source, self.target, cancelled=lambda: next(calls, True)))
        self.assertFalse(self.target.exists())
        self.assertTrue(vw_launcher.install_files(self.source, self.target))
        self.assertEqual(sorted(p.name for p in self.target.rglob('*') if p.is_file()),
                         ['Unicorn.CPK', 'eboot.bin', 'param.sfo'])

    def test_reinstall_replaces_the_previous_game(self):
        self.target.mkdir(parents=True)
        (self.target / 'old.bin').write_bytes(b'old')
        self.assertTrue(vw_launcher.install_files(self.source, self.target))
        self.assertFalse((self.target / 'old.bin').exists())


if __name__ == '__main__':
    unittest.main()
