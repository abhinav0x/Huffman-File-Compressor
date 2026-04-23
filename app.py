import customtkinter as ctk
import subprocess
import threading
import os
import platform
import time
import struct
from tkinter import filedialog, messagebox
from pathlib import Path

# ── Theme ─────────────────────────────────────────────────────
ctk.set_appearance_mode("dark")
ctk.set_default_color_theme("blue")


# ── Locate huffman binary ──────────────────────────────────────
def find_huffman() -> str:
    base = Path(__file__).parent
    for name in ["huffman.exe", "huffman"]:
        p = base / name
        if p.exists():
            return str(p)
    return str(base / ("huffman.exe" if platform.system() == "Windows" else "huffman"))


HUFFMAN_BIN = find_huffman()


# ── Read original filename from .huff v2 header ────────────────
def read_huff_origname(path: str):
    """Return stored original filename from HUF2 header, or None."""
    try:
        with open(path, "rb") as f:
            magic = f.read(4)
            if magic != b"HUF2":
                return None
            name_len = struct.unpack("B", f.read(1))[0]
            orig_name = f.read(name_len).decode("utf-8", errors="replace")
            return orig_name
    except Exception:
        return None


# ════════════════════════════════════════════════════════════════
#  Main Application
# ════════════════════════════════════════════════════════════════
class HuffmanApp(ctk.CTk):

    def __init__(self):
        super().__init__()
        self.title("Huffman Compressor")
        self.geometry("800x680")
        self.resizable(True, True)
        self.minsize(680, 560)

        self._input_path  = ctk.StringVar()
        self._output_path = ctk.StringVar()
        self._verify_var  = ctk.BooleanVar(value=True)
        self._mode        = ctk.StringVar(value="compress")
        self._running     = False

        self._build_ui()

    # ── UI ────────────────────────────────────────────────────
    def _build_ui(self):
        self.grid_columnconfigure(0, weight=1)
        self.grid_rowconfigure(8, weight=1)

        # Header
        header = ctk.CTkFrame(self, corner_radius=0, fg_color=("#1a1a2e", "#0d0d1a"))
        header.grid(row=0, column=0, sticky="ew")
        header.grid_columnconfigure(1, weight=1)

        ctk.CTkLabel(
            header, text="⚙  HUFFMAN CODER",
            font=ctk.CTkFont(family="Courier New", size=22, weight="bold"),
            text_color="#0022ff"
        ).grid(row=0, column=0, padx=24, pady=18, sticky="w")

        ctk.CTkLabel(
            header, text="Compress any file — txt · pdf · jpg · zip · …",
            font=ctk.CTkFont(size=12), text_color="#888"
        ).grid(row=0, column=1, padx=0, pady=18, sticky="w")

        exe_ok = os.path.exists(HUFFMAN_BIN)
        ctk.CTkLabel(
            header,
            text=f"● huffman {'found' if exe_ok else 'NOT FOUND'}",
            font=ctk.CTkFont(size=11),
            text_color="#00c853" if exe_ok else "#ff3d00"
        ).grid(row=0, column=2, padx=24, pady=18, sticky="e")

        # Mode toggle
        mode_frame = ctk.CTkFrame(self, fg_color="transparent")
        mode_frame.grid(row=1, column=0, padx=20, pady=(16, 0), sticky="ew")
        ctk.CTkLabel(mode_frame, text="Mode:",
                     font=ctk.CTkFont(size=13, weight="bold")).pack(side="left", padx=(0, 10))
        self._seg = ctk.CTkSegmentedButton(
            mode_frame, values=["Compress", "Decompress"],
            command=self._on_mode_change,
            font=ctk.CTkFont(size=13, weight="bold"),
            selected_color="#0077b6", selected_hover_color="#0096c7", width=280
        )
        self._seg.set("Compress")
        self._seg.pack(side="left")

        # File pickers
        files_frame = ctk.CTkFrame(self)
        files_frame.grid(row=2, column=0, padx=20, pady=12, sticky="ew")
        files_frame.grid_columnconfigure(1, weight=1)

        self._input_label = ctk.CTkLabel(
            files_frame, text="Input File", width=95,
            font=ctk.CTkFont(size=13, weight="bold"), anchor="w"
        )
        self._input_label.grid(row=0, column=0, padx=(16, 8), pady=(14, 6), sticky="w")

        self._input_entry = ctk.CTkEntry(
            files_frame, textvariable=self._input_path,
            placeholder_text="Select any file to compress…",
            font=ctk.CTkFont(family="Courier New", size=12), height=36
        )
        self._input_entry.grid(row=0, column=1, padx=(0, 8), pady=(14, 6), sticky="ew")

        ctk.CTkButton(
            files_frame, text="Browse", width=80,
            command=self._browse_input,
            fg_color="#0077b6", hover_color="#0096c7"
        ).grid(row=0, column=2, padx=(0, 16), pady=(14, 6))

        self._out_label = ctk.CTkLabel(
            files_frame, text="Output File", width=95,
            font=ctk.CTkFont(size=13, weight="bold"), anchor="w"
        )
        self._out_label.grid(row=1, column=0, padx=(16, 8), pady=(6, 14), sticky="w")

        self._output_entry = ctk.CTkEntry(
            files_frame, textvariable=self._output_path,
            placeholder_text="Output path (auto-filled)…",
            font=ctk.CTkFont(family="Courier New", size=12), height=36
        )
        self._output_entry.grid(row=1, column=1, padx=(0, 8), pady=(6, 14), sticky="ew")

        ctk.CTkButton(
            files_frame, text="Browse", width=80,
            command=self._browse_output,
            fg_color="#0077b6", hover_color="#0096c7"
        ).grid(row=1, column=2, padx=(0, 16), pady=(6, 14))

        # Info banner
        self._info_banner = ctk.CTkLabel(
            self, text="", font=ctk.CTkFont(family="Courier New", size=11),
            text_color="#ffd54f", anchor="w"
        )
        self._info_banner.grid(row=3, column=0, padx=24, pady=(0, 2), sticky="ew")

        # Options
        opts_frame = ctk.CTkFrame(self, fg_color="transparent")
        opts_frame.grid(row=4, column=0, padx=20, pady=0, sticky="ew")
        self._verify_check = ctk.CTkCheckBox(
            opts_frame, text="Verify after compress (--verify)",
            variable=self._verify_var,
            font=ctk.CTkFont(size=12),
            checkbox_height=20, checkbox_width=20
        )
        self._verify_check.pack(side="left")

        # Run button
        self._run_btn = ctk.CTkButton(
            self, text="▶  Run Compress",
            command=self._run, height=46,
            font=ctk.CTkFont(size=15, weight="bold"),
            fg_color="#00897b", hover_color="#00acc1", corner_radius=8
        )
        self._run_btn.grid(row=5, column=0, padx=20, pady=12, sticky="ew")

        # Progress
        self._progress = ctk.CTkProgressBar(self, mode="indeterminate", height=6)
        self._progress.grid(row=6, column=0, padx=20, pady=(0, 6), sticky="ew")
        self._progress.set(0)

        # Log header
        log_label_frame = ctk.CTkFrame(self, fg_color="transparent")
        log_label_frame.grid(row=7, column=0, padx=20, pady=(4, 0), sticky="ew")
        ctk.CTkLabel(log_label_frame, text="Output Log",
                     font=ctk.CTkFont(size=12, weight="bold")).pack(side="left")
        ctk.CTkButton(
            log_label_frame, text="Clear", width=60, height=24,
            command=self._clear_log,
            font=ctk.CTkFont(size=11), fg_color="#37474f", hover_color="#546e7a"
        ).pack(side="right")

        # Log box
        self._log = ctk.CTkTextbox(
            self,
            font=ctk.CTkFont(family="Courier New", size=12),
            fg_color=("#111", "#0a0a0a"),
            text_color="#e0e0e0",
            corner_radius=8, wrap="word"
        )
        self._log.grid(row=8, column=0, padx=20, pady=(4, 16), sticky="nsew")

        # Status bar
        self._status_label = ctk.CTkLabel(
            self, text="Ready.", font=ctk.CTkFont(size=11),
            text_color="#888", anchor="w"
        )
        self._status_label.grid(row=9, column=0, padx=24, pady=(0, 8), sticky="ew")

        self._log_write("Huffman Compressor ready.\n")
        self._log_write("Supports: .txt  .pdf  .jpg  .png  .docx  .zip  and any file\n\n",
                        color="cyan")
        if not os.path.exists(HUFFMAN_BIN):
            self._log_write(
                f"⚠  huffman binary not found at: {HUFFMAN_BIN}\n"
                "   Compile with:\n"
                "   Windows : g++ -O2 -std=c++17 -o huffman.exe huffman.cpp\n"
                "   Linux   : g++ -O2 -std=c++17 -o huffman huffman.cpp\n\n",
                color="red"
            )

    # ── Mode Change ───────────────────────────────────────────
    def _on_mode_change(self, value: str):
        mode = value.lower()
        self._mode.set(mode)
        self._input_path.set("")
        self._output_path.set("")
        self._info_banner.configure(text="")

        if mode == "compress":
            self._run_btn.configure(text="▶  Run Compress",
                                    fg_color="#00897b", hover_color="#00acc1")
            self._verify_check.configure(state="normal")
            self._input_entry.configure(placeholder_text="Select any file to compress…")
            self._out_label.configure(text="Output File")
        else:
            self._run_btn.configure(text="▶  Run Decompress",
                                    fg_color="#6a1b9a", hover_color="#8e24aa")
            self._verify_check.configure(state="disabled")
            self._input_entry.configure(placeholder_text="Select .huff file to decompress…")
            self._out_label.configure(text="Restore As")

    # ── Browse ────────────────────────────────────────────────
    def _browse_input(self):
        if self._mode.get() == "compress":
            f = filedialog.askopenfilename(
                title="Select file to compress",
                filetypes=[("All files", "*.*")]
            )
        else:
            f = filedialog.askopenfilename(
                title="Select .huff file",
                filetypes=[("Huffman archive", "*.huff"), ("All files", "*.*")]
            )
        if f:
            self._input_path.set(f)
            self._auto_fill_output(f)

    def _browse_output(self):
        if self._mode.get() == "compress":
            f = filedialog.asksaveasfilename(
                title="Save compressed file as",
                defaultextension=".huff",
                filetypes=[("Huffman archive", "*.huff")]
            )
        else:
            orig = read_huff_origname(self._input_path.get())
            f = filedialog.asksaveasfilename(
                title="Restore file as",
                initialfile=orig or "",
                filetypes=[("All files", "*.*")]
            )
        if f:
            self._output_path.set(f)

    def _auto_fill_output(self, input_path: str):
        p = Path(input_path)
        if self._mode.get() == "compress":
            # photo.jpg → photo.jpg.huff
            out = Path(str(p) + ".huff")
            self._output_path.set(str(out))
            self._info_banner.configure(text="")
        else:
            orig_name = read_huff_origname(input_path)
            if orig_name:
                out = p.parent / orig_name
                self._output_path.set(str(out))
                self._info_banner.configure(text=f"  ↩  Will restore as: {orig_name}")
            else:
                out = p.parent / p.stem   # strip .huff
                self._output_path.set(str(out))
                self._info_banner.configure(
                    text="  ⚠  Could not read v2 header — using fallback name")

    # ── Run ───────────────────────────────────────────────────
    def _run(self):
        if self._running:
            return
        inp  = self._input_path.get().strip()
        out  = self._output_path.get().strip()
        mode = self._mode.get()

        if not inp:
            messagebox.showerror("Missing input", "Please select an input file."); return
        if not out:
            messagebox.showerror("Missing output", "Please specify an output path."); return
        if not os.path.exists(inp):
            messagebox.showerror("File not found", f"Input not found:\n{inp}"); return
        if not os.path.exists(HUFFMAN_BIN):
            messagebox.showerror(
                "Binary not found",
                f"huffman binary not found:\n{HUFFMAN_BIN}\n\n"
                "Compile huffman.cpp and place it next to app.py."
            ); return

        cmd = [HUFFMAN_BIN, mode, inp, out]
        if mode == "compress" and self._verify_var.get():
            cmd.append("--verify")

        self._log_write(f"\n{'═'*58}\n")
        self._log_write(f"  {' '.join(cmd)}\n", color="cyan")
        self._log_write(f"{'═'*58}\n")

        self._set_running(True)
        threading.Thread(target=self._execute, args=(cmd,), daemon=True).start()

    def _execute(self, cmd):
        start = time.time()
        try:
            result = subprocess.run(
                cmd, capture_output=True, text=True,
                encoding="utf-8", errors="replace"
            )
            elapsed = time.time() - start
            if result.stdout:
                self.after(0, lambda s=result.stdout: self._log_write(s))
            if result.stderr:
                self.after(0, lambda s=result.stderr: self._log_write(s, color="red"))
            if result.returncode == 0:
                self.after(0, lambda: self._log_write(
                    f"\n  ✅ Done in {elapsed:.2f}s\n", color="green"))
                self.after(0, lambda: self._set_status(f"✅ Success ({elapsed:.2f}s)"))
            else:
                self.after(0, lambda: self._log_write(
                    f"\n  ❌ Exit code {result.returncode}\n", color="red"))
                self.after(0, lambda: self._set_status(f"❌ Failed (exit {result.returncode})"))
        except Exception as e:
            self.after(0, lambda: self._log_write(f"\n  Exception: {e}\n", color="red"))
            self.after(0, lambda: self._set_status("❌ Exception"))
        finally:
            self.after(0, lambda: self._set_running(False))

    # ── Helpers ───────────────────────────────────────────────
    def _set_running(self, state: bool):
        self._running = state
        if state:
            self._progress.start()
            self._run_btn.configure(state="disabled", text="⏳ Running…")
            self._set_status("Running…")
        else:
            self._progress.stop()
            self._progress.set(0)
            mode = self._mode.get().capitalize()
            self._run_btn.configure(state="normal", text=f"▶  Run {mode}")

    def _log_write(self, text: str, color: str = ""):
        self._log.configure(state="normal")
        tag = color or "default"
        self._log.insert("end", text, tag)
        self._log.tag_config("green",   foreground="#69f0ae")
        self._log.tag_config("red",     foreground="#ff5252")
        self._log.tag_config("cyan",    foreground="#40c4ff")
        self._log.tag_config("default", foreground="#e0e0e0")
        self._log.see("end")
        self._log.configure(state="disabled")

    def _clear_log(self):
        self._log.configure(state="normal")
        self._log.delete("1.0", "end")
        self._log.configure(state="disabled")

    def _set_status(self, msg: str):
        self._status_label.configure(text=msg)


# ════════════════════════════════════════════════════════════════
if __name__ == "__main__":
    app = HuffmanApp()
    app.mainloop()