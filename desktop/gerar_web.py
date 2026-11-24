"""Gera a cópia da interface embutida no Estúdio Desktop com acentos em códigos ASCII.
HTML: &#NNN;  ·  JavaScript: \\uXXXX  ·  CSS: \\XXXXXX
Assim nenhum caminho entre o app e o navegador interno consegue corromper o texto."""
import re, sys, pathlib
raiz = pathlib.Path(__file__).resolve().parent.parent
saida = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path(__file__).parent / 'web'
saida.mkdir(parents=True, exist_ok=True)

def js(t):  return ''.join(c if ord(c) < 128 else ''.join('\\u%04x' % u for u in (c.encode('utf-16-be') and [int.from_bytes(c.encode('utf-16-be')[i:i+2], 'big') for i in range(0, len(c.encode('utf-16-be')), 2)])) for c in t)
def css(t): return ''.join(c if ord(c) < 128 else '\\%06x' % ord(c) for c in t)
def html(t): return ''.join(c if ord(c) < 128 else '&#%d;' % ord(c) for c in t)

fonte = (raiz / 'index.html').read_text(encoding='utf-8')
partes, pos = [], 0
for m in re.finditer(r'(<script\b[^>]*>)(.*?)(</script>)|(<style\b[^>]*>)(.*?)(</style>)', fonte, re.S):
    partes.append(html(fonte[pos:m.start()]))
    if m.group(1): partes.append(m.group(1) + js(m.group(2)) + m.group(3))
    else:          partes.append(m.group(4) + css(m.group(5)) + m.group(6))
    pos = m.end()
partes.append(html(fonte[pos:]))
texto = ''.join(partes)
assert all(ord(c) < 128 for c in texto)
(saida / 'index.html').write_text(texto, encoding='ascii')
print('interface gerada:', saida / 'index.html', len(texto), 'bytes')
