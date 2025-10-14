# checker.py
import sys
# uso: python3 checker.py saida.txt entrada.txt

S = open(sys.argv[1]).read().splitlines()[0]
with open(sys.argv[2]) as f:
    n = int(f.readline())
    arr = [f.readline().rstrip('\n') for _ in range(n)]

ok = True
for s in arr:
    if s not in S:
        print("FALHOU:", s, "não está contida na saída")
        ok = False

if ok:
    print("OK: todas as strings estão contidas na saída.")
    print("len(saída) =", len(S))
