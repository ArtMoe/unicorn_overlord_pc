from paths import ROOT
import tempfile
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

import patches
from patches import (compile_patches, external_patches, external_selection, compile_external,
                     game_profile, selected_patches, patch_requirements)

BUILT_IN = ROOT / 'patches/Built_In.xml'  # a built-in file's path (only its name is compared)
IDS = {'CUSA00000'}
SEGMENTS = [(0, 0x6000000)]


EXTERNAL = """<?xml version="1.0"?>
<Patch>
  <TitleID><ID>CUSA00000</ID></TitleID>
  <Metadata Title="Test Game" Name="On" Author="x" PatchVer="1.0" AppVer="01.02" AppElf="eboot.bin" isEnabled="true">
    <PatchList><Line Type="bytes" Address="0x00401000" Value="9090"/></PatchList>
  </Metadata>
  <Metadata Title="Test Game" Name="Off" Author="x" PatchVer="1.0" AppVer="01.02" AppElf="eboot.bin">
    <PatchList><Line Type="bytes32" Address="0x00402000" Value="0x12345678"/></PatchList>
  </Metadata>
  <Metadata Title="Test Game" Name="Mask" Author="x" PatchVer="1.0" AppVer="01.02" AppElf="eboot.bin" isEnabled="true">
    <PatchList><Line Type="mask" Value="90 ?? 90" Offset="0"/></PatchList>
  </Metadata>
  <Metadata Title="Test Game" Name="Old" Author="x" PatchVer="1.0" AppVer="01.00" AppElf="eboot.bin" isEnabled="true">
    <PatchList><Line Type="bytes" Address="0x00403000" Value="90"/></PatchList>
  </Metadata>
</Patch>"""


class ExternalPatchTests(unittest.TestCase):
    def test_selection_and_unsupported_lines(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            (folder / 'extra.xml').write_text(EXTERNAL)
            (folder / 'other.xml').write_text(EXTERNAL.replace('CUSA00000', 'CUSA99999'))
            (folder / 'broken.xml').write_text('<Patch>')
            found = external_patches(folder, '01.02', BUILT_IN, IDS)
            self.assertEqual([key for key, _, _ in found],
                             ['extra.xml/On', 'extra.xml/Off', 'extra.xml/Mask'])
            # The file's isEnabled; the mask patch is skipped as a whole.
            writes = compile_external(external_selection(found, None), SEGMENTS)
            self.assertEqual(writes, [(0x1000, bytes.fromhex('9090'))])
            config = folder / 'patches.json'
            config.write_text('{"enabled": ["extra.xml/Off"], "disabled": ["extra.xml/On"]}')
            writes = compile_external(external_selection(found, config), SEGMENTS)
            self.assertEqual(writes, [(0x2000, (0x12345678).to_bytes(4, 'little'))])

    def test_built_in_file_is_not_external(self):
        with tempfile.TemporaryDirectory() as directory:
            built_in = Path(directory) / 'Game.xml'
            built_in.write_text(EXTERNAL)
            self.assertEqual(external_patches(built_in.parent, '01.02', built_in, IDS), [])


REQUIREMENTS = """<?xml version="1.0"?>
<Patch>
  <TitleID><ID>CUSA00000</ID></TitleID>
  <Metadata Name="Big" AppVer="01.02" Note="Requires setting DMEM to 6144 MB" isEnabled="true">
    <PatchList><Line Type="bytes" Address="0x00401000" Value="90"/></PatchList>
  </Metadata>
  <Metadata Name="Fast" AppVer="01.02" Note="Requires setting VBlank to 120 FPS">
    <PatchList><Line Type="bytes" Address="0x00402000" Value="90"/></PatchList>
  </Metadata>
</Patch>"""


class GameProfileTests(unittest.TestCase):
    def setUp(self):
        self.games = patches.GAMES
        patches.GAMES = [(IDS, BUILT_IN, '01.02', '00' * 32)]

    def tearDown(self):
        patches.GAMES = self.games

    def test_profiles_by_title_id(self):
        self.assertEqual(game_profile('CUSA00000')[1:3], (BUILT_IN, '01.02'))
        self.assertIsNone(game_profile('CUSA99999'))
        self.assertIsNone(game_profile(None))

    def test_defaults_and_extras(self):
        with tempfile.TemporaryDirectory() as folder:
            xml = Path(folder) / 'game.xml'
            xml.write_text(REQUIREMENTS)
            self.assertEqual(selected_patches(xml, '01.02', ''), ['Big'])
            self.assertEqual(selected_patches(xml, '01.02', 'Fast; Big'), ['Big', 'Fast'])
            self.assertEqual(selected_patches(xml, '01.00', ''), [])
            # The launcher's selection is exact: unchecked defaults stay off.
            self.assertEqual(selected_patches(xml, '01.02', 'Fast', only=True), ['Fast'])
            self.assertEqual(selected_patches(xml, '01.02', '', only=True), [])

    def test_eboot_build_check(self):
        from patches import eboot_matches
        with tempfile.TemporaryDirectory() as folder:
            (Path(folder) / 'eboot.bin').write_bytes(b'another build')
            self.assertFalse(eboot_matches(folder, game_profile('CUSA00000')))
            self.assertTrue(eboot_matches(folder, None))  # no profile: nothing to check

    def test_patch_file_compiles(self):
        with tempfile.TemporaryDirectory() as folder:
            xml = Path(folder) / 'game.xml'
            xml.write_text(REQUIREMENTS)
            names = [m.get('Name') for m in ET.parse(xml).getroot().iter('Metadata')]
            self.assertTrue(compile_patches(xml, names, '01.02', SEGMENTS))

    def test_requirements_from_patch_notes(self):
        # Notes ask for extra direct memory (shadPS4 "DMEM") and a VBlank rate.
        with tempfile.TemporaryDirectory() as folder:
            xml = Path(folder) / 'game.xml'
            xml.write_text(REQUIREMENTS)
            self.assertEqual(patch_requirements(xml, ['Big'], '01.02'), (5056 + 6144, None))
            self.assertEqual(patch_requirements(xml, ['Fast'], '01.02'), (None, 120))
            self.assertEqual(patch_requirements(xml, [], '01.02'), (None, None))


if __name__ == '__main__':
    unittest.main()
