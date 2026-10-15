#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""The ports' launcher (Windows and Linux, Tkinter), themed per game (GAMES).

One window: the game folder (the user's dump), the graphics options of vw.ini and Play. The
artwork comes from the selected dump (sce_sys/); nothing of the game is shipped. Settings:
data/vw.ini (shared with the in-game menu) and data/launcher.json. Each game has a two-line
entry script (odin_launcher.py, ...) that calls main(<key>).

Packaged (PyInstaller, packaging/package.py) the executable carries run.py, scripts/ and
patches/ and also runs them (`--run`, `--script <file>`), so a packaged port needs no Python
installation:

    <Game>(.exe)        this launcher, with the runtime scripts and bin/vwport(.exe) inside
    LICENSE
    data/               vw.ini, launcher.json, user/ (saves, caches), out/ (generated)
"""
import json
import os
from pathlib import Path
import runpy
import subprocess
import sys
import threading

FROZEN = getattr(sys, 'frozen', False)
HERE = Path(sys.executable).resolve().parent if FROZEN else Path(__file__).resolve().parent
# Packaged: run.py, scripts/, patches/ and bin/vwport are inside the executable (PyInstaller's
# bundle folder). From the source tree: the repository.
RUNTIME = Path(sys._MEIPASS) / 'runtime' if FROZEN else HERE.parent  # noqa: SLF001
WINDOWS = os.name == 'nt'


def data_dir(game):
    """data/ beside a packaged launcher (or its .AppImage: the AppImage itself is read-only); in the
    source tree the game's run directory."""
    beside = Path(os.environ['APPIMAGE']).parent if os.environ.get('APPIMAGE') else HERE
    default = beside / 'data' if FROZEN else RUNTIME / game['run_dir']
    return Path(os.environ.get('VW_DATA_DIR', default))


def run_role(argv):
    """--run [args]: run.py; --script <file> [args]: a preparation script."""
    # As `python script.py`: the script's folder comes first on the import path.
    script = argv[1] if argv[0] == '--script' else str(RUNTIME / 'run.py')
    sys.argv = [script, *argv[2 if argv[0] == '--script' else 1:]]
    sys.path.insert(0, str(Path(script).resolve().parent))
    runpy.run_path(script, run_name='__main__')
    return 0


sys.path.insert(0, str(RUNTIME / 'scripts'))
import graphics_options  # noqa: E402  (runtime/scripts)
from patches import game_app_version, game_title_id  # noqa: E402
import tkinter as tk  # noqa: E402
from tkinter import filedialog, font as tkfont  # noqa: E402

try:
    from PIL import Image, ImageDraw, ImageFilter, ImageTk
except ImportError:  # the window still works, without the game's artwork
    Image = None

W, H = 1000, 620
RED = '#e0806a'
GREEN = '#9fd18b'
RESOLUTIONS = (('native', '1080p'), ('1440p', '1440p'), ('1800p', '1800p'), ('4K', '4K'))
DISPLAY_MODES = (('windowed', 'Windowed'), ('borderless', 'Borderless'), ('fullscreen', 'Fullscreen'))

def rgb(color):
    return tuple(int(color[i:i + 2], 16) for i in (1, 3, 5))


# Per game: the dump it accepts, its texts, its artwork in sce_sys/ and its colors (text, dimmed
# text, frame gold and highlight, background, panel fill, idle frame, chosen fill, button fill).
GAMES = {
    'odin': {
        'title_id': 'CUSA05083', 'app_version': '01.01', 'version_hint': 'base game with update 1.01',
        'name': 'Odin Sphere Leifthrasir', 'title': 'Odin Sphere', 'subtitle': 'Leifthrasir  ·  native PC port',
        'run_dir': 'run', 'backdrop': 'pic1.png',
        'game_heading': 'THE BOOK', 'play': 'Begin the Tale', 'running': 'The tale is being told…',
        'colors': {'ink': '#f3e6c4', 'dim': '#b9a988', 'gold': '#d9b45f', 'gold_hi': '#f6d98b',
                   'bg': '#0d1226', 'panel': (14, 16, 34, 214), 'idle': '#5b4c2a', 'chosen': '#3a2e14',
                   'button': '#5a4416', 'field': '#141729', 'rule': '#6d5a2e', 'inner': '#8a6d2f',
                   'hover': '#fff1c4', 'log_bg': '#0b0e1c', 'shade': (6, 8, 20), 'disabled': '#2a2618'},
    },
    # The game's menus: navy panels in gold frames, cream text, a royal blue banner.
    'unicorn': {
        'title_id': 'CUSA36381', 'app_version': '01.04', 'version_hint': 'base game with update 1.04',
        'name': 'Unicorn Overlord', 'title': 'Unicorn Overlord', 'subtitle': 'native PC port',
        'run_dir': 'run_uo', 'backdrop': 'pic0.png',
        'game_heading': 'THE REALM', 'play': 'Begin the Campaign', 'running': 'The campaign is under way…',
        'colors': {'ink': '#ece3cf', 'dim': '#a9a28f', 'gold': '#c8a254', 'gold_hi': '#f0d48a',
                   'bg': '#0a1222', 'panel': (12, 20, 38, 222), 'idle': '#4f5a6e', 'chosen': '#4a3a17',
                   'button': '#1d3a66', 'field': '#121b2e', 'rule': '#5e6c86', 'inner': '#c8a254',
                   'hover': '#fff3cc', 'log_bg': '#0a1020', 'shade': (5, 8, 16), 'disabled': '#1a2130'},
    },
}


def load_settings(data):
    try:
        return json.loads((data / 'launcher.json').read_text(encoding='utf-8'))
    except (OSError, ValueError):
        return {}


def save_settings(data, settings):
    data.mkdir(parents=True, exist_ok=True)
    (data / 'launcher.json').write_text(json.dumps(settings, indent=2), encoding='utf-8')


def check_game(game_info, folder):
    """(ok, message) for a game folder."""
    name, title_id, app_version = game_info['name'], game_info['title_id'], game_info['app_version']
    if not folder:
        return False, f'Choose the folder of your {name} dump.'
    game = Path(folder)
    if not (game / 'eboot.bin').is_file():
        return False, 'No eboot.bin in this folder.'
    title, version = game_title_id(game), game_app_version(game)
    if title != title_id:
        return False, f'This is {title or "an unknown game"}, not {name} ({title_id}).'
    if version != app_version:
        return False, f'Version {version}: the port needs {app_version} ({game_info["version_hint"]}).'
    return True, f'{name}  ·  {title_id}  ·  v{version}'


def open_folder(path):
    path.mkdir(parents=True, exist_ok=True)
    if WINDOWS:
        os.startfile(path)  # noqa: S606
    else:
        subprocess.Popen(['xdg-open', str(path)])


class Launcher:
    def __init__(self, root, game):
        self.root = root
        self.game = game
        self.c = game['colors']
        self.data = data_dir(game)
        self.settings = load_settings(self.data)
        self.config = self.data / 'vw.ini'
        ini = graphics_options.read_ini(self.config)
        self.options = {
            'render_resolution': ini.get('render_resolution', 'native'),
            'display_mode': ini.get('display_mode', 'windowed'),
            'vsync': ini.get('vsync', '1'),
            'show_fps': ini.get('show_fps', '0'),
            'fsr1': ini.get('fsr1', '0'),
            'rcas': ini.get('rcas', '1'),
            'rcas_strength': ini.get('rcas_strength', '50'),
        }
        self.process = None
        self.log_lines = []
        self.log_window = None
        root.title(game['name'])
        root.geometry(f'{W}x{H}')
        root.resizable(False, False)
        root.configure(bg=self.c['bg'])
        families = set(tkfont.families())
        serif = next((f for f in ('Cinzel', 'Trajan Pro', 'Cormorant Garamond', 'EB Garamond', 'Georgia',
                                  'Palatino Linotype', 'DejaVu Serif', 'Liberation Serif', 'Times New Roman')
                      if f in families), 'Times')
        self.f_title = tkfont.Font(family=serif, size=24, weight='bold')
        while self.f_title.measure(game['title']) > 320 and self.f_title.cget('size') > 14:
            self.f_title.configure(size=self.f_title.cget('size') - 1)
        self.f_sub = tkfont.Font(family=serif, size=12, slant='italic')
        self.f_head = tkfont.Font(family=serif, size=11, weight='bold')
        self.f_body = tkfont.Font(family=serif, size=12)
        self.f_small = tkfont.Font(family=serif, size=10)
        self.f_play = tkfont.Font(family=serif, size=20, weight='bold')
        self.canvas = tk.Canvas(root, width=W, height=H, highlightthickness=0, bg=self.c['bg'])
        self.canvas.pack()
        self.images = {}
        self.redraw()

    # -- artwork --------------------------------------------------------------------------------

    def backdrop(self):
        """The dump's storybook art behind a darkened page, or a night-sky gradient."""
        game = Path(self.settings.get('game_dir', ''))
        if Image is None:
            return None
        art = game / 'sce_sys' / self.game['backdrop']
        if art.is_file():
            bg = Image.open(art).convert('RGB')
            scale = max(W / bg.width, H / bg.height)
            bg = bg.resize((round(bg.width * scale), round(bg.height * scale)), Image.LANCZOS)
            left, top = (bg.width - W) // 2, (bg.height - H) // 2
            bg = bg.crop((left, top, left + W, top + H)).filter(ImageFilter.GaussianBlur(1.2))
        else:
            bg = Image.new('RGB', (W, H), self.c['bg'])
            draw = ImageDraw.Draw(bg)
            for y in range(H):
                t = y / H
                draw.line([(0, y), (W, y)], fill=(int(13 + 30 * t), int(18 + 14 * t), int(38 + 30 * t)))
        shade = Image.new('L', (W, H))
        draw = ImageDraw.Draw(shade)
        for x in range(W):  # darker toward the page on the right, a vignette on the left
            draw.line([(x, 0), (x, H)], fill=int(70 + 120 * (x / W) ** 1.5))
        bg = Image.composite(Image.new('RGB', (W, H), self.c['shade']), bg, shade)
        page = Image.new('RGBA', (W, H), (0, 0, 0, 0))
        draw = ImageDraw.Draw(page)
        x0, y0, x1, y1 = 380, 34, W - 34, H - 34
        draw.rounded_rectangle((x0, y0, x1, y1), 14, fill=self.c['panel'])
        for inset, width, color in ((0, 2, (*rgb(self.c['gold']), 255)), (7, 1, (*rgb(self.c['gold']), 140))):
            draw.rounded_rectangle((x0 + inset, y0 + inset, x1 - inset, y1 - inset), 14 - inset // 2,
                                   outline=color, width=width)
        for cx, cy in ((x0, y0), (x1, y0), (x0, y1), (x1, y1)):  # corner gems
            draw.regular_polygon((cx, cy, 7), 4, rotation=45, fill=(*rgb(self.c['gold_hi']), 255))
        return ImageTk.PhotoImage(Image.alpha_composite(bg.convert('RGBA'), page))

    def cover(self):
        art = Path(self.settings.get('game_dir', '')) / 'sce_sys' / 'icon0.png'
        if Image is None or not art.is_file():
            return None
        img = Image.open(art).convert('RGB').resize((260, 260), Image.LANCZOS)
        framed = Image.new('RGB', (272, 272), rgb(self.c['gold']))
        framed.paste(img, (6, 6))
        return ImageTk.PhotoImage(framed)

    # -- drawing --------------------------------------------------------------------------------

    def redraw(self):
        c = self.canvas
        c.delete('all')
        try:
            self.images['bg'] = self.backdrop()
        except Exception as error:  # noqa: BLE001  (artwork is optional)
            print(f'Launcher: no artwork ({error})', file=sys.stderr)
            self.images['bg'] = None
        if self.images['bg']:
            c.create_image(0, 0, image=self.images['bg'], anchor='nw')
        try:
            self.images['cover'] = self.cover()
        except Exception:  # noqa: BLE001
            self.images['cover'] = None
        if self.images['cover']:
            c.create_image(54, 54, image=self.images['cover'], anchor='nw')
            ty = 350
        else:
            ty = 200
        self.shadow_text(54, ty, self.game['title'], self.f_title, self.c['gold_hi'])
        self.shadow_text(56, ty + 48, self.game['subtitle'], self.f_sub, self.c['ink'])
        c.create_text(56, ty + 96, text='In game, Insert or R3+L2\nopens the settings menu.', font=self.f_small,
                      fill=self.c['dim'], anchor='nw')

        x, y = 414, 62
        ok, message = check_game(self.game, self.settings.get('game_dir'))
        self.heading(x, y, self.game['game_heading'])
        y += 28
        path = self.settings.get('game_dir') or 'no game folder chosen'
        if self.f_body.measure(path) > W - 190 - x:
            while self.f_body.measure('…' + path) > W - 190 - x:
                path = path[1:]
            path = '…' + path
        c.create_text(x, y, text=path, font=self.f_body, fill=self.c['ink'], anchor='nw')
        self.link(W - 70, y, 'Choose…', self.choose_game, anchor='ne')
        y += 26
        c.create_text(x, y, text=('✦ ' if ok else '✗ ') + message, font=self.f_small,
                      fill=GREEN if ok else RED, anchor='nw', width=W - 70 - x)
        y += 44
        self.heading(x, y, 'RESOLUTION')
        self.segments(x, y + 28, RESOLUTIONS, 'render_resolution')
        c.create_text(x, y + 64, text='Above your screen size: supersampled, smoother edges.',
                      font=self.f_small, fill=self.c['dim'], anchor='nw', width=W - 80 - x)
        y += 100
        self.heading(x, y, 'DISPLAY')
        self.segments(x, y + 28, DISPLAY_MODES, 'display_mode')
        y += 76
        self.heading(x, y, 'OPTIONS')
        y += 30
        self.toggle(x, y, 'VSync', 'vsync')
        self.toggle(x + 150, y, 'FPS counter', 'show_fps')
        self.toggle(x + 330, y, 'Sharpening', 'rcas')
        y += 34
        self.toggle(x, y, 'FSR 1 upscaling', 'fsr1')
        if self.options['rcas'] == '1':
            self.stepper(x + 330, y, 'rcas_strength')
        c.create_text(x, y + 26, text='FSR 1 and sharpening: for a window above the render size.', font=self.f_small, fill=self.c['dim'], anchor='nw', width=W - 80 - x)

        self.play_button(ok)
        fy = H - 64
        self.link(414, fy, 'Saves folder', lambda: open_folder(self.data / 'user'))
        self.link(414 + self.f_body.measure('Saves folder') + 24 + self.f_body.measure('Log') + 24, fy,
                  'Data folder', lambda: open_folder(self.data))
        self.link(414 + self.f_body.measure('Saves folder') + 24, fy, 'Log', self.show_log)
        self.status = c.create_text(W - 70, fy, text=self.status_text(), font=self.f_small, fill=self.c['dim'],
                                    anchor='ne')

    def shadow_text(self, x, y, text, font, fill):
        self.canvas.create_text(x + 2, y + 2, text=text, font=font, fill='#000000', anchor='nw')
        self.canvas.create_text(x, y, text=text, font=font, fill=fill, anchor='nw')

    def heading(self, x, y, text):
        c = self.canvas
        c.create_text(x, y, text=text, font=self.f_head, fill=self.c['gold'], anchor='nw')
        width = self.f_head.measure(text)
        c.create_line(x + width + 10, y + 9, W - 70, y + 9, fill=self.c['rule'])

    def link(self, x, y, text, command, anchor='nw'):
        item = self.canvas.create_text(x, y, text=text, font=self.f_body, fill=self.c['gold'], anchor=anchor)
        self.hover(item, self.c['gold'], self.c['gold_hi'])
        self.canvas.tag_bind(item, '<Button-1>', lambda _e: command())

    def hover(self, item, normal, lit, outline=False):
        key = 'outline' if outline else 'fill'
        self.canvas.tag_bind(item, '<Enter>', lambda _e: (self.canvas.itemconfigure(item, **{key: lit}),
                                                          self.canvas.configure(cursor='hand2')))
        self.canvas.tag_bind(item, '<Leave>', lambda _e: (self.canvas.itemconfigure(item, **{key: normal}),
                                                          self.canvas.configure(cursor='')))

    def segments(self, x, y, choices, key):
        c = self.canvas
        for value, label in choices:
            width = self.f_body.measure(label) + 28
            chosen = self.options[key] == value
            box = c.create_rectangle(x, y, x + width, y + 28, outline=self.c['gold'] if chosen else self.c['idle'],
                                     fill=self.c['chosen'] if chosen else self.c['field'], width=2 if chosen else 1)
            text = c.create_text(x + width / 2, y + 14, text=label, font=self.f_body,
                                 fill=self.c['gold_hi'] if chosen else self.c['ink'])
            for item in (box, text):
                c.tag_bind(item, '<Button-1>', lambda _e, v=value: self.set_option(key, v))
            self.hover(box, self.c['gold'] if chosen else self.c['idle'], self.c['gold_hi'], outline=True)
            x += width + 8

    def toggle(self, x, y, label, key):
        on = self.options[key] == '1'
        item = self.canvas.create_text(x, y, text=('◆ ' if on else '◇ ') + label, font=self.f_body,
                                       fill=self.c['gold_hi'] if on else self.c['dim'], anchor='nw')
        self.hover(item, self.c['gold_hi'] if on else self.c['dim'], self.c['gold_hi'])
        self.canvas.tag_bind(item, '<Button-1>', lambda _e: self.set_option(key, '0' if on else '1'))

    def stepper(self, x, y, key):
        value = int(self.options[key]) if self.options[key].isdigit() else 50
        c = self.canvas
        c.create_text(x, y, text='strength', font=self.f_small, fill=self.c['dim'], anchor='nw')
        bx = x + self.f_small.measure('strength') + 12
        for i in range(5):  # five pips: 0, 25, 50, 75, 100
            level = i * 25
            pip = c.create_oval(bx + i * 22, y + 3, bx + i * 22 + 12, y + 15,
                                fill=self.c['gold'] if level <= value else '', outline=self.c['gold'])
            c.tag_bind(pip, '<Button-1>', lambda _e, v=level: self.set_option(key, str(v)))

    def play_button(self, ok):
        c = self.canvas
        x0, y0, x1, y1 = 414, H - 150, W - 70, H - 90
        running = self.process is not None
        label = self.game['running'] if running else self.game['play']
        fill = self.c['button'] if ok and not running else self.c['disabled']
        box = c.create_rectangle(x0, y0, x1, y1, fill=fill, outline=self.c['gold_hi'] if ok else self.c['idle'], width=2)
        c.create_rectangle(x0 + 5, y0 + 5, x1 - 5, y1 - 5, outline=self.c['inner'])
        text = c.create_text((x0 + x1) / 2, (y0 + y1) / 2, text=label, font=self.f_play,
                             fill=self.c['gold_hi'] if ok and not running else self.c['dim'])
        if ok and not running:
            for item in (box, text):
                c.tag_bind(item, '<Button-1>', lambda _e: self.play())
            self.hover(box, self.c['gold_hi'], self.c['hover'], outline=True)

    def status_text(self):
        return 'Running' if self.process is not None else ''

    # -- actions --------------------------------------------------------------------------------

    def set_option(self, key, value):
        self.options[key] = value
        self.data.mkdir(parents=True, exist_ok=True)
        graphics_options.write_keys(self.config, {key: value})
        self.redraw()

    def choose_game(self):
        folder = filedialog.askdirectory(title=f"The {self.game['title_id']} folder (eboot.bin, sce_sys, …)",
                                         initialdir=self.settings.get('game_dir') or str(Path.home()))
        if folder:
            self.settings['game_dir'] = folder
            save_settings(self.data, self.settings)
            self.redraw()

    def play(self):
        env = dict(os.environ, VW_GAME_DIR=self.settings['game_dir'], VW_DATA_DIR=str(self.data),
                   VW_CONFIG=str(self.config))
        if FROZEN:
            # run.py uses the vwport of its own bundle (bin/), which lives as long as the game
            # does; one placed beside the launcher replaces it (testing a build).
            beside = HERE / ('vwport.exe' if WINDOWS else 'vwport')
            if beside.is_file():
                env['VW_PROBE'] = str(beside)
            command = [sys.executable, '--run']
        else:
            command = [sys.executable, str(RUNTIME / 'run.py')]
        self.data.mkdir(parents=True, exist_ok=True)
        self.log_lines = []
        flags = getattr(subprocess, 'CREATE_NO_WINDOW', 0)
        self.process = subprocess.Popen(command, env=env, cwd=str(RUNTIME), stdout=subprocess.PIPE,
                                        stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL, text=True,
                                        errors='replace', creationflags=flags)
        threading.Thread(target=self.read_output, daemon=True).start()
        self.redraw()

    def read_output(self):
        process = self.process
        for line in process.stdout:
            self.log_lines.append(line.rstrip('\n'))
            del self.log_lines[:-4000]
        code = process.wait()
        (self.data / 'last_run.log').write_text('\n'.join(self.log_lines) + '\n', encoding='utf-8')
        self.root.after(0, self.finished, code)

    def finished(self, code):
        self.process = None
        self.redraw()
        if code not in (0, None):
            self.canvas.itemconfigure(self.status, text=f'The game stopped (code {code}): see Log', fill=RED)

    def show_log(self):
        if self.log_window and self.log_window.winfo_exists():
            self.log_window.lift()
            return
        top = self.log_window = tk.Toplevel(self.root)
        top.title(f"{self.game['title']}: log")
        top.geometry('900x480')
        text = tk.Text(top, bg=self.c['log_bg'], fg=self.c['ink'], insertbackground=self.c['ink'], font=('Courier', 10), wrap='none')
        text.pack(fill='both', expand=True)
        lines = self.log_lines
        if not lines and (self.data / 'last_run.log').is_file():
            lines = (self.data / 'last_run.log').read_text(encoding='utf-8', errors='replace').splitlines()

        def refresh(shown=[0]):  # noqa: B006  (the count shown so far)
            if not top.winfo_exists():
                return
            current = self.log_lines or lines
            if len(current) != shown[0]:
                text.delete('1.0', 'end')
                text.insert('end', '\n'.join(current))
                text.see('end')
                shown[0] = len(current)
            top.after(500, refresh)
        refresh()


def main(key):
    """Runs the launcher for GAMES[key], or one of the packaged roles (--run, --script)."""
    if len(sys.argv) > 1 and sys.argv[1] in ('--run', '--script'):
        sys.exit(run_role(sys.argv[1:]))
    root = tk.Tk()
    Launcher(root, GAMES[key])
    root.mainloop()
