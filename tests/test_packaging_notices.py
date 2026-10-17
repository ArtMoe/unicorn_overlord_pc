import importlib.util
from pathlib import Path
import tempfile
import unittest
from paths import ROOT

spec = importlib.util.spec_from_file_location('notices', ROOT / 'packaging/notices.py')
notices = importlib.util.module_from_spec(spec)
spec.loader.exec_module(notices)


class NoticeTests(unittest.TestCase):
    def test_missing_component_fails_instead_of_shipping_incomplete_notices(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            with self.assertRaises(FileNotFoundError):
                notices.collect_native(root / 'dist', root)

    def test_all_native_components_and_inline_notices_are_collected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            paths = list(notices.VENDORED.values())
            paths += [f'out/deps/build/{name}/LICENSE' for name in notices.STATIC]
            paths += ['packaging/licenses/Apache-2.0.txt',
                      'gpu/shadps4/video_core/host_shaders/fsr/ffx_a.h',
                      'gpu/shadps4/video_core/host_shaders/fsr/ffx_fsr1.h',
                      'gpu/third_party/gcn/include/gcn/si_ci_vi_merged_offset.h',
                      'gpu/third_party/gcn/include/gcn/si_ci_vi_merged_pm4_it_opcodes.h']
            for source in paths:
                path = root / source
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(f'original notice: {source}')
            destination = root / 'dist'
            notices.collect_native(destination, root)
            self.assertEqual((destination / 'LibAtrac9.txt').read_text(),
                             'original notice: third_party/LibAtrac9/LICENSE')
            self.assertTrue((destination / 'ffmpeg/LICENSE').is_file())
            self.assertTrue((destination / 'headers/Apache-2.0.txt').is_file())
            self.assertTrue((destination / 'ffx_a.h').is_file())
