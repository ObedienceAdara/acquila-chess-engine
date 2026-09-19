#!/usr/bin/env python3
import queue
import re
import subprocess
import sys
import threading
import time


class UCISession:
    def __init__(self, engine):
        self.proc = subprocess.Popen(
            [engine],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
        )
        self.lines = queue.Queue()
        self.all_lines = []
        self.reader = threading.Thread(target=self._read_stdout, daemon=True)
        self.reader.start()

    def _read_stdout(self):
        for line in self.proc.stdout:
            clean = line.rstrip("\n")
            self.all_lines.append(clean)
            self.lines.put(clean)

    def send(self, command):
        self.proc.stdin.write(command + "\n")
        self.proc.stdin.flush()

    def wait_for(self, predicate, timeout=3.0):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                line = self.lines.get(timeout=0.05)
            except queue.Empty:
                continue
            if predicate(line):
                return line
        raise AssertionError("timed out waiting for UCI output")

    def close(self):
        try:
            self.send("quit")
        except (BrokenPipeError, OSError):
            pass
        try:
            self.proc.wait(timeout=2.0)
        except subprocess.TimeoutExpired:
            self.proc.kill()
            self.proc.wait(timeout=2.0)


def main(engine):
    session = UCISession(engine)
    try:
        session.send("uci")
        session.wait_for(lambda line: line == "uciok")

        session.send("isready")
        session.wait_for(lambda line: line == "readyok")

        mate_fen = "7k/5Q2/6K1/8/8/8/8/8 w - - 0 1"
        session.send("position fen " + mate_fen)
        session.send("go depth 2")
        session.wait_for(lambda line: line.startswith("bestmove "))

        info_lines = [
            line for line in session.all_lines
            if line.startswith("info depth ")
        ]
        mate_infos = [line for line in info_lines if "score mate " in line]
        assert mate_infos, "no UCI score mate line was emitted"
        assert any("score mate 1" in line for line in mate_infos), mate_infos
        assert not any(
            re.search(r"score cp -?29\d+", line)
            for line in info_lines
        ), info_lines

        session.send("position startpos")
        session.send("go movetime 2000")
        time.sleep(0.05)
        stop_sent = time.monotonic()
        session.send("stop")
        session.wait_for(lambda line: line.startswith("bestmove "), timeout=1.0)
        stop_elapsed = time.monotonic() - stop_sent
        assert stop_elapsed < 1.0, f"stop response too slow: {stop_elapsed:.3f}s"

    finally:
        session.close()
        stderr = session.proc.stderr.read()
        assert session.proc.returncode == 0, (
            f"engine exited with code {session.proc.returncode}; stderr={stderr!r}"
        )


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: uci_regression.py PATH_TO_ENGINE")
    main(sys.argv[1])
    print("UCI regression tests: PASS")
