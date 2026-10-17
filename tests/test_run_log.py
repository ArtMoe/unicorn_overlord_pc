import importlib.util
import json
import sys
import tempfile
import unittest
from unittest.mock import patch
from pathlib import Path

from paths import ROOT

spec = importlib.util.spec_from_file_location('run', ROOT / 'run.py')
run = importlib.util.module_from_spec(spec)
spec.loader.exec_module(run)


class RunLogTest(unittest.TestCase):
    def test_gpu_driver_header_survives_log_rotation(self):
        with tempfile.TemporaryDirectory() as temp:
            log = run.RunLog()
            log.open(Path(temp))
            log.support = {'port_version': 'abc123'}
            log.write('GPU selected: Test GPU; driver: 1.2.3; Vulkan: 1.3', console=False)
            log.write('Mods: Test mod: 4 files', console=False)
            log.write('Patches: extra.xml/Test patch (1 writes)', console=False)
            with patch.object(run, 'LOG_BYTES', 1024):
                for _ in range(40):
                    log.write('x' * 100, console=False)
            log.close(0)
            info = json.loads(log.path.with_suffix('.json').read_text())
            self.assertIn('driver: 1.2.3', info['gpu_driver_api'])
            self.assertEqual(info['mods'], ['Test mod'])
            self.assertEqual(info['patches'], ['extra.xml/Test patch (1 writes)'])

    def test_rapid_restarts_live_log_and_bounded_rotation(self):
        with tempfile.TemporaryDirectory() as temp:
            data = Path(temp)
            paths = []
            for _ in range(2):
                log = run.RunLog()
                log.open(data)
                paths.append(log.path)
                with patch.object(run, 'LOG_BYTES', 1024):
                    for i in range(40):
                        log.write(f'{i}: ' + 'x' * 100, console=False)
                    self.assertIn('39:', (data / 'last_run.log').read_text())
                    self.assertLessEqual(log.path.stat().st_size, 1024)
                    self.assertLessEqual(log.path.with_suffix('.log.1').stat().st_size, 1024)
                log.close(0)
            self.assertNotEqual(*paths)

    def test_capture_preserves_protocol_and_logs_stderr(self):
        with tempfile.TemporaryDirectory() as temp:
            log = run.RunLog()
            log.open(Path(temp))
            code, output = log.run([sys.executable, '-c',
                'import sys; print("/path/result"); print("preparation failed", file=sys.stderr); sys.exit(3)'],
                capture=True)
            log.close(code)
            self.assertEqual((code, output.strip()), (3, '/path/result'))
            self.assertIn('preparation failed', log.path.read_text())
            self.assertNotIn('/path/result', log.path.read_text())

    def test_python_exception_is_in_saved_log(self):
        with tempfile.TemporaryDirectory() as temp:
            log = run.RunLog()
            log.open(Path(temp))
            with patch.object(run, 'LOG', log), patch.object(run, 'main', side_effect=RuntimeError('test failure')):
                self.assertEqual(run.entrypoint(), 1)
            self.assertIn('RuntimeError: test failure', log.path.read_text())

    def test_keeps_newest_logs_and_copies_last_run(self):
        with tempfile.TemporaryDirectory() as temp:
            data = Path(temp)
            logs = data / 'logs'
            logs.mkdir()
            for day in range(1, run.LOG_KEEP + 3):
                (logs / f'run-200001{day:02d}-000000.log').write_text('old\n')
            log = run.RunLog()
            log.open(data)
            code = log.run([sys.executable, '-c', 'import sys; print("out"); print("err", file=sys.stderr); sys.exit(3)'])
            log.close(code)
            self.assertEqual(code, 3)
            kept = sorted(logs.glob('run-*.log'))
            self.assertEqual(len(kept), run.LOG_KEEP)
            self.assertEqual(kept[-1], log.path)
            text = (data / 'last_run.log').read_text()
            self.assertIn('] out\n', text)
            self.assertIn('] err\n', text)
            self.assertIn('Exit code 3', text)


if __name__ == '__main__':
    unittest.main()
