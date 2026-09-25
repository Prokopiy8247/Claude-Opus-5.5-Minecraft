"""Fast compile check for MCR assemblies using Unity's bundled Roslyn (no editor launch).

Usage: python Tools/fastcompile.py [runtime|editor] [maxErrors] [--extra DIR]... [--exclude FILE]... [--only SUBSTR]...
  --extra DIR     also compile every .cs file under DIR (staging folders outside Assets)
  --exclude FILE  leave a file out (path relative to the project root, forward slashes)
  --only SUBSTR   only print errors whose path contains SUBSTR (the error count still covers everything)
Reuses the reference list from Unity's last generated Assembly-CSharp.rsp.
"""
import glob, os, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UNITY = "C:/Program Files/Unity/Hub/Editor/6000.6.0f1/Editor/Data"
DOTNET = UNITY + "/DotNetSdk/dotnet.exe"
CSC = UNITY + "/DotNetSdk/sdk/8.0.318/Roslyn/bincore/csc.dll"


def norm(p):
    return os.path.normpath(os.path.abspath(p)).replace("\\", "/").lower()


def main():
    args = sys.argv[1:]
    extra, exclude, only, pos = [], [], [], []
    i = 0
    while i < len(args):
        a = args[i]
        if a in ("--extra", "--exclude", "--only") and i + 1 < len(args):
            {"--extra": extra, "--exclude": exclude, "--only": only}[a].append(args[i + 1])
            i += 2
            continue
        pos.append(a)
        i += 1
    which = pos[0] if len(pos) > 0 else "runtime"
    maxerr = int(pos[1]) if len(pos) > 1 else 60
    base = os.path.join(ROOT, "Library/Bee/artifacts/1900b0aE.dag/Assembly-CSharp-Editor.rsp" if which == "editor" else "Library/Bee/artifacts/1900b0aE.dag/Assembly-CSharp.rsp")
    lines = open(base, encoding="utf-8").read().splitlines()
    out = []
    for l in lines:
        if l.startswith("-r:"):
            p = l[3:].strip('"')
            if "Assembly-CSharp" in p:
                continue
            if which == "runtime" and ("UnityEditor" in p or "Editor" in os.path.basename(p) or "nunit" in p or "Cecil" in p):
                continue
            out.append(l)
        elif l.startswith("-define:") or l.startswith("/nowarn") or l.startswith("-langversion") or l in ("/nologo", "/utf8output", "/deterministic"):
            out.append(l)
        elif l.startswith("/RuntimeMetadataVersion"):
            out.append(l)
    tag = "_" + str(os.getpid())
    outdir = os.path.join(ROOT, "Tools/_out")
    os.makedirs(outdir, exist_ok=True)
    if which == "editor":
        out.append('-r:"%s"' % os.path.join(outdir, "MCR.Runtime.dll").replace("\\", "/"))
        srcs = glob.glob(os.path.join(ROOT, "Assets/_Game/Editor/**/*.cs"), recursive=True)
        name = "MCR.Editor"
    else:
        srcs = glob.glob(os.path.join(ROOT, "Assets/_Game/Scripts/**/*.cs"), recursive=True)
        name = "MCR.Runtime"
    for d in extra:
        dd = d if os.path.isabs(d) else os.path.join(ROOT, d)
        srcs += glob.glob(os.path.join(dd, "**/*.cs"), recursive=True)
    ex = set(norm(e if os.path.isabs(e) else os.path.join(ROOT, e)) for e in exclude)
    srcs = [s for s in srcs if norm(s) not in ex]
    # private output name when staging so parallel checks do not fight over the dll
    dllname = name + (tag if (extra or exclude) else "") + ".dll"
    out.insert(0, "-target:library")
    out.insert(1, '-out:"%s"' % os.path.join(outdir, dllname).replace("\\", "/"))
    out.append("/nowarn:0414,0219,0168,0162,0067,0108,0114,0618")
    out.append("/warn:1")
    out.append("/preferreduilang:en-US")
    for s in srcs:
        out.append('"%s"' % s.replace("\\", "/"))
    rsp = os.path.join(outdir, name + (tag if (extra or exclude) else "") + ".rsp")
    open(rsp, "w", encoding="utf-8").write("\n".join(out))
    r = subprocess.run([DOTNET, "exec", CSC, "/noconfig", "@" + rsp], capture_output=True, text=True, encoding="utf-8", errors="replace", cwd=ROOT)
    msgs = [m for m in r.stdout.splitlines() if ": error " in m]
    warns = [m for m in r.stdout.splitlines() if ": warning " in m]
    seen = set(); uniq = []
    for m in msgs:
        m2 = m.replace(ROOT.replace("\\", "/") + "/", "").replace(ROOT + "\\", "")
        if m2 in seen:
            continue
        seen.add(m2); uniq.append(m2)
    shown = [m for m in uniq if not only or any(o.replace("/", "\\").lower() in m.lower() or o.lower() in m.lower() for o in only)]
    print("%s: %d errors (%d shown), %d warnings" % (name, len(uniq), len(shown), len(warns)))
    for m in shown[:maxerr]:
        print(m)
    if not uniq and r.returncode != 0:
        print(r.stdout[-4000:]); print(r.stderr[-4000:])
    if extra or exclude:
        for f in (os.path.join(outdir, dllname), rsp, os.path.join(outdir, dllname[:-4] + ".pdb")):
            try:
                os.remove(f)
            except OSError:
                pass
    return 0 if not uniq else 1


if __name__ == "__main__":
    sys.exit(main())
