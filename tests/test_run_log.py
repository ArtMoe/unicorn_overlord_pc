import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path

from paths import ROOT

spec = importlib.util.spec_from_file_location('run', ROOT / 'run.py')
run = importlib.util.module_from_spec(spec)
spec.loader.exec_module(run)


class RunLogTest(unittest.TestCase):
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
