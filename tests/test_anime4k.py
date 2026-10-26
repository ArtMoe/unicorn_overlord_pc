import sys
import tempfile
import unittest
from pathlib import Path

import paths
sys.path.insert(0, str(paths.ROOT / 'tools'))
import anime4k  # noqa: E402


class Anime4kShaders(unittest.TestCase):
    """The compute shaders and pass table are generated from the mpv shaders (#32)."""

    def test_committed_files_are_the_generator_output(self):
        with tempfile.TemporaryDirectory() as temp:
            out, table = Path(temp) / 'anime4k', Path(temp) / 'anime4k_models.h'
            self.assertEqual(anime4k.main(out, table), 26)
            self.assertEqual(table.read_text(), anime4k.TABLE.read_text(),
                             'run python3 tools/anime4k.py')
            generated = sorted(p.name for p in out.iterdir())
            self.assertEqual(generated, sorted(p.name for p in anime4k.OUT.iterdir()))
            for name in generated:
                self.assertEqual((out / name).read_text(), (anime4k.OUT / name).read_text(), name)

    def test_size_expressions(self):
        scales = {'MAIN': 1, 'conv2d_last_tf': 1}
        self.assertEqual(anime4k.scale_of('MAIN.w', scales), 1)
        self.assertEqual(anime4k.scale_of('conv2d_last_tf.w 2 *', scales), 2)

    def test_upscale_reads_the_model_input_and_doubles_it(self):
        table = anime4k.TABLE.read_text()
        self.assertIn('{ANIME4K_UPSCALE_S_4_COMP, 2, 2, {-1, 3,', table)
        self.assertIn('{ANIME4K_RESTORE_S_0_COMP, 1, 1, {-1,', table)


if __name__ == '__main__':
    unittest.main()
