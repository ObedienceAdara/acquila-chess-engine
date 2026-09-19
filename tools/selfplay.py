#!/usr/bin/env python3
"""Generate a provenance-preserving UCI self-play dataset."""
import argparse, csv, subprocess


def command(proc, s):
    proc.stdin.write(s + "\n")
    proc.stdin.flush()


def wait_prefix(proc, prefix):
    while True:
        line=proc.stdout.readline()
        if not line:
            raise RuntimeError("engine exited unexpectedly")
        line=line.strip()
        if line.startswith(prefix):
            return line


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("engine")
    ap.add_argument("--games",type=int,default=10)
    ap.add_argument("--plies",type=int,default=120)
    ap.add_argument("--depth",type=int,default=8)
    ap.add_argument("--out",default="dataset.csv")
    args=ap.parse_args()

    with open(args.out,"w",newline="") as f:
        w=csv.writer(f)
        w.writerow(["game","ply","fen","score_cp","bestmove","depth","engine"])
        for game in range(args.games):
            p=subprocess.Popen([args.engine],stdin=subprocess.PIPE,stdout=subprocess.PIPE,
                               stderr=subprocess.DEVNULL,text=True,bufsize=1)
            command(p,"uci");wait_prefix(p,"uciok")
            command(p,"isready");wait_prefix(p,"readyok")
            moves=[]
            for ply in range(args.plies):
                position="position startpos"+(" moves "+" ".join(moves) if moves else "")
                command(p,position);command(p,"fen")
                fen=p.stdout.readline().strip()
                if not fen or " " not in fen:
                    raise RuntimeError(f"unexpected FEN response: {fen!r}")
                command(p,f"go depth {args.depth}")
                score="";best=None
                while True:
                    line=p.stdout.readline()
                    if not line: raise RuntimeError("engine exited during search")
                    line=line.strip()
                    if line.startswith("info ") and " score cp " in line:
                        parts=line.split();i=parts.index("cp");score=parts[i+1]
                    elif line.startswith("bestmove "):
                        best=line.split()[1];break
                w.writerow([game,ply,fen,score,best,args.depth,args.engine])
                moves.append(best)
            command(p,"quit");p.wait(timeout=5)


if __name__=="__main__":
    main()
