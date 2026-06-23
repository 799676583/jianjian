import base64
import ctypes
import queue
import threading
import tkinter as tk
from tkinter import messagebox, ttk

try:
    import serial
    from serial.tools import list_ports
except ImportError as exc:
    raise SystemExit("Missing dependency: pip install pyserial") from exc


class JianjianVaultApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Jianjian Password Vault")
        self.root.resizable(False, False)
        self.port_var = tk.StringVar()
        self.site_var = tk.StringVar()
        self.username_var = tk.StringVar()
        self.password_var = tk.StringVar()
        self.status_var = tk.StringVar(value="Choose a USB serial port, then press Connect.")
        self.ser = None
        self.rx_queue = queue.Queue()
        self.session_token = None
        self.reader_stop = threading.Event()
        self.input_layout_to_restore = None

        outer = ttk.Frame(root, padding=16)
        outer.grid(sticky="nsew")
        outer.columnconfigure(1, weight=1)

        ttk.Label(outer, text="USB port").grid(row=0, column=0, sticky="w", pady=(0, 8))
        self.port_box = ttk.Combobox(outer, textvariable=self.port_var, width=28, state="readonly")
        self.port_box.grid(row=0, column=1, sticky="ew", pady=(0, 8))
        ttk.Button(outer, text="Refresh", command=self.refresh_ports).grid(row=0, column=2, padx=(8, 0), pady=(0, 8))

        self.connect_button = ttk.Button(outer, text="Connect", command=self.request_pairing)
        self.connect_button.grid(row=1, column=1, sticky="w", pady=(0, 14))

        ttk.Separator(outer).grid(row=2, column=0, columnspan=3, sticky="ew", pady=(0, 14))
        self._field(outer, 3, "Website", self.site_var, show=None)
        self._field(outer, 4, "Username", self.username_var, show=None)
        self._field(outer, 5, "Password", self.password_var, show="*")

        self.save_button = ttk.Button(outer, text="Save to Jianjian", command=self.save_entry, state="disabled")
        self.save_button.grid(row=6, column=1, sticky="w", pady=(6, 12))
        ttk.Label(outer, textvariable=self.status_var, wraplength=360).grid(row=7, column=0, columnspan=3, sticky="w")
        ttk.Label(outer, text="Saving needs physical approval on Jianjian. Passwords cannot be read back by this app.", wraplength=360).grid(
            row=8, column=0, columnspan=3, sticky="w", pady=(10, 0)
        )

        self.root.protocol("WM_DELETE_WINDOW", self.close)
        self.refresh_ports()
        self.root.after(100, self.poll_messages)

    def _field(self, parent, row, label, variable, show):
        ttk.Label(parent, text=label).grid(row=row, column=0, sticky="w", pady=4)
        entry = ttk.Entry(parent, textvariable=variable, width=32, show=show, state="disabled")
        entry.grid(row=row, column=1, columnspan=2, sticky="ew", pady=4)
        if label == "Website":
            self.site_entry = entry
        elif label == "Username":
            self.username_entry = entry
        else:
            self.password_entry = entry

    def refresh_ports(self):
        ports = []
        for port in list_ports.comports():
            label = f"{port.device} - {port.description}"
            ports.append(label)
        self.port_box["values"] = ports
        if ports and not self.port_var.get():
            self.port_box.current(0)

    def selected_port(self):
        return self.port_var.get().split(" - ", 1)[0].strip()

    def open_serial(self):
        if self.ser and self.ser.is_open:
            return True
        port = self.selected_port()
        if not port:
            messagebox.showerror("No port", "Choose Jianjian's USB serial port first.")
            return False
        try:
            self.ser = serial.Serial(port, 115200, timeout=0.25, write_timeout=1)
        except serial.SerialException as exc:
            messagebox.showerror("Connection failed", str(exc))
            return False
        self.reader_stop.clear()
        threading.Thread(target=self.reader_loop, daemon=True).start()
        return True

    def request_pairing(self):
        if not self.open_serial():
            return
        self.session_token = None
        self.set_entries_enabled(False)
        self.status_var.set("Pair request sent. Confirm Yes on Jianjian with the encoder.")
        self.send("#PING")
        self.send("#PAIR")

    def reader_loop(self):
        while not self.reader_stop.is_set() and self.ser and self.ser.is_open:
            try:
                raw = self.ser.readline()
            except serial.SerialException:
                break
            if raw:
                self.rx_queue.put(raw.decode("utf-8", errors="replace").strip())

    def poll_messages(self):
        while True:
            try:
                line = self.rx_queue.get_nowait()
            except queue.Empty:
                break
            self.handle_message(line)
        self.root.after(100, self.poll_messages)

    def force_english_layout(self):
        if not hasattr(ctypes, "windll"):
            return
        user32 = ctypes.windll.user32
        hwnd = user32.GetForegroundWindow()
        if not hwnd:
            return
        thread_id = user32.GetWindowThreadProcessId(hwnd, None)
        self.input_layout_to_restore = user32.GetKeyboardLayout(thread_id)
        english_layout = user32.LoadKeyboardLayoutW("00000409", 1)
        if english_layout:
            user32.PostMessageW(hwnd, 0x0050, 0, english_layout)
            self.root.after(4200, self.restore_input_layout)

    def restore_input_layout(self):
        if not self.input_layout_to_restore or not hasattr(ctypes, "windll"):
            return
        user32 = ctypes.windll.user32
        hwnd = user32.GetForegroundWindow()
        if hwnd:
            user32.PostMessageW(hwnd, 0x0050, 0, self.input_layout_to_restore)
        self.input_layout_to_restore = None
    def handle_message(self, line):
        if line == "#TYPE_PENDING":
            self.force_english_layout()
            self.status_var.set("English input prepared. Jianjian is typing, then your input method will be restored.")
        elif line.startswith("#READY"):
            self.status_var.set("Jianjian is ready. Waiting for physical approval.")
        elif line == "#PAIR_PROMPT":
            self.status_var.set("Check Jianjian: select Yes, connect and short-press.")
        elif line.startswith("#PAIR_OK|"):
            self.session_token = line.split("|", 1)[1]
            self.set_entries_enabled(True)
            self.status_var.set("Connected. Enter one website credential and save it to Jianjian.")
        elif line == "#PAIR_DENY":
            self.session_token = None
            self.set_entries_enabled(False)
            self.status_var.set("Pairing was denied or timed out on Jianjian.")
        elif line == "#SAVED":
            self.password_var.set("")
            self.status_var.set("Saved to Jianjian. The password was not returned to this app.")
        elif line.startswith("#ERROR|"):
            self.status_var.set(f"Jianjian rejected the request: {line.split('|', 1)[1]}")
        elif line == "#CLOSED":
            self.session_token = None
            self.set_entries_enabled(False)
            self.status_var.set("Session closed.")

    def set_entries_enabled(self, enabled):
        state = "normal" if enabled else "disabled"
        for entry in (self.site_entry, self.username_entry, self.password_entry):
            entry.configure(state=state)
        self.save_button.configure(state=state)

    @staticmethod
    def ascii_safe(value):
        return all(32 <= ord(char) <= 126 for char in value)

    @staticmethod
    def encode(value):
        return base64.b64encode(value.encode("utf-8")).decode("ascii")

    def send(self, line):
        try:
            self.ser.write((line + "\n").encode("utf-8"))
            self.ser.flush()
        except (serial.SerialException, AttributeError) as exc:
            self.status_var.set(f"Serial error: {exc}")

    def save_entry(self):
        if not self.session_token:
            messagebox.showerror("Not paired", "Press Connect and approve it on Jianjian first.")
            return
        site = self.site_var.get().strip()
        username = self.username_var.get()
        password = self.password_var.get()
        if not site or not username or not password:
            messagebox.showerror("Missing field", "Website, username, and password are all required.")
            return
        if any(len(value) > limit for value, limit in ((site, 32), (username, 64), (password, 96))):
            messagebox.showerror("Too long", "Maximum lengths: website 32, username 64, password 96 characters.")
            return
        if not all(self.ascii_safe(value) for value in (site, username, password)):
            messagebox.showerror("Keyboard compatibility", "This demo only types printable ASCII reliably through USB HID.")
            return
        self.send("#SAVE|" + "|".join((self.session_token, self.encode(site), self.encode(username), self.encode(password))))
        self.status_var.set("Saving to Jianjian...")

    def close(self):
        if self.ser and self.ser.is_open:
            try:
                self.send("#CLOSE")
                self.ser.close()
            except serial.SerialException:
                pass
        self.reader_stop.set()
        self.root.destroy()


if __name__ == "__main__":
    root = tk.Tk()
    JianjianVaultApp(root)
    root.mainloop()