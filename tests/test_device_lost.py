# SPDX-License-Identifier: GPL-2.0-or-later
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from paths import ROOT


class DeviceLostTest(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++') or shutil.which('clang++'), 'C++ compiler unavailable')
    def test_fatal_report_is_flushed_without_gpu_or_teardown(self):
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp) / 'lost.cpp'
            source.write_text('#include "video_core/renderer_vulkan/vk_device_lost.h"\n'
                              'int main() { Vulkan::DeviceLost("injected test"); }\n')
            exe = Path(temp) / ('lost.exe' if __import__('os').name == 'nt' else 'lost')
            compiler = shutil.which('c++') or shutil.which('clang++')
            subprocess.run([compiler, '-std=c++17', '-I', str(ROOT / 'gpu/shadps4'),
                            str(source), '-o', str(exe)], check=True, capture_output=True)
            result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=5)
            self.assertEqual(result.returncode, 23)
            self.assertIn('GPU crash: the graphics device was lost (injected test)', result.stderr)
