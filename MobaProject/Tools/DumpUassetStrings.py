from pathlib import Path

def ascii_strings(b, n=5):
    out = []
    cur = bytearray()
    for x in b:
        if 32 <= x < 127:
            cur.append(x)
        else:
            if len(cur) >= n:
                out.append(cur.decode("ascii"))
            cur = bytearray()
    if len(cur) >= n:
        out.append(cur.decode("ascii"))
    return out

root = Path(r"C:\Users\Thedo\Documents\MobaRepo\MobaProject\Content\Moba")
for name in ["BP_Mage.uasset", "BP_MobaBaseChar.uasset", "BP_Brawler.uasset"]:
    p = root / name
    print("====", name, p.stat().st_size if p.exists() else "MISSING")
    if not p.exists():
        continue
    for s in ascii_strings(p.read_bytes(), 5):
        keys = (
            "/Game/", "SK_", "SM_", "ABP_", "Manny", "Gideon", "Mesh", "Hat",
            "Cap", "Wizard", "Anim", "Scale", "Socket", "Skeletal", "StaticMesh",
            "Mannequin", "Relative", "Attach",
        )
        if any(k in s for k in keys):
            print(" ", s)
    print()
