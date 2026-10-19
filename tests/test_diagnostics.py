# SPDX-License-Identifier: GPL-2.0-or-later
import json
from pathlib import Path
import tempfile
import unittest
import zipfile
from paths import ROOT
import diagnostics


class DiagnosticsTests(unittest.TestCase):
    def test_export_only_selected_run_redacts_paths_and_preserves_fault(self):
        with tempfile.TemporaryDirectory() as temp:
            data = Path(temp)
            logs = data / 'logs'
            logs.mkdir()
            log = logs / 'run-20261018-040000-unique.log'
            log.write_text(f'Game: {data}/game\nFault C:\\Users\\Alice\\game at guest+0x123\n')
            log.with_suffix('.log.1').write_text('/home/Bob/old\n')
            diagnostics.write_summary(log, {'port_version': 'abc123', 'graphics': {'render_resolution': 'native'}})
            (data / 'secret.key').write_text('do not include')
            files = diagnostics.support_files(log, data)
            destination = data / 'support.zip'
            diagnostics.export_archive(destination, files)
            with zipfile.ZipFile(destination) as archive:
                self.assertEqual(set(archive.namelist()), {'run.log', 'previous.log', 'system.json'})
                text = '\n'.join(archive.read(n).decode() for n in archive.namelist())
            for private in ('Alice', 'Bob', str(data), 'do not include'):
                self.assertNotIn(private, text)
            self.assertIn('guest+0x123', text)
            self.assertIn('abc123', text)

    def test_rejects_files_outside_logs_and_symlinks(self):
        with tempfile.TemporaryDirectory() as temp:
            data = Path(temp)
            (data / 'logs').mkdir()
            outside = data / 'run-secret.log'
            outside.write_text('secret')
            with self.assertRaises(ValueError):
                diagnostics.support_files(outside, data)
            link = data / 'logs' / 'run-link.log'
            try:
                link.symlink_to(outside)
            except OSError:
                return  # Windows without symlink privilege
            with self.assertRaises(ValueError):
                diagnostics.support_files(link, data)

    def test_system_info_is_allowlisted(self):
        info = diagnostics.system_info()
        self.assertEqual(set(info), {'os', 'cpu', 'cpu_threads', 'ram_gib'})
        json.dumps(info)
